// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/ui/l10n_util.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/atl.h"
#include "base/win/embedded_i18n/language_selector.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"

namespace updater {
namespace {

constexpr base::win::i18n::LanguageSelector::LangToOffset
    kLanguageOffsetPairs[] = {
#define HANDLE_LANGUAGE(l_, o_) {L## #l_, o_},
        DO_LANGUAGES
#undef HANDLE_LANGUAGE
};

std::wstring GetPreferredLanguageFromGoogleUpdate() {
  const TagParsingResult tag_parsing_result = GetTagArgs();
  if (tag_parsing_result.error != tagging::ErrorCode::kSuccess) {
    VLOG(0) << "Couldn't parse the tag!";
    return {};
  }
  const tagging::TagArgs tag_args =
      tag_parsing_result.tag_args.value_or(tagging::TagArgs());

  return tag_args.language.empty() ? L"en-us"
                                   : base::SysUTF8ToWide(tag_args.language);
}

const base::win::i18n::LanguageSelector& GetLanguageSelector() {
  static base::NoDestructor<base::win::i18n::LanguageSelector> instance(
      GetPreferredLanguageFromGoogleUpdate(), kLanguageOffsetPairs);
  return *instance;
}

}  // namespace

std::wstring GetLocalizedString(int base_message_id) {
  // Map `base_message_id` to the base id for the current install mode.
  std::wstring localized_string;
  int message_id = base_message_id + GetLanguageSelector().offset();
  const ATLSTRINGRESOURCEIMAGE* image =
      AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(), message_id);
  if (image) {
    localized_string = std::wstring(image->achString, image->nLength);
  } else {
    NOTREACHED() << "Unable to find resource id " << message_id;
  }
  return localized_string;
}

std::wstring GetLocalizedStringF(int base_message_id,
                                 const std::wstring& replacement) {
  return GetLocalizedStringF(base_message_id,
                             std::vector<std::wstring>{replacement});
}

std::wstring GetLocalizedStringF(int base_message_id,
                                 std::vector<std::wstring> replacements) {
  // Replacements start at index 1 because the implementation of
  // ReplaceStringPlaceholders does i+1, so the first placeholder would be `$1`.
  // A `$0` is considered an invalid placeholder.
  replacements.insert(replacements.begin(), {});
  return base::ReplaceStringPlaceholders(GetLocalizedString(base_message_id),
                                         replacements, nullptr);
}

}  // namespace updater
