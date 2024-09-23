// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_dom_handler.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/current_thread.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_query.h"
#include "chrome/browser/download/download_ui_safe_browsing_util.h"
#include "chrome/browser/download/download_warning_desktop_hats_utils.h"
#include "chrome/browser/download/drag_download_item.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/fileicon_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/download/public/common/download_item.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;

namespace {

using WarningAction = DownloadItemWarningData::WarningAction;
using WarningSurface = DownloadItemWarningData::WarningSurface;

enum DownloadsDOMEvent {
  DOWNLOADS_DOM_EVENT_GET_DOWNLOADS = 0,
  DOWNLOADS_DOM_EVENT_OPEN_FILE = 1,
  DOWNLOADS_DOM_EVENT_DRAG = 2,
  // Obsolete: DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS = 3,
  DOWNLOADS_DOM_EVENT_DISCARD_DANGEROUS = 4,
  DOWNLOADS_DOM_EVENT_SHOW = 5,
  DOWNLOADS_DOM_EVENT_PAUSE = 6,
  DOWNLOADS_DOM_EVENT_REMOVE = 7,
  DOWNLOADS_DOM_EVENT_CANCEL = 8,
  DOWNLOADS_DOM_EVENT_CLEAR_ALL = 9,
  DOWNLOADS_DOM_EVENT_OPEN_FOLDER = 10,
  DOWNLOADS_DOM_EVENT_RESUME = 11,
  DOWNLOADS_DOM_EVENT_RETRY_DOWNLOAD = 12,
  DOWNLOADS_DOM_EVENT_OPEN_DURING_SCANNING = 13,
  DOWNLOADS_DOM_EVENT_REVIEW_DANGEROUS = 14,
  DOWNLOADS_DOM_EVENT_DEEP_SCAN = 15,
  DOWNLOADS_DOM_EVENT_BYPASS_DEEP_SCAN = 16,
  DOWNLOADS_DOM_EVENT_SAVE_SUSPICIOUS = 17,
  DOWNLOADS_DOM_EVENT_OPEN_BYPASS_WARNING_PROMPT = 18,
  DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS_FROM_PROMPT = 19,
  DOWNLOADS_DOM_EVENT_CANCEL_BYPASS_WARNING_PROMPT = 20,
  DOWNLOADS_DOM_EVENT_OPEN_SURVEY_ON_DANGEROUS_INTERSTITIAL = 21,
  DOWNLOADS_DOM_EVENT_MAX
};

void CountDownloadsDOMEvents(DownloadsDOMEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Download.DOMEvent", event,
                            DOWNLOADS_DOM_EVENT_MAX);
}

bool CanLogWarningMetrics(download::DownloadItem* file) {
  return file && file->IsDangerous() && !file->IsDone();
}

std::string InteractionTypeToString(
    DangerousDownloadInterstitialInteraction interaction_type) {
  switch (interaction_type) {
    case DangerousDownloadInterstitialInteraction::kCancelInterstitial:
      return "CancelInterstitial";
    case DangerousDownloadInterstitialInteraction::kOpenSurvey:
      return "OpenSurvey";
    case DangerousDownloadInterstitialInteraction::kCompleteSurvey:
      return "CompleteSurvey";
    case DangerousDownloadInterstitialInteraction::kSaveDangerous:
      return "SaveDangerous";
  }
}

void RecordDangerousDownloadInterstitialActionHistogram(
    DangerousDownloadInterstitialAction action) {
  base::UmaHistogramEnumeration("Download.DangerousDownloadInterstitial.Action",
                                action);
}

void RecordDangerousDownloadInterstitialInteractionHistogram(
    DangerousDownloadInterstitialInteraction interaction_type,
    const base::TimeDelta elapsed_time) {
  const std::string histogram_name =
      "Download.DangerousDownloadInterstitial.InteractionTime." +
      InteractionTypeToString(interaction_type);
  base::UmaHistogramMediumTimes(histogram_name, elapsed_time);
}

void PromptForScanningInBubble(content::WebContents* web_contents,
                               download::DownloadItem* download) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return;
  }
  browser->window()
      ->GetDownloadBubbleUIController()
      ->GetDownloadDisplayController()
      ->OpenSecuritySubpage(
          OfflineItemUtils::GetContentIdForDownload(download));
}

// Records DownloadItemWarningData and maybe sends the Safe Browsing report.
// This should be called when the user takes a bypass action (either proceed or
// cancel).
void MaybeReportBypassAction(download::DownloadItem* file,
                             WarningSurface surface,
                             WarningAction action) {
  CHECK(file);
  CHECK(file->IsDangerous());
  CHECK(!file->IsDone());
  CHECK(surface == WarningSurface::DOWNLOADS_PAGE ||
        surface == WarningSurface::DOWNLOAD_PROMPT);
  CHECK(action == WarningAction::PROCEED || action == WarningAction::CANCEL ||
        action == WarningAction::DISCARD || action == WarningAction::KEEP);
  // If this is called from the DOWNLOADS_PAGE, the action must be proceed,
  // discard, or keep. There is no cancellation action on the page, because
  // there's no prompt to cancel.
  CHECK(surface != WarningSurface::DOWNLOADS_PAGE ||
        action != WarningAction::CANCEL);

  // The warning action event needs to be added before Safe Browsing report is
  // sent, because this event should be included in the report.
  DownloadItemWarningData::AddWarningActionEvent(file, surface, action);

  // Do not send cancel or keep report since it's not a terminal action.
  if (action != WarningAction::PROCEED && action != WarningAction::DISCARD) {
    return;
  }
  SendSafeBrowsingDownloadReport(
      safe_browsing::ClientSafeBrowsingReportRequest::
          DANGEROUS_DOWNLOAD_RECOVERY,
      /*did_proceed=*/action == WarningAction::PROCEED, file);
}

// Triggers a Trust and Safety sentiment survey (if enabled). Should be called
// when the user takes an explicit action to save or discard a
// suspicious/dangerous file. Not called when the prompt is merely shown.
void MaybeTriggerTrustSafetySurvey(download::DownloadItem* file,
                                   WarningSurface surface,
                                   WarningAction action) {
  CHECK(file);
  CHECK(surface == WarningSurface::DOWNLOADS_PAGE ||
        surface == WarningSurface::DOWNLOAD_PROMPT);
  CHECK(action == WarningAction::PROCEED || action == WarningAction::DISCARD);
  if (Profile* profile = Profile::FromBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(file));
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

void RecordDownloadsPageValidatedHistogram(download::DownloadItem* item) {
  base::UmaHistogramEnumeration(
      "Download.UserValidatedDangerousDownload.DownloadsPage",
      item->GetDangerType(), download::DOWNLOAD_DANGER_TYPE_MAX);
}

}  // namespace

DownloadsDOMHandler::DownloadsDOMHandler(
    mojo::PendingReceiver<downloads::mojom::PageHandler> receiver,
    mojo::PendingRemote<downloads::mojom::Page> page,
    content::DownloadManager* download_manager,
    content::WebUI* web_ui)
    : list_tracker_(download_manager, std::move(page)),
      web_ui_(web_ui),
      receiver_(this, std::move(receiver)) {
  // Create our fileicon data source.
  content::URLDataSource::Add(
      Profile::FromBrowserContext(download_manager->GetBrowserContext()),
      std::make_unique<FileIconSource>());
  CheckForRemovedFiles();
}

DownloadsDOMHandler::~DownloadsDOMHandler() {
  OnDownloadsPageDismissed();
  list_tracker_.Stop();
  list_tracker_.Reset();
  if (!render_process_gone_) {
    CheckForRemovedFiles();
  }
  FinalizeRemovals();
}

void DownloadsDOMHandler::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // TODO(dbeam): WebUI + WebUIMessageHandler should do this automatically.
  // http://crbug.com/610450
  render_process_gone_ = true;
}

void DownloadsDOMHandler::GetDownloads(
    const std::vector<std::string>& search_terms) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_GET_DOWNLOADS);

  bool terms_changed = list_tracker_.SetSearchTerms(search_terms);
  if (terms_changed) {
    list_tracker_.Reset();
  }

  list_tracker_.StartAndSendChunk();
}

void DownloadsDOMHandler::OpenFileRequiringGesture(const std::string& id) {
  if (!GetWebUIWebContents()->HasRecentInteraction()) {
    LOG(ERROR) << "OpenFileRequiringGesture received without recent "
                  "user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FILE);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file) {
    file->OpenDownload();
  }
}

void DownloadsDOMHandler::Drag(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DRAG);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!file) {
    return;
  }

  content::WebContents* web_contents = GetWebUIWebContents();
  // |web_contents| is only NULL in the test.
  if (!web_contents) {
    return;
  }

  if (file->GetState() != download::DownloadItem::COMPLETE) {
    return;
  }
  const display::Screen* const screen = display::Screen::GetScreen();
  gfx::NativeView view = web_contents->GetNativeView();
  gfx::Image* icon = g_browser_process->icon_manager()->LookupIconFromFilepath(
      file->GetTargetFilePath(), IconLoader::NORMAL,
      screen->GetPreferredScaleFactorForView(view).value_or(1.0f));
  {
    // Enable nested tasks during DnD, while |DragDownload()| blocks.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;
    DragDownloadItem(file, icon, view);
  }
}

// "Suspicious" in this context applies to insecure as well as dangerous
// downloads of certain danger types.
void DownloadsDOMHandler::SaveSuspiciousRequiringGesture(
    const std::string& id) {
  if (!GetWebUIWebContents()->HasRecentInteraction()) {
    LOG(ERROR) << "SaveSuspiciousRequiringGesture received without recent "
                  "user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_SUSPICIOUS);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!file || file->IsDone()) {
    return;
  }

  // If a download is insecure, validate that first. Is most cases, insecure
  // download warnings will occur first, but in the worst case scenario, we show
  // a dangerous warning twice. That's better than showing an insecure download
  // warning, then dismissing the dangerous download warning. Since insecure
  // downloads triggering the UI are temporary and rare to begin with, this
  // should very rarely occur.
  if (file->IsInsecure()) {
    // `file` is potentially deleted.
    file->ValidateInsecureDownload();
  } else if (file->IsDangerous()) {
    MaybeReportBypassAction(file, WarningSurface::DOWNLOADS_PAGE,
                            WarningAction::PROCEED);
    MaybeTriggerDownloadWarningHatsSurvey(
        file, DownloadWarningHatsType::kDownloadsPageBypass);
    MaybeTriggerTrustSafetySurvey(file, WarningSurface::DOWNLOADS_PAGE,
                                  WarningAction::PROCEED);

    RecordDownloadsPageValidatedHistogram(file);

    // `file` is potentially deleted.
    file->ValidateDangerousDownload();
  }
}

void DownloadsDOMHandler::RecordOpenBypassWarningDialog(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_BYPASS_WARNING_PROMPT);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!CanLogWarningMetrics(file)) {
    return;
  }

  RecordDownloadDangerPromptHistogram("Shown", *file);

  MaybeReportBypassAction(file, WarningSurface::DOWNLOADS_PAGE,
                          WarningAction::KEEP);
}

void DownloadsDOMHandler::RecordOpenBypassWarningInterstitial(
    const std::string& id) {
  CHECK(base::FeatureList::IsEnabled(
      safe_browsing::kDangerousDownloadInterstitial));
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_BYPASS_WARNING_PROMPT);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!CanLogWarningMetrics(file)) {
    return;
  }

  interstitial_open_time_ = base::TimeTicks::Now();

  RecordDangerousDownloadInterstitialActionHistogram(
      DangerousDownloadInterstitialAction::kOpenInterstitial);

  RecordDownloadDangerPromptHistogram("Shown", *file);

  MaybeReportBypassAction(file, WarningSurface::DOWNLOADS_PAGE,
                          WarningAction::KEEP);
}

void DownloadsDOMHandler::RecordOpenSurveyOnDangerousInterstitial(
    const std::string& id) {
  CHECK(base::FeatureList::IsEnabled(
      safe_browsing::kDangerousDownloadInterstitial));
  CountDownloadsDOMEvents(
      DOWNLOADS_DOM_EVENT_OPEN_SURVEY_ON_DANGEROUS_INTERSTITIAL);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!CanLogWarningMetrics(file)) {
    return;
  }

  DCHECK(interstitial_open_time_.has_value())
      << "Dangerous download interstitial survey should only open after the "
         "download interstitial is opened.";
  interstitial_survey_open_time_ = base::TimeTicks::Now();

  RecordDangerousDownloadInterstitialInteractionHistogram(
      DangerousDownloadInterstitialInteraction::kOpenSurvey,
      (*interstitial_survey_open_time_) - (*interstitial_open_time_));
  RecordDangerousDownloadInterstitialActionHistogram(
      DangerousDownloadInterstitialAction::kOpenSurvey);
}

void DownloadsDOMHandler::SaveDangerousFromDialogRequiringGesture(
    const std::string& id) {
  if (!GetWebUIWebContents()->HasRecentInteraction()) {
    LOG(ERROR) << "SaveDangerousFromDialogRequiringGesture received without "
                  "recent user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS_FROM_PROMPT);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!CanLogWarningMetrics(file)) {
    return;
  }

  RecordDownloadDangerPromptHistogram("Proceed", *file);

  MaybeReportBypassAction(file, WarningSurface::DOWNLOAD_PROMPT,
                          WarningAction::PROCEED);
  MaybeTriggerDownloadWarningHatsSurvey(
      file, DownloadWarningHatsType::kDownloadsPageBypass);
  MaybeTriggerTrustSafetySurvey(file, WarningSurface::DOWNLOAD_PROMPT,
                                WarningAction::PROCEED);

  RecordDownloadsPageValidatedHistogram(file);

  // `file` is potentially deleted.
  file->ValidateDangerousDownload();
}

void DownloadsDOMHandler::SaveDangerousFromInterstitialNeedGesture(
    const std::string& id,
    downloads::mojom::DangerousDownloadInterstitialSurveyOptions response) {
  CHECK(base::FeatureList::IsEnabled(
      safe_browsing::kDangerousDownloadInterstitial));
  if (!GetWebUIWebContents()->HasRecentInteraction()) {
    LOG(ERROR) << "SaveDangerousFromInterstitialNeedGesture received without "
                  "recent user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SAVE_DANGEROUS_FROM_PROMPT);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!CanLogWarningMetrics(file)) {
    return;
  }

  DCHECK(interstitial_open_time_.has_value())
      << "Saving from the dangerous download interstitial should only happen "
         "if the interstitial is opened.";
  DCHECK(interstitial_survey_open_time_.has_value())
      << "Saving from the dangerous download interstitial should only happen "
         "after the interstitial survey is opened.";

  base::TimeTicks save_time = base::TimeTicks::Now();
  RecordDangerousDownloadInterstitialInteractionHistogram(
      DangerousDownloadInterstitialInteraction::kCompleteSurvey,
      save_time - (*interstitial_survey_open_time_));
  RecordDangerousDownloadInterstitialInteractionHistogram(
      DangerousDownloadInterstitialInteraction::kSaveDangerous,
      save_time - (*interstitial_open_time_));

  RecordDangerousDownloadInterstitialActionHistogram(
      DangerousDownloadInterstitialAction::kSaveDangerous);

  base::UmaHistogramEnumeration(
      "Download.DangerousDownloadInterstitial.SurveyResponse", response);

  RecordDownloadDangerPromptHistogram("Proceed", *file);

  MaybeReportBypassAction(file, WarningSurface::DOWNLOAD_PROMPT,
                          WarningAction::PROCEED);
  MaybeTriggerDownloadWarningHatsSurvey(
      file, DownloadWarningHatsType::kDownloadsPageBypass);
  MaybeTriggerTrustSafetySurvey(file, WarningSurface::DOWNLOAD_PROMPT,
                                WarningAction::PROCEED);

  RecordDownloadsPageValidatedHistogram(file);

  // `file` is potentially deleted.
  file->ValidateDangerousDownload();
}

void DownloadsDOMHandler::RecordCancelBypassWarningDialog(
    const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CANCEL_BYPASS_WARNING_PROMPT);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!CanLogWarningMetrics(file)) {
    return;
  }

  MaybeReportBypassAction(file, WarningSurface::DOWNLOAD_PROMPT,
                          WarningAction::CANCEL);
}

void DownloadsDOMHandler::RecordCancelBypassWarningInterstitial(
    const std::string& id) {
  CHECK(base::FeatureList::IsEnabled(
      safe_browsing::kDangerousDownloadInterstitial));
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CANCEL_BYPASS_WARNING_PROMPT);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!CanLogWarningMetrics(file)) {
    return;
  }

  DCHECK(interstitial_open_time_.has_value())
      << "Dangerous download interstitial should only be cancelled after the "
         "download interstitial is opened.";

  RecordDangerousDownloadInterstitialInteractionHistogram(
      DangerousDownloadInterstitialInteraction::kCancelInterstitial,
      base::TimeTicks::Now() - (*interstitial_open_time_));

  RecordDangerousDownloadInterstitialActionHistogram(
      DangerousDownloadInterstitialAction::kCancelInterstitial);

  MaybeReportBypassAction(file, WarningSurface::DOWNLOAD_PROMPT,
                          WarningAction::CANCEL);
}

void DownloadsDOMHandler::DiscardDangerous(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DISCARD_DANGEROUS);
  download::DownloadItem* download = GetDownloadByStringId(id);
  if (download && !download->IsDone() && download->IsDangerous()) {
    MaybeReportBypassAction(download, WarningSurface::DOWNLOADS_PAGE,
                            WarningAction::DISCARD);
    MaybeTriggerDownloadWarningHatsSurvey(
        download, DownloadWarningHatsType::kDownloadsPageHeed);
    MaybeTriggerTrustSafetySurvey(download, WarningSurface::DOWNLOADS_PAGE,
                                  WarningAction::DISCARD);
  }
  RemoveDownloadInArgs(id);
}

void DownloadsDOMHandler::RetryDownload(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_RETRY_DOWNLOAD);

  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!file) {
    return;
  }
  content::WebContents* web_contents = GetWebUIWebContents();
  content::RenderFrameHost* render_frame_host =
      web_contents->GetPrimaryMainFrame();
  const GURL url = file->GetURL();

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("downloads_dom_handler", R"(
        semantics {
          sender: "The downloads page."
          description: "Retrying a download."
          trigger:
            "The user selects the 'Retry' button for a cancelled download on "
            "the downloads page."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled by settings, but it's only "
            "triggered by user request."
          policy_exception_justification: "Not implemented."
        })");

  // For "Retry", we want to use the network isolation key associated with the
  // initial download request rather than treating it as initiated from the
  // chrome://downloads/ page. Thus we get the NIK from |file|, not from
  // |render_frame_host|.
  auto dl_params = std::make_unique<download::DownloadUrlParameters>(
      url, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), traffic_annotation);
  dl_params->set_content_initiated(true);
  dl_params->set_initiator(url::Origin::Create(GURL("chrome://downloads")));
  dl_params->set_download_source(download::DownloadSource::RETRY);

  web_contents->GetBrowserContext()->GetDownloadManager()->DownloadUrl(
      std::move(dl_params));
}

void DownloadsDOMHandler::Show(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_SHOW);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file) {
    file->ShowDownloadInShell();
  }
}

void DownloadsDOMHandler::Pause(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_PAUSE);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file) {
    file->Pause();
  }
}

void DownloadsDOMHandler::Resume(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_RESUME);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file) {
    file->Resume(true);
  }
}

void DownloadsDOMHandler::Remove(const std::string& id) {
  if (!IsDeletingHistoryAllowed()) {
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_REMOVE);
  RemoveDownloadInArgs(id);
}

void DownloadsDOMHandler::Undo() {
  // TODO(dbeam): handle more than removed downloads someday?
  if (removals_.empty()) {
    return;
  }

  const IdSet last_removed_ids = removals_.back();
  removals_.pop_back();

  const bool undoing_clear_all = last_removed_ids.size() > 1;
  if (undoing_clear_all) {
    list_tracker_.Reset();
    list_tracker_.Stop();
  }

  for (auto id : last_removed_ids) {
    download::DownloadItem* download = GetDownloadById(id);
    if (!download) {
      continue;
    }

    DownloadItemModel model(download);
    model.SetShouldShowInShelf(true);
    model.SetIsBeingRevived(true);

    download->UpdateObservers();

    model.SetIsBeingRevived(false);
  }

  if (undoing_clear_all) {
    list_tracker_.StartAndSendChunk();
  }
}

void DownloadsDOMHandler::Cancel(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CANCEL);
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (file) {
    file->Cancel(true);
  }
}

void DownloadsDOMHandler::ClearAll() {
  if (!IsDeletingHistoryAllowed()) {
    // This should only be reached during tests.
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_CLEAR_ALL);

  list_tracker_.Reset();
  list_tracker_.Stop();

  DownloadVector downloads;
  if (GetMainNotifierManager()) {
    GetMainNotifierManager()->GetAllDownloads(&downloads);
  }
  if (GetOriginalNotifierManager()) {
    GetOriginalNotifierManager()->GetAllDownloads(&downloads);
  }
  RemoveDownloads(downloads);

  list_tracker_.StartAndSendChunk();
}

void DownloadsDOMHandler::RemoveDownloads(const DownloadVector& to_remove) {
  IdSet ids;

  for (download::DownloadItem* download : to_remove) {
    if (download->IsDangerous() || download->IsInsecure()) {
      // Don't allow users to revive dangerous downloads; just nuke 'em.
      download->Remove();
      continue;
    }

    DownloadItemModel item_model(download);
    if (!item_model.ShouldShowInShelf() ||
        download->GetState() == download::DownloadItem::IN_PROGRESS) {
      continue;
    }

    item_model.SetShouldShowInShelf(false);
    ids.insert(download->GetId());
    download->UpdateObservers();
  }

  if (!ids.empty()) {
    removals_.push_back(ids);
  }
}

void DownloadsDOMHandler::OpenDownloadsFolderRequiringGesture() {
  if (!GetWebUIWebContents()->HasRecentInteraction()) {
    LOG(ERROR) << "OpenDownloadsFolderRequiringGesture received without recent "
                  "user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_FOLDER);
  content::DownloadManager* manager = GetMainNotifierManager();
  if (manager) {
    platform_util::OpenItem(
        Profile::FromBrowserContext(manager->GetBrowserContext()),
        DownloadPrefs::FromDownloadManager(manager)->DownloadPath(),
        platform_util::OPEN_FOLDER, platform_util::OpenOperationCallback());
  }
}

void DownloadsDOMHandler::OpenDuringScanningRequiringGesture(
    const std::string& id) {
  if (!GetWebUIWebContents()->HasRecentInteraction()) {
    LOG(ERROR) << "OpenDownloadsFolderRequiringGesture received without recent "
                  "user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_OPEN_DURING_SCANNING);
  download::DownloadItem* download = GetDownloadByStringId(id);
  if (download) {
    DownloadItemModel model(download);
    model.SetOpenWhenComplete(true);
#if BUILDFLAG(FULL_SAFE_BROWSING)
    model.CompleteSafeBrowsingScan();
#endif
  }
}

void DownloadsDOMHandler::DeepScan(const std::string& id) {
  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_DEEP_SCAN);
  download::DownloadItem* download = GetDownloadByStringId(id);
  if (!download) {
    return;
  }

  if (DownloadItemWarningData::IsTopLevelEncryptedArchive(download)) {
    // For encrypted archives, we need a password from the user. We will request
    // this in the download bubble.
    PromptForScanningInBubble(GetWebUIWebContents(), download);
    return;
  }

  LogDeepScanEvent(download,
                   safe_browsing::DeepScanEvent::kPromptAcceptedFromWebUI);
  DownloadItemWarningData::AddWarningActionEvent(
      download, DownloadItemWarningData::WarningSurface::DOWNLOADS_PAGE,
      DownloadItemWarningData::WarningAction::ACCEPT_DEEP_SCAN);
  DownloadItemModel model(download);
  DownloadCommands commands(model.GetWeakPtr());
  commands.ExecuteCommand(DownloadCommands::DEEP_SCAN);
}

void DownloadsDOMHandler::BypassDeepScanRequiringGesture(
    const std::string& id) {
  if (!GetWebUIWebContents()->HasRecentInteraction()) {
    LOG(ERROR) << "BypassDeepScanRequiringGesture received without recent "
                  "user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_BYPASS_DEEP_SCAN);
  download::DownloadItem* download = GetDownloadByStringId(id);
  if (download) {
    if (CanShowDownloadWarningHatsSurvey(download)) {
      MaybeTriggerDownloadWarningHatsSurvey(
          download, DownloadWarningHatsType::kDownloadsPageBypass);
    }
    DownloadItemModel model(download);
    DownloadCommands commands(model.GetWeakPtr());
    // The button says "Download suspicious file" which does not imply opening
    // the file.
    commands.ExecuteCommand(DownloadCommands::BYPASS_DEEP_SCANNING);
  }
}

void DownloadsDOMHandler::ReviewDangerousRequiringGesture(
    const std::string& id) {
  if (!GetWebUIWebContents()->HasRecentInteraction()) {
    LOG(ERROR) << __func__ << " received without recent user interaction";
    return;
  }

  CountDownloadsDOMEvents(DOWNLOADS_DOM_EVENT_REVIEW_DANGEROUS);
  download::DownloadItem* download = GetDownloadByStringId(id);
  if (download) {
    DownloadItemModel model(download);
    model.ReviewScanningVerdict(GetWebUIWebContents());
  }
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// This function will be called when a user clicks on the ESB
// (Enhanced Safe Browsing) download row promo. It will notify
// the feature engagement backend to record the event that the
// promo was clicked.
void DownloadsDOMHandler::OpenEsbSettings() {
  Browser* browser = chrome::FindBrowserWithTab(GetWebUIWebContents());
  if (!browser) {
    return;
  }
  chrome::ShowSafeBrowsingEnhancedProtectionWithIph(
      browser,
      safe_browsing::SafeBrowsingSettingReferralMethod::kDownloadPageRowPromo);

  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser->profile());
  tracker->NotifyEvent("esb_download_promo_row_clicked");
  base::RecordAction(
      base::UserMetricsAction("SafeBrowsing.EsbDownloadRowPromo.Click"));
  base::UmaHistogramEnumeration(
      "SafeBrowsing.EsbDownloadRowPromo.Outcome",
      SafeBrowsingEsbDownloadRowPromoOutcome::kClicked);
}

void DownloadsDOMHandler::IsEligibleForEsbPromo(
    IsEligibleForEsbPromoCallback callback) {
  content::DownloadManager* manager = GetMainNotifierManager();
  if (!manager) {
    std::move(callback).Run(false);
    return;
  }

  content::BrowserContext* browser_context = manager->GetBrowserContext();

  if (!safe_browsing::SafeBrowsingService::IsUserEligibleForESBPromo(
          Profile::FromBrowserContext(browser_context))) {
    std::move(callback).Run(false);
    return;
  }
  bool should_show_esb_promo = false;
  if (feature_engagement::Tracker* tracker =
          feature_engagement::TrackerFactory::GetForBrowserContext(
              browser_context);
      tracker && tracker->ShouldTriggerHelpUI(
                     feature_engagement::kEsbDownloadRowPromoFeature)) {
    should_show_esb_promo = true;
    // since the promotion row is not an IPH, it never calls dismissed, so we
    // need to do it artificially here or we can trigger a DCHECK.
    tracker->Dismissed(feature_engagement::kEsbDownloadRowPromoFeature);
  }
  std::move(callback).Run(should_show_esb_promo);
}

void DownloadsDOMHandler::LogEsbPromotionRowViewed() {
  content::DownloadManager* manager = GetMainNotifierManager();
  if (!manager) {
    return;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          manager->GetBrowserContext());
  tracker->NotifyEvent("esb_download_promo_row_viewed");
  base::UmaHistogramEnumeration("SafeBrowsing.EsbDownloadRowPromo.Outcome",
                                SafeBrowsingEsbDownloadRowPromoOutcome::kShown);
}
#else
// These next three functions are empty implementations for the non-branded
// chromium build since the ESB download row promo only runs on branded
// google chrome.
void DownloadsDOMHandler::OpenEsbSettings() {
  return;
}

void DownloadsDOMHandler::IsEligibleForEsbPromo(
    IsEligibleForEsbPromoCallback callback) {
  std::move(callback).Run(false);
}

void DownloadsDOMHandler::LogEsbPromotionRowViewed() {
  return;
}
#endif

// DownloadsDOMHandler, private: --------------------------------------------

content::DownloadManager* DownloadsDOMHandler::GetMainNotifierManager() const {
  return list_tracker_.GetMainNotifierManager();
}

content::DownloadManager* DownloadsDOMHandler::GetOriginalNotifierManager()
    const {
  return list_tracker_.GetOriginalNotifierManager();
}

void DownloadsDOMHandler::FinalizeRemovals() {
  while (!removals_.empty()) {
    const IdSet remove = removals_.back();
    removals_.pop_back();

    for (const auto id : remove) {
      download::DownloadItem* download = GetDownloadById(id);
      if (download) {
        download->Remove();
      }
    }
  }
}

void DownloadsDOMHandler::MaybeTriggerDownloadWarningHatsSurvey(
    download::DownloadItem* item,
    DownloadWarningHatsType survey_type) {
  CHECK(CanShowDownloadWarningHatsSurvey(item));

  content::DownloadManager* manager = GetMainNotifierManager();
  Profile* profile = Profile::FromBrowserContext(manager->GetBrowserContext());
  if (!profile) {
    return;
  }

  auto psd = DownloadWarningHatsProductSpecificData::Create(survey_type, item);
  psd.AddNumPageWarnings(list_tracker_.NumDangerousItemsSent());

  MaybeLaunchDownloadWarningHatsSurvey(profile, psd);
}

void DownloadsDOMHandler::OnDownloadsPageDismissed() {
  // If the chrome://downloads page is closed as part of the browser shutting
  // down, do not run the HaTS survey because that would call into the network
  // stack and try to use objects that are already being torn down.
  if (browser_shutdown::HasShutdownStarted()) {
    return;
  }

  // There's no specific warning associated with navigating away from
  // chrome://downloads or closing the tab, so let's just launch the survey on
  // the topmost download with a warning.
  if (download::DownloadItem* first_dangerous_item =
          list_tracker_.GetFirstActiveWarningItem();
      first_dangerous_item &&
      CanShowDownloadWarningHatsSurvey(first_dangerous_item)) {
    MaybeTriggerDownloadWarningHatsSurvey(
        first_dangerous_item, DownloadWarningHatsType::kDownloadsPageIgnore);
  }
}

bool DownloadsDOMHandler::IsDeletingHistoryAllowed() {
  content::DownloadManager* manager = GetMainNotifierManager();
  return manager && Profile::FromBrowserContext(manager->GetBrowserContext())
                        ->GetPrefs()
                        ->GetBoolean(prefs::kAllowDeletingBrowserHistory);
}

download::DownloadItem* DownloadsDOMHandler::GetDownloadByStringId(
    const std::string& id) {
  uint64_t id_num;
  if (!base::StringToUint64(id, &id_num)) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  return GetDownloadById(static_cast<uint32_t>(id_num));
}

download::DownloadItem* DownloadsDOMHandler::GetDownloadById(uint32_t id) {
  download::DownloadItem* item = nullptr;
  if (GetMainNotifierManager()) {
    item = GetMainNotifierManager()->GetDownload(id);
  }
  if (!item && GetOriginalNotifierManager()) {
    item = GetOriginalNotifierManager()->GetDownload(id);
  }
  return item;
}

content::WebContents* DownloadsDOMHandler::GetWebUIWebContents() {
  return web_ui_->GetWebContents();
}

void DownloadsDOMHandler::CheckForRemovedFiles() {
  if (GetMainNotifierManager()) {
    GetMainNotifierManager()->CheckForHistoryFilesRemoval();
  }
  if (GetOriginalNotifierManager()) {
    GetOriginalNotifierManager()->CheckForHistoryFilesRemoval();
  }
}

void DownloadsDOMHandler::RemoveDownloadInArgs(const std::string& id) {
  download::DownloadItem* file = GetDownloadByStringId(id);
  if (!file) {
    return;
  }

  DownloadVector downloads;
  downloads.push_back(file);
  RemoveDownloads(downloads);
}
