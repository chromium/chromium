// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_result.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace app_list {

namespace {

using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;

}  // namespace

// TODO(crbug.com/1290682): Complete implementation.
KeyboardShortcutResult::KeyboardShortcutResult(Profile* profile,
                                               const KeyboardShortcutData& data,
                                               double relevance)
    : profile_(profile) {
  set_relevance(relevance);
  SetTitle(data.description);
  SetResultType(ResultType::kKeyboardShortcut);
  SetMetricsType(ash::KEYBOARD_SHORTCUT);
  SetDisplayType(DisplayType::kList);
  SetCategory(Category::kHelp);

  // Set the details to the display name of the Keyboard Shortcut Viewer app.
  std::u16string sanitized_name = base::CollapseWhitespace(
      l10n_util::GetStringUTF16(IDS_INTERNAL_APP_KEYBOARD_SHORTCUT_VIEWER),
      true);
  base::i18n::SanitizeUserSuppliedString(&sanitized_name);
  SetDetails(sanitized_name);

  // Process |data.keyboard_shortcut_codes| to create:
  //   1. A vector of information for the KSV text.
  //      TODO(crbug.com/1290682): Decide on formatting, and implement.
  //   2. The accessible name.

  std::vector<std::u16string> replacement_strings;
  std::vector<std::u16string> accessible_names;
  const size_t shortcut_key_codes_size = data.shortcut_key_codes.size();
  replacement_strings.reserve(shortcut_key_codes_size);
  accessible_names.reserve(shortcut_key_codes_size);
  bool has_invalid_dom_key = false;

  for (ui::KeyboardCode key_code : data.shortcut_key_codes) {
    // Get the string for the |DomKey|.
    std::u16string dom_key_string =
        keyboard_shortcut_viewer::GetStringForKeyboardCode(key_code);

    // See ash/shortcut_viewer/views/keyboard_shortcut_item_view.cc for details
    // on why this is necessary.
    const bool dont_remap_position =
        data.description_message_id == IDS_KSV_DESCRIPTION_IDC_ZOOM_PLUS ||
        data.description_message_id == IDS_KSV_DESCRIPTION_IDC_ZOOM_MINUS;
    if (dont_remap_position) {
      dom_key_string = keyboard_shortcut_viewer::GetStringForKeyboardCode(
          key_code, /*remap_positional_key=*/false);
    }

    // If the |key_code| has no mapped |dom_key_string|, we use an alternative
    // string to indicate that the shortcut is not supported by current keyboard
    // layout.
    if (dom_key_string.empty()) {
      replacement_strings.clear();
      accessible_names.clear();
      has_invalid_dom_key = true;
      break;
    }

    std::u16string accessible_name =
        keyboard_shortcut_viewer::GetAccessibleNameForKeyboardCode(key_code);
    accessible_names.push_back(accessible_name.empty() ? dom_key_string
                                                       : accessible_name);
    replacement_strings.push_back(std::move(dom_key_string));
  }

  int shortcut_message_id;
  if (has_invalid_dom_key) {
    // |shortcut_message_id| should never be used if the shortcut is not
    // supported on the current keyboard layout.
    shortcut_message_id = -1;
  } else if (data.shortcut_message_id) {
    shortcut_message_id = *data.shortcut_message_id;
  } else {
    // Automatically determine the shortcut message based on the number of
    // replacement strings.
    // As there are separators inserted between the modifiers, a shortcut with
    // N modifiers has 2*N + 1 replacement strings.
    switch (replacement_strings.size()) {
      case 1:
        shortcut_message_id = IDS_KSV_SHORTCUT_ONE_KEY;
        break;
      case 3:
        shortcut_message_id = IDS_KSV_SHORTCUT_ONE_MODIFIER_ONE_KEY;
        break;
      case 5:
        shortcut_message_id = IDS_KSV_SHORTCUT_TWO_MODIFIERS_ONE_KEY;
        break;
      case 7:
        shortcut_message_id = IDS_KSV_SHORTCUT_THREE_MODIFIERS_ONE_KEY;
        break;
      default:
        NOTREACHED() << "Automatically determined shortcut has "
                     << replacement_strings.size() << " replacement strings.";
    }
  }

  std::u16string shortcut_string;
  std::u16string accessible_string;
  if (replacement_strings.empty()) {
    shortcut_string = l10n_util::GetStringUTF16(
        has_invalid_dom_key ? IDS_KSV_KEY_NO_MAPPING : shortcut_message_id);
    accessible_string = shortcut_string;
  } else {
    accessible_string = l10n_util::GetStringFUTF16(
        shortcut_message_id, accessible_names, /*offsets=*/nullptr);
  }

  SetAccessibleName(data.description + u", " + details() + u", " +
                    accessible_string);
}

KeyboardShortcutResult::~KeyboardShortcutResult() = default;

void KeyboardShortcutResult::Open(int event_flags) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->Launch(ash::kInternalAppIdKeyboardShortcutViewer, event_flags,
                apps::mojom::LaunchSource::kFromAppListQuery, nullptr);
}

double KeyboardShortcutResult::CalculateRelevance(
    const TokenizedString& query_tokenized,
    const std::u16string& target) {
  const TokenizedString target_tokenized(target, TokenizedString::Mode::kWords);

  const bool use_default_relevance =
      query_tokenized.text().empty() || target_tokenized.text().empty();

  if (use_default_relevance) {
    static constexpr double kDefaultRelevance = 0.0;
    return kDefaultRelevance;
  }

  TokenizedStringMatch match;
  match.Calculate(query_tokenized, target_tokenized);
  return match.relevance();
}

}  // namespace app_list
