// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for fetching localized resources.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/installer/util/l10n_string_util.h"

#include <windows.h>

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/buffer_iterator.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/embedded_i18n/language_selector.h"
#include "base/win/shlwapi.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/installer_util_strings.h"

namespace {

constexpr base::win::i18n::LanguageSelector::LangToOffset
    kLanguageOffsetPairs[] = {
#define HANDLE_LANGUAGE(l_, o_) {L## #l_, o_},
        DO_LANGUAGES
#undef HANDLE_LANGUAGE
};

// Returns the language under which Chrome was downloaded, or an empty string if
// no such language is specified.
std::wstring GetPreferredLanguageFromGoogleUpdate() {
  std::wstring language;
  GoogleUpdateSettings::GetLanguage(&language);
  return language;
}

const base::win::i18n::LanguageSelector& GetLanguageSelector() {
  static base::NoDestructor<base::win::i18n::LanguageSelector> instance(
      GetPreferredLanguageFromGoogleUpdate(), kLanguageOffsetPairs);
  return *instance;
}

installer::TranslationDelegate* g_translation_delegate = nullptr;

}  // namespace

namespace installer {

TranslationDelegate::~TranslationDelegate() {}

void SetTranslationDelegate(TranslationDelegate* delegate) {
  g_translation_delegate = delegate;
}

std::wstring GetLocalizedString(int base_message_id) {
  // Map |base_message_id| to the base id for the current install mode.
  base_message_id = GetBaseMessageIdForMode(base_message_id);

  if (g_translation_delegate)
    return g_translation_delegate->GetLocalizedString(base_message_id);

  const auto& language_selector = GetLanguageSelector();
  const auto language_offset = language_selector.offset();
  const UINT message_id = base::checked_cast<UINT>(
      base::CheckAdd(base_message_id, language_offset).ValueOrDie());

  // Strings are bundled up in batches of 16; one resource per bundle; see
  // https://devblogs.microsoft.com/oldnewthing/20040130-00/?p=40813. Find the
  // bundle containing the desired string.
  HGLOBAL bundle_data_handle = nullptr;
  const uint8_t* bundle_data = nullptr;
  DWORD bundle_size = 0;
  const uint16_t bundle_id = (message_id >> 4) + 1;
  auto* const bundle_handle =
      ::FindResourceW(CURRENT_MODULE(), MAKEINTRESOURCEW(bundle_id), RT_STRING);
  if (bundle_handle != nullptr) {
    bundle_data_handle = ::LoadResource(CURRENT_MODULE(), bundle_handle);
    if (bundle_data_handle != nullptr) {
      bundle_data =
          reinterpret_cast<const uint8_t*>(::LockResource(bundle_data_handle));
      if (bundle_data != nullptr) {
        // The bundle is a sequence of ATLSTRINGRESOURCEIMAGE structures, which
        // are each a DWORD length followed by that many wide characters.
        bundle_size = ::SizeofResource(CURRENT_MODULE(), bundle_handle);
        base::BufferIterator<const uint8_t> iterator(bundle_data, bundle_size);
        // Scan forward in the bundle past all preceding messages.
        for (int index = message_id & 0xF; index; --index) {
          if (const auto* length = iterator.Object<const WORD>(); length) {
            iterator.Span<wchar_t>(*length);
          }
        }
        // Return a copy of the string.
        if (const auto* length = iterator.Object<const WORD>(); length) {
          if (*length == 0) {
            return std::wstring();
          }
          if (auto string = iterator.Span<wchar_t>(*length); !string.empty()) {
            return std::wstring(string.data(), *length);
          }
        }
      }
    }
  }

  // Debugging aid for https://crbug.com/1478933.
  auto last_error = ::GetLastError();
  base::debug::Alias(&last_error);
  base::debug::Alias(&base_message_id);
  base::debug::Alias(&language_offset);
  base::debug::Alias(&message_id);
  base::debug::Alias(&bundle_handle);
  base::debug::Alias(&bundle_data_handle);
  base::debug::Alias(&bundle_data);
  base::debug::Alias(&bundle_size);
  DEBUG_ALIAS_FOR_WCHARCSTR(selected_translation,
                            language_selector.selected_translation().c_str(),
                            16);
  NOTREACHED_IN_MIGRATION() << "Unable to find resource id " << message_id;

  return std::wstring();
}

std::wstring GetLocalizedStringF(int base_message_id,
                                 std::vector<std::wstring> replacements) {
  // Replacements start at index 1, corresponding to placeholder `$1`.
  replacements.insert(replacements.begin(), {});
  return base::ReplaceStringPlaceholders(GetLocalizedString(base_message_id),
                                         replacements, /*offsets=*/{});
}

// Here we generate the url spec with the Microsoft res:// scheme which is
// explained here : http://support.microsoft.com/kb/220830
std::wstring GetLocalizedEulaResource() {
  wchar_t full_exe_path[MAX_PATH];
  int len = ::GetModuleFileName(nullptr, full_exe_path, MAX_PATH);
  if (len == 0 || len == MAX_PATH)
    return L"";

  // The resource names are more or less the upcased language names.
  std::wstring language(GetLanguageSelector().selected_translation());
  std::replace(language.begin(), language.end(), L'-', L'_');
  language = base::ToUpperASCII(language);

  std::wstring resource(L"IDR_OEMPG_");
  resource.append(language).append(L".HTML");

  // Fall back on "en" if we don't have a resource for this language.
  if (nullptr == FindResource(nullptr, resource.c_str(), RT_HTML))
    resource = L"IDR_OEMPG_EN.HTML";

  // Spaces and DOS paths must be url encoded.
  std::wstring url_path =
      base::StrCat({L"res://", full_exe_path, L"/#23/", resource});

  // The cast is safe because url_path has limited length
  // (see the definition of full_exe_path and resource).
  DCHECK(std::numeric_limits<uint32_t>::max() > (url_path.size() * 3));
  DWORD count = static_cast<DWORD>(url_path.size() * 3);
  auto url_canon = base::HeapArray<wchar_t>::WithSize(count);
  HRESULT hr = ::UrlCanonicalizeW(url_path.c_str(), url_canon.data(), &count,
                                  URL_ESCAPE_UNSAFE);
  if (SUCCEEDED(hr))
    return std::wstring(url_canon.data());
  return url_path;
}

std::wstring GetCurrentTranslation() {
  return GetLanguageSelector().selected_translation();
}

int GetBaseMessageIdForMode(int base_message_id) {
// Generate the constants holding the mode-specific resource ID arrays.
#define HANDLE_MODE_STRING(id, ...)                                   \
  static constexpr int k##id##Strings[] = {__VA_ARGS__};              \
  static_assert(                                                      \
      std::size(k##id##Strings) == install_static::NUM_INSTALL_MODES, \
      "resource " #id                                                 \
      " has the wrong number of mode-specific "                       \
      "strings.");
  DO_MODE_STRINGS
#undef HANDLE_MODE_STRING

  const int* mode_strings = nullptr;
  switch (base_message_id) {
// Generate the cases mapping each mode-specific resource ID to its array.
#define HANDLE_MODE_STRING(id, ...)    \
  case id:                             \
    mode_strings = &k##id##Strings[0]; \
    break;
    DO_MODE_STRINGS
#undef HANDLE_MODE_STRING
    default:
      // This ID has no per-mode variants.
      return base_message_id;
  }

  // Return the variant of |base_message_id| for the current mode.
  return mode_strings[install_static::InstallDetails::Get()
                          .install_mode_index()];
}

}  // namespace installer
