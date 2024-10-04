// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_document_helper.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "components/pdf/browser/pdf_frame_util.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer_type_converters.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/pdf_features.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"

namespace pdf {

// static
void PDFDocumentHelper::BindPdfHost(
    mojo::PendingAssociatedReceiver<mojom::PdfHost> pdf_host,
    content::RenderFrameHost* rfh,
    std::unique_ptr<PDFDocumentHelperClient> client) {
  auto* pdf_helper = PDFDocumentHelper::GetForCurrentDocument(rfh);
  if (!pdf_helper) {
    PDFDocumentHelper::CreateForCurrentDocument(rfh, std::move(client));
    pdf_helper = PDFDocumentHelper::GetForCurrentDocument(rfh);
  }
  pdf_helper->pdf_host_receivers_.Bind(rfh, std::move(pdf_host));
}

PDFDocumentHelper::PDFDocumentHelper(
    content::RenderFrameHost* rfh,
    std::unique_ptr<PDFDocumentHelperClient> client)
    : content::DocumentUserData<PDFDocumentHelper>(rfh),
      pdf_host_receivers_(content::WebContents::FromRenderFrameHost(rfh), this),
      client_(std::move(client)) {}

PDFDocumentHelper::~PDFDocumentHelper() {
  if (pdf_rwh_) {
    pdf_rwh_->RemoveObserver(this);
  }

  if (!touch_selection_controller_client_manager_) {
    return;
  }

  ui::TouchSelectionController* touch_selection_controller =
      touch_selection_controller_client_manager_->GetTouchSelectionController();
  touch_selection_controller->HideAndDisallowShowingAutomatically();

  touch_selection_controller_client_manager_->InvalidateClient(this);
  touch_selection_controller_client_manager_->RemoveObserver(this);
}

void PDFDocumentHelper::SetListener(
    mojo::PendingRemote<mojom::PdfListener> listener) {
  remote_pdf_client_.reset();
  remote_pdf_client_.Bind(std::move(listener));

  if (pdf_rwh_) {
    pdf_rwh_->RemoveObserver(this);
  }

  content::RenderFrameHost* pdf_host;
  if (chrome_pdf::features::IsOopifPdfEnabled()) {
    pdf_host = &render_frame_host();
  } else {
    content::RenderFrameHost* main_frame =
        GetWebContents().GetPrimaryMainFrame();
    content::RenderFrameHost* pdf_frame =
        pdf_frame_util::FindPdfChildFrame(main_frame);
    pdf_host = pdf_frame ? pdf_frame : main_frame;
  }

  pdf_rwh_ = pdf_host->GetRenderWidgetHost();
  pdf_rwh_->AddObserver(this);
}

gfx::PointF PDFDocumentHelper::ConvertHelper(const gfx::PointF& point_f,
                                             float scale) {
  if (!pdf_rwh_) {
    return point_f;
  }

  content::RenderWidgetHostView* view = pdf_rwh_->GetView();
  if (!view) {
    return point_f;
  }

  gfx::Vector2dF offset =
      view->TransformPointToRootCoordSpaceF(gfx::PointF()).OffsetFromOrigin();
  offset.Scale(scale);

  return point_f + offset;
}

gfx::PointF PDFDocumentHelper::ConvertFromRoot(const gfx::PointF& point_f) {
  return ConvertHelper(point_f, -1.f);
}

gfx::PointF PDFDocumentHelper::ConvertToRoot(const gfx::PointF& point_f) {
  return ConvertHelper(point_f, +1.f);
}

void PDFDocumentHelper::SelectionChanged(const gfx::PointF& left,
                                         int32_t left_height,
                                         const gfx::PointF& right,
                                         int32_t right_height) {
  selection_left_ = left;
  selection_left_height_ = left_height;
  selection_right_ = right;
  selection_right_height_ = right_height;

  DidScroll();
}

void PDFDocumentHelper::SetPluginCanSave(bool can_save) {
  client_->SetPluginCanSave(pdf_host_receivers_.GetCurrentTargetFrame(),
                            can_save);
}

void PDFDocumentHelper::DidScroll() {
  if (!touch_selection_controller_client_manager_) {
    InitTouchSelectionClientManager();
  }

  if (!touch_selection_controller_client_manager_) {
    return;
  }

  gfx::SelectionBound start;
  gfx::SelectionBound end;
  start.SetEdgeStart(ConvertToRoot(selection_left_));
  start.SetEdgeEnd(ConvertToRoot(gfx::PointF(
      selection_left_.x(), selection_left_.y() + selection_left_height_)));
  end.SetEdgeStart(ConvertToRoot(selection_right_));
  end.SetEdgeEnd(ConvertToRoot(gfx::PointF(
      selection_right_.x(), selection_right_.y() + selection_right_height_)));

  // TouchSelectionControllerClientAura needs these visible edges of selection
  // to show the quick menu and context menu. Set the visible edges by the
  // edges of |start| and |end|.
  start.SetVisibleEdge(start.edge_start(), start.edge_end());
  end.SetVisibleEdge(end.edge_start(), end.edge_end());

  // Don't do left/right comparison after setting type.
  // TODO(wjmaclean): When PDFium supports editing, we'll need to detect
  // start == end as *either* no selection, or an insertion point.
  has_selection_ = start != end;
  start.set_visible(has_selection_);
  end.set_visible(has_selection_);
  start.set_type(has_selection_ ? gfx::SelectionBound::LEFT
                                : gfx::SelectionBound::EMPTY);
  end.set_type(has_selection_ ? gfx::SelectionBound::RIGHT
                              : gfx::SelectionBound::EMPTY);

  touch_selection_controller_client_manager_->UpdateClientSelectionBounds(
      start, end, this, this);
  client_->OnDidScroll(start, end);
}

void PDFDocumentHelper::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  if (pdf_rwh_ == widget_host) {
    pdf_rwh_ = nullptr;
  }
}

bool PDFDocumentHelper::SupportsAnimation() const {
  return false;
}

void PDFDocumentHelper::MoveCaret(const gfx::PointF& position) {
  if (!remote_pdf_client_) {
    return;
  }
  remote_pdf_client_->SetCaretPosition(ConvertFromRoot(position));
}

void PDFDocumentHelper::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  if (!remote_pdf_client_) {
    return;
  }
  remote_pdf_client_->MoveRangeSelectionExtent(ConvertFromRoot(extent));
}

void PDFDocumentHelper::SelectBetweenCoordinates(const gfx::PointF& base,
                                                 const gfx::PointF& extent) {
  if (!remote_pdf_client_) {
    return;
  }
  remote_pdf_client_->SetSelectionBounds(ConvertFromRoot(base),
                                         ConvertFromRoot(extent));
}

void PDFDocumentHelper::GetPdfBytes(
    uint32_t size_limit,
    pdf::mojom::PdfListener::GetPdfBytesCallback callback) {
  if (!remote_pdf_client_) {
    std::move(callback).Run(
        pdf::mojom::PdfListener::GetPdfBytesStatus::kFailed, {});
    return;
  }
  remote_pdf_client_->GetPdfBytes(size_limit, std::move(callback));
}

void PDFDocumentHelper::OnSelectionEvent(ui::SelectionEventType event) {
  // Should be handled by `TouchSelectionControllerClientAura`.
  NOTREACHED_IN_MIGRATION();
}

void PDFDocumentHelper::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  // Should be handled by `TouchSelectionControllerClientAura`.
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<ui::TouchHandleDrawable> PDFDocumentHelper::CreateDrawable() {
  // We can return null here, as the manager will look after this.
  return nullptr;
}

void PDFDocumentHelper::OnManagerWillDestroy(
    content::TouchSelectionControllerClientManager* manager) {
  DCHECK_EQ(touch_selection_controller_client_manager_, manager);
  manager->RemoveObserver(this);
  touch_selection_controller_client_manager_ = nullptr;
}

bool PDFDocumentHelper::IsCommandIdEnabled(int command_id) const {
  // TODO(wjmaclean|dsinclair): Make PDFium send readability information in the
  // selection changed message?
  bool readable = true;

  switch (command_id) {
    case ui::TouchEditable::kCopy:
      return readable && has_selection_;
      // TODO(wjmaclean): add logic for cut/paste as the information required
      // from PDFium becomes available.
  }
  return false;
}

void PDFDocumentHelper::ExecuteCommand(int command_id, int event_flags) {
  // TODO(wjmaclean, dsinclair): Need to communicate to PDFium to accept
  // cut/paste commands.
  switch (command_id) {
    case ui::TouchEditable::kCopy:
      GetWebContents().Copy();
      break;
  }
}

void PDFDocumentHelper::RunContextMenu() {
  content::RenderFrameHost* focused_frame;
  if (chrome_pdf::features::IsOopifPdfEnabled()) {
    focused_frame = &render_frame_host();
  } else {
    focused_frame = GetWebContents().GetFocusedFrame();
    if (!focused_frame) {
      return;
    }
  }

  content::RenderWidgetHost* widget = focused_frame->GetRenderWidgetHost();
  if (!widget || !widget->GetView()) {
    return;
  }

  if (!touch_selection_controller_client_manager_) {
    InitTouchSelectionClientManager();
  }

  if (!touch_selection_controller_client_manager_) {
    return;
  }

  ui::TouchSelectionController* touch_selection_controller =
      touch_selection_controller_client_manager_->GetTouchSelectionController();
  gfx::RectF anchor_rect =
      touch_selection_controller->GetVisibleRectBetweenBounds();
  gfx::PointF anchor_point =
      gfx::PointF(anchor_rect.CenterPoint().x(), anchor_rect.y());

  gfx::PointF origin =
      widget->GetView()->TransformPointToRootCoordSpaceF(gfx::PointF());
  anchor_point.Offset(-origin.x(), -origin.y());
  widget->ShowContextMenuAtPoint(gfx::ToRoundedPoint(anchor_point),
                                 ui::MENU_SOURCE_TOUCH_EDIT_MENU);

  // Hide selection handles after getting rect-between-bounds from touch
  // selection controller; otherwise, rect would be empty and the above
  // calculations would be invalid.
  touch_selection_controller->HideAndDisallowShowingAutomatically();
}

bool PDFDocumentHelper::ShouldShowQuickMenu() {
  return false;
}

std::u16string PDFDocumentHelper::GetSelectedText() {
  return std::u16string();
}

content::WebContents& PDFDocumentHelper::GetWebContents() {
  return *content::WebContents::FromRenderFrameHost(&render_frame_host());
}

void PDFDocumentHelper::InitTouchSelectionClientManager() {
  content::RenderWidgetHostView* view =
      GetWebContents().GetRenderWidgetHostView();
  if (!view) {
    return;
  }

  touch_selection_controller_client_manager_ =
      view->GetTouchSelectionControllerClientManager();
  if (!touch_selection_controller_client_manager_) {
    return;
  }

  touch_selection_controller_client_manager_->AddObserver(this);
}

void PDFDocumentHelper::HasUnsupportedFeature() {
  client_->OnPDFHasUnsupportedFeature(&GetWebContents());
}

void PDFDocumentHelper::SaveUrlAs(const GURL& url,
                                  network::mojom::ReferrerPolicy policy) {
  client_->OnSaveURL(&GetWebContents());

  // Save using the PDF embedder host.
  content::RenderFrameHost* rfh =
      chrome_pdf::features::IsOopifPdfEnabled()
          ? pdf_frame_util::GetEmbedderHost(&render_frame_host())
          : GetWebContents().GetOuterWebContentsFrame();
  if (!rfh) {
    return;
  }

  content::Referrer referrer(url, policy);
  referrer = content::Referrer::SanitizeForRequest(url, referrer);
  GetWebContents().SaveFrame(url, referrer, rfh);
}

void PDFDocumentHelper::UpdateContentRestrictions(
    int32_t content_restrictions) {
  client_->UpdateContentRestrictions(
      pdf_host_receivers_.GetCurrentTargetFrame(), content_restrictions);
}

DOCUMENT_USER_DATA_KEY_IMPL(PDFDocumentHelper);

}  // namespace pdf
