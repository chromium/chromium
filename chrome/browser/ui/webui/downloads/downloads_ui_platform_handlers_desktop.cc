// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_warning_desktop_hats_utils.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/icon_loader.h"
#include "chrome/browser/icon_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui_platform_handlers.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/interaction/element_tracker_views.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/ui/browser_window.h"
#endif

namespace downloads {

namespace {

DownloadWarningHatsType ToHatsType(WarningSurveyType survey_type) {
  switch (survey_type) {
    case WarningSurveyType::kBypass:
      return DownloadWarningHatsType::kDownloadsPageBypass;
    case WarningSurveyType::kHeed:
      return DownloadWarningHatsType::kDownloadsPageHeed;
    case WarningSurveyType::kIgnore:
      return DownloadWarningHatsType::kDownloadsPageIgnore;
  }
}

}  // namespace

void NotifyDownloadOpened(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  if (auto* tab_interface =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(
            tab_interface->GetBrowserWindowInterface())) {
      views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
          kDownloadedFileOpenedCustomEventId, browser_view);
    }
  }
}

void DragDownload(const download::DownloadItem* item,
                  content::WebContents* web_contents) {
  CHECK(item);
  CHECK(web_contents);
  const display::Screen* const screen = display::Screen::Get();
  gfx::NativeView view = web_contents->GetNativeView();
  gfx::Image* icon = g_browser_process->icon_manager()->LookupIconFromFilepath(
      item->GetTargetFilePath(), IconLoader::NORMAL,
      screen->GetPreferredScaleFactorForView(view).value_or(1.0f));
  {
    // Enable nested tasks during DnD, while `DragDownloadItem()` blocks.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
    DragDownloadItem(item, icon, view);
  }
}

bool ShowEsbSettings(content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  if (!browser) {
    return false;
  }
  chrome::ShowSafeBrowsingEnhancedProtectionWithIph(
      browser,
      safe_browsing::SafeBrowsingSettingReferralMethod::kDownloadPageRowPromo);
  return true;
}

void PromptForScanningPassword(download::DownloadItem* item,
                               content::WebContents* web_contents) {
  CHECK(item);
  // ChromeOS does not have the download bubble and does not support local
  // password prompts for deep scans.
#if !BUILDFLAG(IS_CHROMEOS)
  if (!web_contents) {
    return;
  }
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }
  BrowserWindow::FromBrowser(browser)
      ->GetDownloadBubbleUIController()
      ->GetDownloadDisplayController()
      ->OpenSecuritySubpage(OfflineItemUtils::GetContentIdForDownload(item));
#endif
}

void StartDeepScan(download::DownloadItem* item) {
  CHECK(item);
  DownloadItemModel model(item);
  DownloadCommands commands(model.GetWeakPtr());
  commands.ExecuteCommand(DownloadCommands::DEEP_SCAN);
}

void BypassDeepScan(download::DownloadItem* item) {
  CHECK(item);
  DownloadItemModel model(item);
  DownloadCommands commands(model.GetWeakPtr());
  // The button says "Download suspicious file" which does not imply opening
  // the file.
  commands.ExecuteCommand(DownloadCommands::BYPASS_DEEP_SCANNING);
}

void MaybeTriggerDownloadWarningHatsSurvey(download::DownloadItem* item,
                                           Profile* profile,
                                           WarningSurveyType survey_type,
                                           int num_page_warnings) {
  CHECK(item);
  CHECK(profile);
  if (!CanShowDownloadWarningHatsSurvey(item)) {
    return;
  }

  auto psd = DownloadWarningHatsProductSpecificData::Create(
      ToHatsType(survey_type), item);
  psd.AddNumPageWarnings(num_page_warnings);

  MaybeLaunchDownloadWarningHatsSurvey(profile, psd);
}

void MaybeTriggerTrustSafetySurvey(
    download::DownloadItem* item,
    DownloadItemWarningData::WarningSurface surface,
    DownloadItemWarningData::WarningAction action) {
  CHECK(item);
  CHECK(surface == DownloadItemWarningData::WarningSurface::DOWNLOADS_PAGE ||
        surface == DownloadItemWarningData::WarningSurface::DOWNLOAD_PROMPT);
  CHECK(action == DownloadItemWarningData::WarningAction::PROCEED ||
        action == DownloadItemWarningData::WarningAction::DISCARD);
  if (Profile* profile = Profile::FromBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(item));
      profile &&
      safe_browsing::IsSafeBrowsingSurveysEnabled(*profile->GetPrefs())) {
    TrustSafetySentimentService* trust_safety_sentiment_service =
        TrustSafetySentimentServiceFactory::GetForProfile(profile);
    if (trust_safety_sentiment_service) {
      trust_safety_sentiment_service->InteractedWithDownloadWarningUI(surface,
                                                                      action);
    }
  }
}

}  // namespace downloads
