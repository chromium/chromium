// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_DOM_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_DOM_HANDLER_H_

#include <stdint.h>

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_danger_prompt.h"
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
  ~DownloadsDOMHandler() override;

  // WebContentsObserver implementation.
  void RenderProcessGone(base::TerminationStatus status) override;

  // downloads::mojom::PageHandler:
  void GetDownloads(const std::vector<std::string>& search_terms) override;
  void OpenFileRequiringGesture(const std::string& id) override;
  void Drag(const std::string& id) override;
  void SaveDangerousRequiringGesture(const std::string& id) override;
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

 protected:
  // These methods are for mocking so that most of this class does not actually
  // depend on WebUI. The other methods that depend on WebUI are
  // RegisterMessages() and HandleDrag().
  virtual content::WebContents* GetWebUIWebContents();

  // Actually remove downloads with an ID in |removals_|. This cannot be undone.
  void FinalizeRemovals();

  using DownloadVector = std::vector<download::DownloadItem*>;

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

  // Displays a native prompt asking the user for confirmation after accepting
  // the dangerous download specified by |dangerous|. The function returns
  // immediately, and will invoke DangerPromptAccepted() asynchronously if the
  // user accepts the dangerous download. The native prompt will observe
  // |dangerous| until either the dialog is dismissed or |dangerous| is no
  // longer an in-progress dangerous download.
  virtual void ShowDangerPrompt(download::DownloadItem* dangerous);

  // Conveys danger acceptance from the DownloadDangerPrompt to the
  // DownloadItem.
  void DangerPromptDone(int download_id, DownloadDangerPrompt::Action action);

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

  // IDs of downloads to remove when this handler gets deleted.
  std::vector<IdSet> removals_;

  // Whether the render process has gone.
  bool render_process_gone_ = false;

  content::WebUI* web_ui_;

  mojo::Receiver<downloads::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<DownloadsDOMHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadsDOMHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_DOWNLOADS_DOM_HANDLER_H_
