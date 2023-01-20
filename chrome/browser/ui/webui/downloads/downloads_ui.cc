// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_ui.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/values.h"
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
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/downloads_resources.h"
#include "chrome/grit/downloads_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
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
      {"title", IDS_DOWNLOAD_TITLE},
      {"searchResultsPlural", IDS_SEARCH_RESULTS_PLURAL},
      {"searchResultsSingular", IDS_SEARCH_RESULTS_SINGULAR},
      {"downloads", IDS_DOWNLOAD_TITLE},
      {"actionMenuDescription", IDS_DOWNLOAD_ACTION_MENU_DESCRIPTION},
      {"clearAll", IDS_DOWNLOAD_LINK_CLEAR_ALL},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"openDownloadsFolder", IDS_DOWNLOAD_LINK_OPEN_DOWNLOADS_FOLDER},
      {"moreActions", IDS_DOWNLOAD_MORE_ACTIONS},
      {"search", IDS_DOWNLOAD_SEARCH},

      // No results message that shows instead of the downloads list.
      {"noDownloads", IDS_DOWNLOAD_NO_DOWNLOADS},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},

      // Status.
      {"statusCancelled", IDS_DOWNLOAD_TAB_CANCELLED},
      {"statusRemoved", IDS_DOWNLOAD_FILE_REMOVED},

      // Dangerous file.
      {"dangerFileDesc", IDS_BLOCK_REASON_GENERIC_DOWNLOAD},
      {"dangerSave", IDS_CONFIRM_DOWNLOAD},
      {"dangerRestore", IDS_CONFIRM_DOWNLOAD_RESTORE},
      {"dangerDiscard", IDS_DISCARD_DOWNLOAD},
      {"dangerReview", IDS_REVIEW_DOWNLOAD},

      // Deep scanning strings.
      {"deepScannedSafeDesc", IDS_DEEP_SCANNED_SAFE_DESCRIPTION},
      {"deepScannedOpenedDangerousDesc",
       IDS_DEEP_SCANNED_OPENED_DANGEROUS_DESCRIPTION},
      {"sensitiveContentWarningDesc",
       IDS_BLOCK_REASON_SENSITIVE_CONTENT_WARNING},
      {"sensitiveContentBlockedDesc",
       IDS_SENSITIVE_CONTENT_BLOCKED_DESCRIPTION},
      {"blockedTooLargeDesc", IDS_BLOCKED_TOO_LARGE_DESCRIPTION},
      {"blockedPasswordProtectedDesc",
       IDS_BLOCKED_PASSWORD_PROTECTED_DESCRIPTION},
      {"promptForScanningDesc", IDS_BLOCK_REASON_PROMPT_FOR_SCANNING},

      // Controls.
      {"controlPause", IDS_DOWNLOAD_LINK_PAUSE},
      {"controlCancel", IDS_DOWNLOAD_LINK_CANCEL},
      {"controlResume", IDS_DOWNLOAD_LINK_RESUME},
      {"controlRemoveFromList", IDS_DOWNLOAD_LINK_REMOVE},
      {"controlRemoveFromListAriaLabel", IDS_DOWNLOAD_LINK_REMOVE_ARIA_LABEL},
      {"controlRetry", IDS_DOWNLOAD_LINK_RETRY},
      {"controlledByUrl", IDS_DOWNLOAD_BY_EXTENSION_URL},
      {"controlOpenNow", IDS_OPEN_DOWNLOAD_NOW},
      {"controlDeepScan", IDS_DOWNLOAD_DEEP_SCAN},
      {"controlBypassDeepScan", IDS_DOWNLOAD_BYPASS_DEEP_SCAN},
      {"toastClearedAll", IDS_DOWNLOAD_TOAST_CLEARED_ALL},
      {"toastRemovedFromList", IDS_DOWNLOAD_TOAST_REMOVED_FROM_LIST},
      {"undo", IDS_DOWNLOAD_UNDO},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddLocalizedString("dangerDownloadDesc",
                             IDS_BLOCK_REASON_DANGEROUS_DOWNLOAD);
  source->AddLocalizedString(
      "dangerUncommonDesc",
      requests_ap_verdicts
          ? IDS_BLOCK_REASON_UNCOMMON_DOWNLOAD_IN_ADVANCED_PROTECTION
          : IDS_BLOCK_REASON_UNCOMMON_DOWNLOAD);
  source->AddLocalizedString("dangerSettingsDesc",
                             IDS_BLOCK_REASON_UNWANTED_DOWNLOAD);
  source->AddLocalizedString("insecureDownloadDesc",
                             IDS_BLOCK_REASON_INSECURE_DOWNLOAD);
  source->AddLocalizedString("asyncScanningDownloadDesc",
                             IDS_BLOCK_REASON_DEEP_SCANNING);
  source->AddLocalizedString("accountCompromiseDownloadDesc",
                             IDS_BLOCK_REASON_ACCOUNT_COMPROMISE);
  source->AddBoolean("hasShowInFolder",
                     browser_defaults::kDownloadPageHasShowInFolder);

  // Build an Accelerator to describe undo shortcut
  // NOTE: the undo shortcut is also defined in downloads/downloads.html
  // TODO(crbug/893033): de-duplicate shortcut by moving all shortcut
  // definitions from JS to C++.
  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));

  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean("allowDeletingHistory",
                     prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory) &&
                         !profile->IsChild());

  source->AddLocalizedString("inIncognito", IDS_DOWNLOAD_IN_INCOGNITO);

  source->AddBoolean(
      "allowOpenNow",
      !enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
           profile)
           ->DelayUntilVerdict(
               enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED));

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// DownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

DownloadsUI::DownloadsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
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
