// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_H_
#define COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/buildflags.h"
#include "pdf/mojom/pdf.mojom.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "ui/gfx/geometry/point_f.h"
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

  static PDFDocumentHelper* MaybeGetForWebContents(
      content::WebContents* contents);

  // content::RenderWidgetHostObserver:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  // ui::TouchSelectionControllerClient:
  bool SupportsAnimation() const override;
  void SetNeedsAnimate() override;
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
  void OnDocumentLoadComplete() override;
  void SaveUrlAs(const GURL& url,
                 network::mojom::ReferrerPolicy policy) override;
  void UpdateContentRestrictions(int32_t content_restrictions) override;
  void SelectionChanged(const gfx::PointF& left,
                        int32_t left_height,
                        const gfx::PointF& right,
                        int32_t right_height) override;
  void SetPluginCanSave(bool can_save) override;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void OnSearchifyStarted() override;
#endif

  // Returns whether document is loaded, at which point, the other calls to
  // document metadata such as `GetPdfBytes`, `GetPageText` can return data.
  bool IsDocumentLoadComplete() const { return is_document_load_complete_; }

  // Get PDF bytes, if they do not exceed the size limit. If called before
  // document is loaded, the callback will be invoked with an empty vector.
  void GetPdfBytes(uint32_t size_limit,
                   pdf::mojom::PdfListener::GetPdfBytesCallback callback);

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  void GetSaveDataBufferHandlerForDrive(
      pdf::mojom::SaveRequestType request_type,
      pdf::mojom::PdfListener::GetSaveDataBufferHandlerForDriveCallback
          callback);
#endif

  // Returns text of the given page. If called before document is loaded, the
  // callback will be invoked with an empty string.
  void GetPageText(int32_t page_index,
                   pdf::mojom::PdfListener::GetPageTextCallback callback);

  void GetMostVisiblePageIndex(
      pdf::mojom::PdfListener::GetMostVisiblePageIndexCallback callback);

  // Registers `callback` to be run when document load completes successfully.
  // When the PDF is already loaded, `callback` is invoked immediately. Will not
  // be invoked when the load fails. This is useful to wait for document
  // metadata to be loaded, before calls to `GetPdfBytes()`, and `GetPageText()`
  // should be made.
  //
  // This `callback` will run before
  // `PDFDocumentHelperClient::OnDocumentLoadComplete()`.
  void RegisterForDocumentLoadComplete(base::OnceClosure callback);

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Returns whether document is searchified.
  bool SearchifyStarted() const { return searchify_started_; }
#endif

 private:
  friend class content::DocumentUserData<PDFDocumentHelper>;

  PDFDocumentHelper(content::RenderFrameHost* rfh,
                    std::unique_ptr<PDFDocumentHelperClient> client);

  content::WebContents& GetWebContents();
  void InitTouchSelectionClientManager();
  gfx::PointF ConvertFromRoot(const gfx::PointF& point);
  gfx::PointF ConvertToRoot(const gfx::PointF& point);
  gfx::PointF ConvertHelper(const gfx::PointF& point, float scale);

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

  bool is_document_load_complete_ = false;

  // Callbacks to invoke when document load is completed.
  std::vector<base::OnceClosure> document_load_complete_callbacks_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  bool searchify_started_ = false;
#endif

  mojo::Remote<mojom::PdfListener> remote_pdf_client_;

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_DOCUMENT_HELPER_H_
