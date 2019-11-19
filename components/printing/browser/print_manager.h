// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

#if defined(OS_ANDROID)
#include "base/callback.h"
#endif

namespace IPC {
class Message;
}

struct PrintHostMsg_DidPrintDocument_Params;
struct PrintHostMsg_ScriptedPrint_Params;

namespace printing {

class PrintManager : public content::WebContentsObserver {
 public:
  ~PrintManager() override;

#if defined(OS_ANDROID)
  // TODO(timvolodine): consider introducing PrintManagerAndroid (crbug/500960)
  using PdfWritingDoneCallback =
      base::RepeatingCallback<void(int /* page count */)>;

  virtual void PdfWritingDone(int page_count) = 0;
#endif

 protected:
  explicit PrintManager(content::WebContents* contents);

  // Helper method to fetch the PrintRenderFrame associated remote interface
  // pointer.
  const mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>&
  GetPrintRenderFrame(content::RenderFrameHost* rfh);

  // Terminates or cancels the print job if one was pending.
  void PrintingRenderFrameDeleted();

  // content::WebContentsObserver
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // IPC handling support
  struct FrameDispatchHelper;

  // IPC message PrintHostMsg_DidPrintDocument can require handling in other
  // processes beyond the rendering process running OnMessageReceived(),
  // requiring that the renderer needs to wait.
  class DelayedFrameDispatchHelper {
   public:
    DelayedFrameDispatchHelper(content::RenderFrameHost* render_frame_host,
                               IPC::Message* reply_msg);
    DelayedFrameDispatchHelper(const DelayedFrameDispatchHelper&) = delete;
    ~DelayedFrameDispatchHelper();
    DelayedFrameDispatchHelper& operator=(const DelayedFrameDispatchHelper&) =
        delete;

    // SendCompleted() can be called at most once, since it provides the success
    // reply for a message. A failure reply for the message is automatically
    // sent if this is never called.
    void SendCompleted();

   private:
    content::RenderFrameHost* const render_frame_host_;
    IPC::Message* reply_msg_;
  };

  // IPC handlers
  virtual void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  virtual void OnDidPrintDocument(
      content::RenderFrameHost* render_frame_host,
      const PrintHostMsg_DidPrintDocument_Params& params,
      std::unique_ptr<DelayedFrameDispatchHelper> helper) = 0;
  virtual void OnGetDefaultPrintSettings(
      content::RenderFrameHost* render_frame_host,
      IPC::Message* reply_msg) = 0;
  virtual void OnPrintingFailed(int cookie);
  virtual void OnScriptedPrint(content::RenderFrameHost* render_frame_host,
                               const PrintHostMsg_ScriptedPrint_Params& params,
                               IPC::Message* reply_msg) = 0;

  int number_pages_ = 0;  // Number of pages to print in the print job.
  int cookie_ = 0;        // The current document cookie.

#if defined(OS_ANDROID)
  // Callback to execute when done writing pdf.
  PdfWritingDoneCallback pdf_writing_done_callback_;
#endif

 private:
  void OnDidGetDocumentCookie(int cookie);

  // Stores a PrintRenderFrame associated remote with the RenderFrameHost used
  // to bind it. The PrintRenderFrame is used to transmit mojo interface method
  // calls to the associated receiver.
  std::map<content::RenderFrameHost*,
           mojo::AssociatedRemote<printing::mojom::PrintRenderFrame>>
      print_render_frames_;

  DISALLOW_COPY_AND_ASSIGN(PrintManager);
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_MANAGER_H_
