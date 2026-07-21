// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/infobars/browser_infobar_registry.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/ui/page_info/chrome_page_info_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"

namespace infobars {

void RegisterInfoBars() {

  if (IsInfoBarMigrated(InfoBarDelegate::COLLECTED_COOKIES_INFOBAR_DELEGATE)) {
      auto* browser_infobar_manager =
      BrowserInfoBarManager::From(g_browser_process);
    CHECK(browser_infobar_manager);
    auto spec =
        InfoBarSpec::Builder(
            InfoBarDelegate::COLLECTED_COOKIES_INFOBAR_DELEGATE)
            .SetMessageText(l10n_util::GetStringUTF16(
                IDS_COLLECTED_COOKIES_INFOBAR_MESSAGE))
            .SetIcon(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSettingsIcon
                         : vector_icons::kSettingsChromeRefreshOldIcon)
            .SetScope(InfoBarScope::kTab)
            .AddOkButton(
                l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_INFOBAR_BUTTON),
                base::BindRepeating([](content::WebContents* web_contents) {
                  if (web_contents) {
                    web_contents->GetController().Reload(
                        content::ReloadType::NORMAL, true);
                  }
                }))
            .Build();
    browser_infobar_manager->Register(std::move(spec));
  }

  if (IsInfoBarMigrated(InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE)) {
      auto* browser_infobar_manager =
      BrowserInfoBarManager::From(g_browser_process);
    if (browser_infobar_manager) {
      ChromePageInfoDelegate::RegisterPageInfoInfoBar(browser_infobar_manager);
    }
  }
}

}  // namespace infobars
