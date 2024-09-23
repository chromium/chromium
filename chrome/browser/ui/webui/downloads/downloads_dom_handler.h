// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_DOM_HANDLER_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/download/download_danger_prompt.h"
#include "chrome/browser/download/download_warning_desktop_hats_utils.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom-forward.h"
#include "chrome/browser/ui/webui/downloads/downloads_list_tracker.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class DownloadManager;
class WebContents;
class WebUI;
}

namespace download {
class DownloadItem;
}

// Represents the possible outcomes of showing a ESB download row promotion.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SafeBrowsingEsbDownloadRowPromoOutcome)
enum class SafeBrowsingEsbDownloadRowPromoOutcome {
  // The kShown and kClicked values are not meant to be mutually exclusive,
  // the same promo row can be shown AND clicked.
  kShown = 0,
  kClicked = 1,
  kMaxValue = kClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/safe_browsing/enums.xml:SafeBrowsingEsbDownloadRowPromoOutcome)

// Represents the possible actions a user can take on chrome://downloads from
// the dangerous download interstitial.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DangerousDownloadInterstitialAction)
enum class DangerousDownloadInterstitialAction {
  kOpenInterstitial = 0,
  kCancelInterstitial = 1,
  kOpenSurvey = 2,
  kSaveDangerous = 3,
  kMaxValue = kSaveDangerous
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/download/enums.xml:DangerousDownloadInterstitialAction)

// Represents the possible actions a user can take on the chrome://downloads
// dangerous download interstitial that trigger UMA logging of the latency
// between opening the interstitial and performing the action.
enum class DangerousDownloadInterstitialInteraction {
  // Latency between opening and closing the interstitial.
  kCancelInterstitial,
  // Latency between opening the interstitial and opening the survey.
  kOpenSurvey,
  // Latency between opening the survey and saving the dangerous file.
  kCompleteSurvey,
  // Latency between opening the survey and saving the dangerous file.
  kSaveDangerous
};

// The handler for Javascript messages related to the "downloads" view,
// also observes changes to the download manager.
// TODO(calamity): Remove WebUIMessageHandler.
class DownloadsDOMHandler : public content::WebContentsObserver,
                            public downloads::mojom::PageHandler {
 public:
  DownloadsDOMHandler(
      mojo::PendingReceiver<downloads::mojom::PageHandler> receiver,
      mojo::PendingRemote<downloads::mojom::Page> page,
      content::DownloadManager* download_manager,
      content::WebUI* web_ui);

  DownloadsDOMHandler(const DownloadsDOMHandler&) = delete;
  DownloadsDOMHandler& operator=(const DownloadsDOMHandler&) = delete;

  ~DownloadsDOMHandler() override;

  // WebContentsObserver implementation.
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // downloads::mojom::PageHandler:
  void GetDownloads(const std::vector<std::string>& search_terms) override;
  void OpenFileRequiringGesture(const std::string& id) override;
  void Drag(const std::string& id) override;
  void SaveSuspiciousRequiringGesture(const std::string& id) override;
  void RecordOpenBypassWarningDialog(const std::string& id) override;
  void RecordOpenBypassWarningInterstitial(const std::string& id) override;
  void RecordOpenSurveyOnDangerousInterstitial(const std::string& id) override;
  void SaveDangerousFromDialogRequiringGesture(const std::string& id) override;
  void SaveDangerousFromInterstitialNeedGesture(
      const std::string& id,
      downloads::mojom::DangerousDownloadInterstitialSurveyOptions) override;
  void RecordCancelBypassWarningDialog(const std::string& id) override;
  void RecordCancelBypassWarningInterstitial(const std::string& id) override;
  void DiscardDangerous(const std::string& id) override;
  void RetryDownload(const std::string& id) override;
  void Show(const std::string& id) override;
  void Pause(const std::string& id) override;
  void Resume(const std::string& id) override;
  void Remove(const std::string& id) override;
  void Undo() override;
  void Cancel(const std::string& id) override;
  void ClearAll() override;
  void OpenDownloadsFolderRequiringGesture() override;
  void OpenDuringScanningRequiringGesture(const std::string& id) override;
  void ReviewDangerousRequiringGesture(const std::string& id) override;
  void DeepScan(const std::string& id) override;
  void BypassDeepScanRequiringGesture(const std::string& id) override;
  void OpenEsbSettings() override;
  void IsEligibleForEsbPromo(IsEligibleForEsbPromoCallback callback) override;
  void LogEsbPromotionRowViewed() override;

 protected:
  // These methods are for mocking so that most of this class does not actually
  // depend on WebUI. The other methods that depend on WebUI are
  // RegisterMessages() and HandleDrag().
  virtual content::WebContents* GetWebUIWebContents();

  // Actually remove downloads with an ID in |removals_|. This cannot be undone.
  void FinalizeRemovals();

  using DownloadVector =
      std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>;

  // Remove all downloads in |to_remove|. Safe downloads can be revived,
  // dangerous ones are immediately removed. Protected for testing.
  void RemoveDownloads(const DownloadVector& to_remove);

 private:
  using IdSet = std::set<uint32_t>;

  // Convenience method to call |main_notifier_->GetManager()| while
  // null-checking |main_notifier_|.
  content::DownloadManager* GetMainNotifierManager() const;

  // Convenience method to call |original_notifier_->GetManager()| while
  // null-checking |original_notifier_|.
  content::DownloadManager* GetOriginalNotifierManager() const;

  // Launches a HaTS survey for a download warning that is heeded, bypassed, or
  // ignored (if all preconditions are met).
  void MaybeTriggerDownloadWarningHatsSurvey(
      download::DownloadItem* item,
      DownloadWarningHatsType survey_type);

  // Called when the downloads page is dismissed by closing the tab, or
  // navigating the tab to another page.
  void OnDownloadsPageDismissed();

  // Returns true if the records of any downloaded items are allowed (and able)
  // to be deleted.
  bool IsDeletingHistoryAllowed();

  // Returns the download that is referred to by a given string |id|.
  download::DownloadItem* GetDownloadByStringId(const std::string& id);

  // Returns the download with |id| or NULL if it doesn't exist.
  download::DownloadItem* GetDownloadById(uint32_t id);

  // Removes the download specified by an ID from JavaScript in |args|.
  void RemoveDownloadInArgs(const std::string& id);

  // Checks whether a download's file was removed from its original location.
  void CheckForRemovedFiles();

  DownloadsListTracker list_tracker_;

  // Used for logging UMA metrics.
  std::optional<base::TimeTicks> interstitial_open_time_;
  std::optional<base::TimeTicks> interstitial_survey_open_time_;

  // IDs of downloads to remove when this handler gets deleted.
  std::vector<IdSet> removals_;

  // Whether the render process has gone.
  bool render_process_gone_ = false;

  raw_ptr<content::WebUI> web_ui_;

  mojo::Receiver<downloads::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<DownloadsDOMHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_DOM_HANDLER_H_
