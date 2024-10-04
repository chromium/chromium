// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_H_
#define COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/mojom/pdf.mojom.h"
#include "ui/touch_selection/selection_event_type.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/touch_selection/touch_selection_menu_runner.h"

namespace content {
class RenderWidgetHost;
class WebContents;
}  // namespace content

namespace pdf {

class PDFDocumentHelperClient;

// Per-Document class to handle PDF messages.
class PDFDocumentHelper
    : public content::DocumentUserData<PDFDocumentHelper>,
      public content::RenderWidgetHostObserver,
      public mojom::PdfHost,
      public ui::TouchSelectionControllerClient,
      public ui::TouchSelectionMenuClient,
      public content::TouchSelectionControllerClientManager::Observer {
 public:
  PDFDocumentHelper(const PDFDocumentHelper&) = delete;
  PDFDocumentHelper& operator=(const PDFDocumentHelper&) = delete;

  ~PDFDocumentHelper() override;

  static void BindPdfHost(
      mojo::PendingAssociatedReceiver<mojom::PdfHost> pdf_host,
      content::RenderFrameHost* rfh,
      std::unique_ptr<PDFDocumentHelperClient> client);

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  // ui::TouchSelectionControllerClient:
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override {}
  void MoveCaret(const gfx::PointF& position) override;
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override;
  void SelectBetweenCoordinates(const gfx::PointF& base,
                                const gfx::PointF& extent) override;
  void OnSelectionEvent(ui::SelectionEventType event) override;
  void OnDragUpdate(const ui::TouchSelectionDraggable::Type type,
                    const gfx::PointF& position) override;
  std::unique_ptr<ui::TouchHandleDrawable> CreateDrawable() override;
  void DidScroll() override;

  // ui::TouchSelectionMenuClient:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void RunContextMenu() override;
  bool ShouldShowQuickMenu() override;
  std::u16string GetSelectedText() override;

  // content::TouchSelectionControllerClientManager::Observer:
  void OnManagerWillDestroy(
      content::TouchSelectionControllerClientManager* manager) override;

  // pdf::mojom::PdfHost:
  void SetListener(mojo::PendingRemote<mojom::PdfListener> listener) override;
  void HasUnsupportedFeature() override;
  void SaveUrlAs(const GURL& url,
                 network::mojom::ReferrerPolicy policy) override;
  void UpdateContentRestrictions(int32_t content_restrictions) override;
  void SelectionChanged(const gfx::PointF& left,
                        int32_t left_height,
                        const gfx::PointF& right,
                        int32_t right_height) override;
  void SetPluginCanSave(bool can_save) override;

  void GetPdfBytes(uint32_t size_limit,
                   pdf::mojom::PdfListener::GetPdfBytesCallback callback);

 private:
  friend class content::DocumentUserData<PDFDocumentHelper>;

  PDFDocumentHelper(content::RenderFrameHost* rfh,
                    std::unique_ptr<PDFDocumentHelperClient> client);

  content::WebContents& GetWebContents();
  void InitTouchSelectionClientManager();
  gfx::PointF ConvertFromRoot(const gfx::PointF& point_f);
  gfx::PointF ConvertToRoot(const gfx::PointF& point_f);
  gfx::PointF ConvertHelper(const gfx::PointF& point_f, float scale);

  content::RenderFrameHostReceiverSet<mojom::PdfHost> pdf_host_receivers_;
  std::unique_ptr<PDFDocumentHelperClient> const client_;
  raw_ptr<content::TouchSelectionControllerClientManager>
      touch_selection_controller_client_manager_ = nullptr;

  // The `RenderWidgetHost` associated to the frame containing the PDF plugin.
  // This should be null until the plugin is known to have been created; the
  // signal comes from `SetListener()`.
  raw_ptr<content::RenderWidgetHost> pdf_rwh_ = nullptr;

  // Latest selection bounds received from PDFium.
  gfx::PointF selection_left_;
  int32_t selection_left_height_ = 0;
  gfx::PointF selection_right_;
  int32_t selection_right_height_ = 0;
  bool has_selection_ = false;

  mojo::Remote<mojom::PdfListener> remote_pdf_client_;

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_H_
