// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_TOOLS_TOOLS_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_TOOLS_TOOLS_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.mojom.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

class BrowserWindowInterface;

namespace ttc {

class PageContextMonitor;

class AiOverlayTools : public ai_overlay_dialog::mojom::AiOverlayTools {
 public:
  AiOverlayTools(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
      BrowserWindowInterface* browser,
      PageContextMonitor* page_context_monitor);
  ~AiOverlayTools() override;
  AiOverlayTools(const AiOverlayTools&) = delete;
  AiOverlayTools& operator=(const AiOverlayTools&) = delete;

  // ai_overlay_dialog::mojom::AiOverlayTools:
  void OpenUrl(const std::string& url,
               bool new_tab,
               OpenUrlCallback callback) override;
  void FollowLink(const std::string& id, FollowLinkCallback callback) override;
  void PerformSearch(const std::string& query,
                     bool new_tab,
                     PerformSearchCallback callback) override;
  void SwitchTab(const std::string& query, SwitchTabCallback callback) override;
  void CloseCurrentTab(CloseCurrentTabCallback callback) override;
  void GoBack(GoBackCallback callback) override;
  void GoForward(GoForwardCallback callback) override;
  void ReloadPage(ReloadPageCallback callback) override;
  void FindAndHighlight(const std::string& query,
                        FindAndHighlightCallback callback) override;
  void Scroll(ai_overlay_dialog::mojom::ScrollGranularity granularity,
              double magnitude,
              ScrollCallback callback) override;
  void PlayVideo(PlayVideoCallback callback) override;
  void PauseVideo(PauseVideoCallback callback) override;
  void SeekToTimestamp(const std::string& timecode,
                       SeekToTimestampCallback callback) override;
  void TranslatePage(const std::string& target_language,
                     TranslatePageCallback callback) override;
  void InvokeGlic(const std::string& prompt,
                  InvokeGlicCallback callback) override;

 private:
  class AnnotationTask : public blink::mojom::AnnotationAgentHost {
   public:
    AnnotationTask(
        mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
        mojo::Remote<blink::mojom::AnnotationAgent> agent_remote,
        FindAndHighlightCallback callback);
    AnnotationTask(const AnnotationTask&) = delete;
    AnnotationTask& operator=(const AnnotationTask&) = delete;
    ~AnnotationTask() override;

    void DidFinishAttachment(
        const gfx::Rect& document_relative_rect,
        blink::mojom::AttachmentResult attachment_result) override;

   private:
    mojo::Receiver<blink::mojom::AnnotationAgentHost> receiver_;
    mojo::Remote<blink::mojom::AnnotationAgent> agent_remote_;
    FindAndHighlightCallback callback_;
  };

  void OnAnnotationAgentDisconnected();

  mojo::Receiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver_;
  raw_ptr<BrowserWindowInterface> browser_;
  // `page_context_monitor_` is owned by `AiOverlayDialogUntrustedUI` and must
  // outlive `AiOverlayTools`.
  raw_ptr<PageContextMonitor> page_context_monitor_;

  std::unique_ptr<AnnotationTask> annotation_task_;
  content::WeakDocumentPtr annotation_document_;
  mojo::Remote<blink::mojom::AnnotationAgentContainer> annotation_container_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_TOOLS_TOOLS_H_
