// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_result.h"

#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/string_matching/tokenized_string_match.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace app_list {

namespace {

using chromeos::string_matching::TokenizedString;
using chromeos::string_matching::TokenizedStringMatch;

}  // namespace

// TODO(crbug.com/1290682): Complete implementation.
KeyboardShortcutResult::KeyboardShortcutResult(Profile* profile,
                                               const KeyboardShortcutData& data,
                                               double relevance)
    : profile_(profile), description_(data.description) {
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
