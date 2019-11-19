// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_
#define COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/pdf/common/pdf.mojom.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/pdf_accessibility_shared.h"

namespace blink {
class WebLocalFrame;
}

namespace content {
class RenderFrame;
class RendererPpapiHost;
}

namespace ppapi {
namespace host {
struct HostMessageContext;
}  // namespace host
}  // namespace ppapi

namespace pdf {

class PdfAccessibilityTree;

class PepperPDFHost : public ppapi::host::ResourceHost,
                      public mojom::PdfListener {
 public:
  class PrintClient {
   public:
    virtual ~PrintClient() {}

    // Returns whether printing is enabled for the plugin instance identified by
    // |instance_id|.
    virtual bool IsPrintingEnabled(PP_Instance instance_id) = 0;

    // Invokes the "Print" command for the plugin instance identified by
    // |instance_id|. Returns whether the "Print" command was issued or not.
    virtual bool Print(PP_Instance instance_id) = 0;
  };

  PepperPDFHost(content::RendererPpapiHost* host,
                PP_Instance instance,
                PP_Resource resource);
  ~PepperPDFHost() override;

  // Invokes the "Print" command for the given instance as if the user right
  // clicked on it and selected "Print". Returns if the "Print" command was
  // issued or not.
  static bool InvokePrintingForInstance(PP_Instance instance);

  // The caller retains the ownership of |print_client|. The client is
  // allowed to be set only once, and when set, the client must outlive the
  // PPB_PDF_Impl instance.
  static void SetPrintClient(PrintClient* print_client);

  // ppapi::host::ResourceHost:
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // mojom::PdfListener
  void SetCaretPosition(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SetSelectionBounds(const gfx::PointF& base,
                          const gfx::PointF& extent) override;

 private:
  int32_t OnHostMsgDidStartLoading(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgDidStopLoading(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSetContentRestriction(
      ppapi::host::HostMessageContext* context,
      int restrictions);
  int32_t OnHostMsgUserMetricsRecordAction(
      ppapi::host::HostMessageContext* context,
      const std::string& action);
  int32_t OnHostMsgHasUnsupportedFeature(
      ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgPrint(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgSaveAs(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgShowAlertDialog(ppapi::host::HostMessageContext* context,
                                   const std::string& message);
  int32_t OnHostMsgShowConfirmDialog(ppapi::host::HostMessageContext* context,
                                     const std::string& message);
  int32_t OnHostMsgShowPromptDialog(ppapi::host::HostMessageContext* context,
                                    const std::string& message,
                                    const std::string& default_answer);
  int32_t OnHostMsgSetSelectedText(ppapi::host::HostMessageContext* context,
                                   const base::string16& selected_text);
  int32_t OnHostMsgSetLinkUnderCursor(ppapi::host::HostMessageContext* context,
                                      const std::string& url);
  int32_t OnHostMsgSetAccessibilityViewportInfo(
      ppapi::host::HostMessageContext* context,
      const PP_PrivateAccessibilityViewportInfo& viewport_info);
  int32_t OnHostMsgSetAccessibilityDocInfo(
      ppapi::host::HostMessageContext* context,
      const PP_PrivateAccessibilityDocInfo& doc_info);
  int32_t OnHostMsgSetAccessibilityPageInfo(
      ppapi::host::HostMessageContext* context,
      const PP_PrivateAccessibilityPageInfo& page_info,
      const std::vector<ppapi::PdfAccessibilityTextRunInfo>& text_runs,
      const std::vector<PP_PrivateAccessibilityCharInfo>& chars,
      const ppapi::PdfAccessibilityPageObjects& page_objects);
  int32_t OnHostMsgSelectionChanged(ppapi::host::HostMessageContext* context,
                                    const PP_FloatPoint& left,
                                    int32_t left_height,
                                    const PP_FloatPoint& right,
                                    int32_t right_height);
  int32_t OnHostMsgSetPluginCanSave(ppapi::host::HostMessageContext* context,
                                    bool can_save);

  void CreatePdfAccessibilityTreeIfNeeded();

  content::RenderFrame* GetRenderFrame();
  mojom::PdfService* GetRemotePdfService();
  blink::WebLocalFrame* GetWebLocalFrame();

  std::unique_ptr<PdfAccessibilityTree> pdf_accessibility_tree_;
  content::RendererPpapiHost* const host_;
  mojo::AssociatedRemote<mojom::PdfService> remote_pdf_service_;
  mojo::Receiver<mojom::PdfListener> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperPDFHost);
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PEPPER_PDF_HOST_H_
