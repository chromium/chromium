// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/downloads/downloads_ui.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads_dom_handler.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/downloads_resources.h"
#include "chrome/grit/downloads_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_resources.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::DownloadManager;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateAndAddDownloadsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIDownloadsHost);
  webui::SetupWebUIDataSource(
      source, base::make_span(kDownloadsResources, kDownloadsResourcesSize),
      IDR_DOWNLOADS_DOWNLOADS_HTML);

  bool requests_ap_verdicts =
      safe_browsing::AdvancedProtectionStatusManagerFactory::GetForProfile(
          profile)
          ->IsUnderAdvancedProtection();
  source->AddBoolean("requestsApVerdicts", requests_ap_verdicts);

  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_DOWNLOAD_HISTORY_TITLE},
      {"searchResultsPlural", IDS_SEARCH_RESULTS_PLURAL},
      {"searchResultsSingular", IDS_SEARCH_RESULTS_SINGULAR},
      {"actionMenuDescription", IDS_DOWNLOAD_ACTION_MENU_DESCRIPTION},
      {"clearAll", IDS_DOWNLOAD_LINK_CLEAR_ALL},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"openDownloadsFolder", IDS_DOWNLOAD_LINK_OPEN_DOWNLOADS_FOLDER},
      {"moreActions", IDS_DOWNLOAD_MORE_ACTIONS},
      {"search", IDS_DOWNLOAD_HISTORY_SEARCH},
      {"inIncognito", IDS_DOWNLOAD_IN_INCOGNITO},

      // No results message that shows instead of the downloads list.
      {"noDownloads", IDS_DOWNLOAD_NO_DOWNLOADS},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},

      // Status.
      {"statusCancelled", IDS_DOWNLOAD_TAB_CANCELLED},
      {"statusRemoved", IDS_DOWNLOAD_FILE_REMOVED},

      // Dangerous file.
      {"dangerDiscard", IDS_DISCARD_DOWNLOAD},
      {"dangerReview", IDS_REVIEW_DOWNLOAD},

      // Deep scanning strings.
      {"deepScannedFailedDesc", IDS_DEEP_SCANNED_FAILED_DESCRIPTION},
      {"deepScannedOpenedDangerousDesc",
       IDS_DEEP_SCANNED_OPENED_DANGEROUS_DESCRIPTION},
      {"sensitiveContentWarningDesc",
       IDS_BLOCK_REASON_SENSITIVE_CONTENT_WARNING},
      {"sensitiveContentBlockedDesc",
       IDS_SENSITIVE_CONTENT_BLOCKED_DESCRIPTION},
      {"blockedTooLargeDesc", IDS_BLOCKED_TOO_LARGE_DESCRIPTION},
      {"blockedPasswordProtectedDesc",
       IDS_BLOCKED_PASSWORD_PROTECTED_DESCRIPTION},
      {"asyncScanningDownloadDesc", IDS_BLOCK_REASON_DEEP_SCANNING_UPDATED},
      {"asyncScanningDownloadDescSecond",
       IDS_BLOCK_REASON_DEEP_SCANNING_SECOND_UPDATED},
      {"promptForScanningDesc", IDS_BLOCK_REASON_PROMPT_FOR_SCANNING_UPDATED},
      {"promptForLocalPasswordScanningDesc",
       IDS_BLOCK_REASON_PROMPT_FOR_LOCAL_PASSWORD_SCANNING},
      {"controlDeepScan", IDS_DOWNLOAD_DEEP_SCAN_UPDATED},
      {"controlLocalPasswordScan", IDS_DOWNLOAD_LOCAL_PASSWORD_SCAN},

      // Controls.
      {"controlPause", IDS_DOWNLOAD_LINK_PAUSE},
      {"controlCancel", IDS_DOWNLOAD_LINK_CANCEL},
      {"controlResume", IDS_DOWNLOAD_LINK_RESUME},
      {"controlRetry", IDS_DOWNLOAD_LINK_RETRY},
      {"controlledByUrl", IDS_DOWNLOAD_BY_EXTENSION_URL},
      {"controlOpenNow", IDS_OPEN_DOWNLOAD_NOW},
      {"controlOpenAnyway", IDS_OPEN_DOWNLOAD_ANYWAY},
      {"toastClearedAll", IDS_DOWNLOAD_TOAST_CLEARED_ALL},
      {"toastDeletedFromHistoryStillOnDevice",
       IDS_DOWNLOADS_TOAST_DELETED_FROM_HISTORY_STILL_ON_DEVICE},
      {"toastDeletedFromHistory", IDS_DOWNLOADS_TOAST_DELETED_FROM_HISTORY},
      {"toastCopiedDownloadLink", IDS_DOWNLOADS_TOAST_COPIED_DOWNLOAD_LINK},
      {"undo", IDS_DOWNLOAD_UNDO},
      {"controlKeepDangerous", IDS_DOWNLOAD_KEEP_DANGEROUS_FILE},
      {"controlKeepSuspicious", IDS_DOWNLOAD_KEEP_SUSPICIOUS_FILE},
      {"controlKeepUnverified", IDS_DOWNLOAD_KEEP_UNVERIFIED_FILE},
      {"controlKeepInsecure", IDS_DOWNLOAD_KEEP_INSECURE_FILE},
      {"controlDeleteFromHistory", IDS_DOWNLOAD_DELETE_FROM_HISTORY},
      {"controlCopyDownloadLink", IDS_DOWNLOAD_COPY_DOWNLOAD_LINK},

      // Accessible labels for file icons.
      {"accessibleLabelDangerous",
       IDS_DOWNLOAD_DANGEROUS_ICON_ACCESSIBLE_LABEL},
      {"accessibleLabelSuspicious",
       IDS_DOWNLOAD_SUSPICIOUS_ICON_ACCESSIBLE_LABEL},
      {"accessibleLabelInsecure", IDS_DOWNLOAD_INSECURE_ICON_ACCESSIBLE_LABEL},
      {"accessibleLabelUnverified",
       IDS_DOWNLOAD_UNVERIFIED_ICON_ACCESSIBLE_LABEL},

      // Screenreader announcements.
      {"screenreaderSavedDangerous", IDS_DOWNLOAD_SCREENREADER_SAVED_DANGEROUS},
      {"screenreaderSavedSuspicious",
       IDS_DOWNLOAD_SCREENREADER_SAVED_SUSPICIOUS},
      {"screenreaderSavedInsecure", IDS_DOWNLOAD_SCREENREADER_SAVED_INSECURE},
      {"screenreaderSavedUnverified",
       IDS_DOWNLOAD_SCREENREADER_SAVED_UNVERIFIED},
      {"screenreaderPaused", IDS_DOWNLOAD_SCREENREADER_PAUSED},
      {"screenreaderResumed", IDS_DOWNLOAD_SCREENREADER_RESUMED},
      {"screenreaderCanceled", IDS_DOWNLOAD_SCREENREADER_CANCELED},

      // Warning bypass prompt (used in both the dialog and interstitial).
      {"warningBypassPromptLearnMoreLink",
       IDS_DOWNLOAD_WARNING_BYPASS_PROMPT_LEARN_MORE_LINK},
      {"warningBypassPromptDescription",
       IDS_DOWNLOAD_WARNING_BYPASS_PROMPT_DESCRIPTION},

      // Warning bypass prompt accessibility text.
      {"warningBypassPromptLearnMoreLinkAccessible",
       IDS_DOWNLOAD_WARNING_BYPASS_PROMPT_LEARN_MORE_LINK_ACCESSIBLE},

      // Warning bypass dialog.
      {"warningBypassDialogTitle", IDS_DOWNLOAD_WARNING_BYPASS_DIALOG_TITLE},
      {"warningBypassDialogCancel", IDS_CANCEL},

      // Warning bypass interstitial main content.
      {"warningBypassInterstitialTitle",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_TITLE},
      {"warningBypassInterstitialContinue",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_CONTINUE},
      {"warningBypassInterstitialCancel",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_CANCEL},
      {"warningBypassInterstitialDownload",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_DOWNLOAD},

      // Warning bypass interstitial survey content.
      {"warningBypassInterstitialSurveyTitle",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_TITLE},
      {"warningBypassInterstitialSurveyCreatedFile",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_CREATED_FILE},
      {"warningBypassInterstitialSurveyTrustSiteWithoutUrl",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_TRUST_SITE_WITHOUT_URL},
      {"warningBypassInterstitialSurveyTrustSiteWithUrl",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_TRUST_SITE_WITH_URL},
      {"warningBypassInterstitialSurveyAcceptRisk",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_ACCEPT_RISK},

      // Warning bypass interstitial accessibility text.
      {"warningBypassInterstitialSurveyTitleAccessible",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_TITLE_ACCESSIBLE},
      {"warningBypassInterstitialSurveyCreatedFileAccessible",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_CREATED_FILE_ACCESSIBLE},
      {"warningBypassInterstitialSurveyTrustSiteWithoutUrlAccessible",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_TRUST_SITE_WITHOUT_URL_ACCESSIBLE},
      {"warningBypassInterstitialSurveyTrustSiteWithUrlAccessible",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_TRUST_SITE_WITH_URL_ACCESSIBLE},
      {"warningBypassInterstitialSurveyAcceptRiskAccessible",
       IDS_DOWNLOAD_WARNING_BYPASS_INTERSTITIAL_SURVEY_ACCEPT_RISK_ACCESSIBLE},

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // ESB Download Row Promo
      {"esbDownloadRowPromoString", IDS_DOWNLOAD_ROW_ESB_PROMOTION},
      {"esbDownloadRowPromoA11y", IDS_DOWNLOAD_ROW_ESB_PROMO_A11Y},
#endif

      // Strings describing reasons for blocked downloads.
      {"noSafeBrowsingDesc",
       IDS_BLOCK_DOWNLOAD_REASON_UNVERIFIED_NO_SAFE_BROWSING},
      {"dangerFileDesc", IDS_BLOCK_DOWNLOAD_REASON_DANGEROUS_FILETYPE},
      {"dangerDownloadDesc", IDS_BLOCK_DOWNLOAD_REASON_DANGEROUS},
      {"dangerDownloadCookieTheft",
       IDS_BLOCK_DOWNLOAD_REASON_DANGEROUS_COOKIE_THEFT},
      {"dangerDownloadCookieTheftAndAccountDesc",
       IDS_BLOCK_DOWNLOAD_REASON_DANGEROUS_COOKIE_THEFT_AND_ACCOUNT},
      {"dangerSettingsDesc", IDS_BLOCK_DOWNLOAD_REASON_POTENTIALLY_UNWANTED},
      {"insecureDownloadDesc", IDS_BLOCK_DOWNLOAD_REASON_INSECURE},

      {"referrerLine", IDS_DOWNLOADS_PAGE_REFERRER_LINE},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddBoolean("dangerousDownloadInterstitial",
                     base::FeatureList::IsEnabled(
                         safe_browsing::kDangerousDownloadInterstitial));

  source->AddBoolean(
      "showReferrerUrl",
      base::FeatureList::IsEnabled(safe_browsing::kDownloadsPageReferrerUrl));
  source->AddLocalizedString(
      "dangerUncommonDesc",
      requests_ap_verdicts
          ? IDS_BLOCK_REASON_UNCOMMON_DOWNLOAD_IN_ADVANCED_PROTECTION
          : IDS_BLOCK_DOWNLOAD_REASON_UNCOMMON);
  source->AddLocalizedString(
      "dangerUncommonSuspiciousArchiveDesc",
      requests_ap_verdicts
          ? IDS_BLOCK_REASON_UNCOMMON_DOWNLOAD_IN_ADVANCED_PROTECTION
          : IDS_BLOCK_DOWNLOAD_REASON_UNCOMMON_SUSPICIOUS_ARCHIVE);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Download Row ESB Promo:
  source->AddBoolean("esbDownloadRowPromo",
                     base::FeatureList::IsEnabled(
                         feature_engagement::kEsbDownloadRowPromoFeature));
#endif

  // Build an Accelerator to describe undo shortcut
  // NOTE: the undo shortcut is also defined in downloads/downloads.html
  // TODO(crbug.com/40597071): de-duplicate shortcut by moving all shortcut
  // definitions from JS to C++.
  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));

  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean("allowDeletingHistory",
                     prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory) &&
                         !profile->IsChild());

  // The URL to open when the user clicks on "Learn more" for a blocked
  // dangerous file.
  source->AddString("blockedLearnMoreUrl",
                    google_util::AppendGoogleLocaleParam(
                        GURL(chrome::kDownloadBlockedLearnMoreURL),
                        g_browser_process->GetApplicationLocale())
                        .spec());

  source->AddResourcePath("safebrowsing_shared.css",
                          IDR_SECURITY_INTERSTITIAL_SAFEBROWSING_SHARED_CSS);

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUIConfig
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUIConfig::DownloadsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIDownloadsHost) {}

DownloadsUIConfig::~DownloadsUIConfig() = default;

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUI::DownloadsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Download.WebUi.DocumentLoadedInMainFrameTime",
                        "Download.WebUi.LoadCompletedInMainFrame") {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  // Set up the chrome://downloads/ source.
  content::WebUIDataSource* source = CreateAndAddDownloadsUIHTMLSource(profile);
  ManagedUIHandler::Initialize(web_ui, source);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  base::UmaHistogramEnumeration(
      "Download.OpenDownloads.PerProfileType",
      profile_metrics::GetBrowserProfileType(profile));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DownloadsUI)

DownloadsUI::~DownloadsUI() = default;

// static
base::RefCountedMemory* DownloadsUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_DOWNLOADS_FAVICON, scale_factor);
}

void DownloadsUI::BindInterface(
    mojo::PendingReceiver<downloads::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();

  page_factory_receiver_.Bind(std::move(receiver));
}

void DownloadsUI::CreatePageHandler(
    mojo::PendingRemote<downloads::mojom::Page> page,
    mojo::PendingReceiver<downloads::mojom::PageHandler> receiver) {
  DCHECK(page);
  Profile* profile = Profile::FromWebUI(web_ui());
  DownloadManager* dlm = profile->GetDownloadManager();

  page_handler_ = std::make_unique<DownloadsDOMHandler>(
      std::move(receiver), std::move(page), dlm, web_ui());
}
