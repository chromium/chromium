// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/l10n_util.h"

#include <string>
#include <vector>

#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/atl.h"
#include "base/win/embedded_i18n/language_selector.h"
#include "base/win/i18n.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/installer/exit_code.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"

namespace updater {
namespace {

constexpr base::win::i18n::LanguageSelector::LangToOffset
    kLanguageOffsetPairs[] = {
#define HANDLE_LANGUAGE(l_, o_) {L## #l_, o_},
        DO_LANGUAGES
#undef HANDLE_LANGUAGE
};

size_t GetLanguageOffset(const std::wstring& lang) {
  return base::win::i18n::LanguageSelector(lang, kLanguageOffsetPairs).offset();
}

}  // namespace

std::wstring GetPreferredLanguage() {
  std::vector<std::wstring> languages;
  if (!base::win::i18n::GetUserPreferredUILanguageList(&languages) ||
      languages.size() == 0) {
    return L"en-us";
  }

  return languages[0];
}

std::wstring GetLocalizedString(unsigned int base_message_id,
                                const std::wstring& lang) {
  ::SetLastError(ERROR_SUCCESS);

  // Map `base_message_id` to the base id for the current install mode.
  const unsigned int message_id =
      static_cast<UINT>(base_message_id + GetLanguageOffset(lang));
  const ATLSTRINGRESOURCEIMAGE* image =
      AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(), message_id);
  if (image) {
    return std::wstring(image->achString, image->nLength);
  }
  const DWORD error_code = ::GetLastError();
  base::debug::Alias(&base_message_id);
  base::debug::Alias(&message_id);
  base::debug::Alias(&error_code);
  DEBUG_ALIAS_FOR_CSTR(dbg_lang, base::WideToUTF8(lang).c_str(), 16);
  VLOG(2) << base_message_id << ", " << message_id << ", " << error_code << ", "
          << lang;
  SCOPED_CRASH_KEY_NUMBER("l10_util", "base_message_id", base_message_id);
  SCOPED_CRASH_KEY_NUMBER("l10_util", "message_id", message_id);
  SCOPED_CRASH_KEY_NUMBER("l10_util", "error_code", error_code);
  SCOPED_CRASH_KEY_STRING32("l10_util", "lang", dbg_lang);
  NOTREACHED_IN_MIGRATION();
  return std::wstring();
}

std::wstring GetLocalizedStringF(unsigned int base_message_id,
                                 const std::wstring& replacement,
                                 const std::wstring& lang) {
  return GetLocalizedStringF(base_message_id,
                             std::vector<std::wstring>{replacement}, lang);
}

std::wstring GetLocalizedStringF(unsigned int base_message_id,
                                 std::vector<std::wstring> replacements,
                                 const std::wstring& lang) {
  // Replacements start at index 1 because the implementation of
  // ReplaceStringPlaceholders does i+1, so the first placeholder would be `$1`.
  // A `$0` is considered an invalid placeholder.
  replacements.insert(replacements.begin(), {});
  return base::ReplaceStringPlaceholders(
      GetLocalizedString(base_message_id, lang), replacements, nullptr);
}

std::wstring GetLocalizedMetainstallerErrorString(DWORD exit_code,
                                                  DWORD windows_error) {
#define METAINSTALLER_ERROR_SWITCH_ENTRY(exit_code)                        \
  case static_cast<int>(exit_code):                                        \
    return GetLocalizedStringF(                                            \
        IDS_GENERIC_METAINSTALLER_ERROR_BASE,                              \
        {L#exit_code, windows_error ? GetTextForSystemError(windows_error) \
                                    : std::wstring()})

  switch (exit_code) {
    METAINSTALLER_ERROR_SWITCH_ENTRY(TEMP_DIR_FAILED);
    METAINSTALLER_ERROR_SWITCH_ENTRY(UNPACKING_FAILED);
    METAINSTALLER_ERROR_SWITCH_ENTRY(GENERIC_INITIALIZATION_FAILURE);
    METAINSTALLER_ERROR_SWITCH_ENTRY(COMMAND_STRING_OVERFLOW);
    METAINSTALLER_ERROR_SWITCH_ENTRY(WAIT_FOR_PROCESS_FAILED);
    METAINSTALLER_ERROR_SWITCH_ENTRY(PATH_STRING_OVERFLOW);
    METAINSTALLER_ERROR_SWITCH_ENTRY(UNABLE_TO_GET_WORK_DIRECTORY);
    METAINSTALLER_ERROR_SWITCH_ENTRY(UNABLE_TO_EXTRACT_ARCHIVE);
    METAINSTALLER_ERROR_SWITCH_ENTRY(UNEXPECTED_ELEVATION_LOOP);
    METAINSTALLER_ERROR_SWITCH_ENTRY(UNEXPECTED_DE_ELEVATION_LOOP);
    METAINSTALLER_ERROR_SWITCH_ENTRY(UNEXPECTED_ELEVATION_LOOP_SILENT);
    METAINSTALLER_ERROR_SWITCH_ENTRY(UNABLE_TO_SET_DIRECTORY_ACL);
    METAINSTALLER_ERROR_SWITCH_ENTRY(INVALID_OPTION);
    METAINSTALLER_ERROR_SWITCH_ENTRY(FAILED_TO_DE_ELEVATE_METAINSTALLER);
    METAINSTALLER_ERROR_SWITCH_ENTRY(RUN_SETUP_FAILED_FILE_NOT_FOUND);
    METAINSTALLER_ERROR_SWITCH_ENTRY(RUN_SETUP_FAILED_PATH_NOT_FOUND);
    METAINSTALLER_ERROR_SWITCH_ENTRY(RUN_SETUP_FAILED_COULD_NOT_CREATE_PROCESS);
    METAINSTALLER_ERROR_SWITCH_ENTRY(UNABLE_TO_GET_EXE_PATH);

    case UNSUPPORTED_WINDOWS_VERSION:
      return GetLocalizedString(IDS_UPDATER_OS_NOT_SUPPORTED_BASE);
    case FAILED_TO_ELEVATE_METAINSTALLER:
      return GetLocalizedStringF(IDS_FAILED_TO_ELEVATE_METAINSTALLER_BASE,
                                 GetTextForSystemError(windows_error));
    case UPDATER_EXIT_CODE:
    default:
      NOTREACHED_IN_MIGRATION();
      return {};
  }
#undef METAINSTALLER_ERROR_SWITCH_ENTRY
}

std::wstring GetLocalizedSplashScreenString() {
  return GetLocalizedString(IDS_INITIALIZING_BASE);
}

}  // namespace updater
