// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog.mojom.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog_tools.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "url/gurl.h"

class BrowserWindowInterface;

class AiOverlayDialogPageHandler : public ai_overlay_dialog::mojom::PageHandler,
                                   public AiOverlayDialogTools {
 public:
  AiOverlayDialogPageHandler(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
      mojo::PendingRemote<ai_overlay_dialog::mojom::Page> remote,
      BrowserWindowInterface* browser);
  ~AiOverlayDialogPageHandler() override;

  // overlay_dialog::mojom::PageHandler interface
  void GetMockAudioData(GetMockAudioDataCallback callback) override;
  void GetToolDefinitions(GetToolDefinitionsCallback callback) override;
  void ExecuteTool(const std::string& name,
                   const std::string& json_args,
                   ExecuteToolCallback callback) override;

  void DidChangePage(const GURL& url,
                     const std::optional<std::u16string>& title,
                     const std::optional<std::string>& content);
  void UpdateCurrentPageContext(const std::u16string& title,
                                const std::string& content);

  // AiOverlayDialogTools:
  void OpenUrl(const std::string& url,
               bool new_tab,
               base::OnceCallback<void(ToolResult)> callback) override;
  void PerformSearch(const std::string& query,
                     bool new_tab,
                     base::OnceCallback<void(ToolResult)> callback) override;
  void SwitchTab(const std::string& query,
                 base::OnceCallback<void(ToolResult)> callback) override;
  void CloseCurrentTab(base::OnceCallback<void(ToolResult)> callback) override;
  void GoBack(base::OnceCallback<void(ToolResult)> callback) override;
  void GoForward(base::OnceCallback<void(ToolResult)> callback) override;
  void ReloadPage(base::OnceCallback<void(ToolResult)> callback) override;
  void FindAndHighlight(const std::string& query,
                        base::OnceCallback<void(ToolResult)> callback) override;
  void Scroll(const std::string& direction,
              double magnitude,
              base::OnceCallback<void(ToolResult)> callback) override;
  void PlayVideo(base::OnceCallback<void(ToolResult)> callback) override;
  void PauseVideo(base::OnceCallback<void(ToolResult)> callback) override;
  void SeekToTimestamp(const std::string& timecode,
                       base::OnceCallback<void(ToolResult)> callback) override;

 private:
  class AnnotationTask : public blink::mojom::AnnotationAgentHost {
   public:
    AnnotationTask(
        mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
        mojo::Remote<blink::mojom::AnnotationAgent> agent_remote,
        base::OnceCallback<void(ToolResult)> callback);
    AnnotationTask(const AnnotationTask&) = delete;
    AnnotationTask& operator=(const AnnotationTask&) = delete;
    ~AnnotationTask() override;

    void DidFinishAttachment(
        const gfx::Rect& document_relative_rect,
        blink::mojom::AttachmentResult attachment_result) override;

   private:
    mojo::Receiver<blink::mojom::AnnotationAgentHost> receiver_;
    mojo::Remote<blink::mojom::AnnotationAgent> agent_remote_;
    base::OnceCallback<void(ToolResult)> callback_;
  };

  void OnAnnotationAgentDisconnected();

  mojo::Receiver<ai_overlay_dialog::mojom::PageHandler> receiver_;
  std::unique_ptr<AnnotationTask> annotation_task_;
  content::WeakDocumentPtr annotation_document_;
  mojo::Remote<blink::mojom::AnnotationAgentContainer> annotation_container_;
  mojo::Remote<ai_overlay_dialog::mojom::Page> page_;
  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_PAGE_HANDLER_H_
