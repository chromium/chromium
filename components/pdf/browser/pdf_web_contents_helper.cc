// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_web_contents_helper.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/referrer_type_converters.h"
#include "ui/base/pointer/touch_editing_controller.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"

namespace pdf {

// static
void PDFWebContentsHelper::CreateForWebContentsWithClient(
    content::WebContents* contents,
    std::unique_ptr<PDFWebContentsHelperClient> client) {
  if (FromWebContents(contents))
    return;
  contents->SetUserData(
      UserDataKey(),
      base::WrapUnique(new PDFWebContentsHelper(contents, std::move(client))));
}

// static
void PDFWebContentsHelper::BindPdfService(
    mojo::PendingAssociatedReceiver<mojom::PdfService> pdf_service,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* pdf_helper = PDFWebContentsHelper::FromWebContents(web_contents);
  if (!pdf_helper)
    return;
  pdf_helper->pdf_service_receivers_.Bind(rfh, std::move(pdf_service));
}

PDFWebContentsHelper::PDFWebContentsHelper(
    content::WebContents* web_contents,
    std::unique_ptr<PDFWebContentsHelperClient> client)
    : content::WebContentsUserData<PDFWebContentsHelper>(*web_contents),
      pdf_service_receivers_(web_contents, this),
      client_(std::move(client)) {}

PDFWebContentsHelper::~PDFWebContentsHelper() {
  if (pdf_rwh_)
    pdf_rwh_->RemoveObserver(this);

  if (!touch_selection_controller_client_manager_)
    return;

  // PDFWebContentsHelperTest overrides TouchSelectionControllerClientManager
  // to mock it and GetTouchSelectionController() returns nullptr in that case.
  // This check prevents the tests from failing in that condition.
  ui::TouchSelectionController* touch_selection_controller =
      touch_selection_controller_client_manager_->GetTouchSelectionController();
  if (touch_selection_controller)
    touch_selection_controller->HideAndDisallowShowingAutomatically();

  touch_selection_controller_client_manager_->InvalidateClient(this);
  touch_selection_controller_client_manager_->RemoveObserver(this);
}

void PDFWebContentsHelper::SetListener(
    mojo::PendingRemote<mojom::PdfListener> listener) {
  remote_pdf_client_.reset();
  remote_pdf_client_.Bind(std::move(listener));

  if (pdf_rwh_)
    pdf_rwh_->RemoveObserver(this);
  pdf_rwh_ = client_->FindPdfFrame(&GetWebContents())->GetRenderWidgetHost();
  pdf_rwh_->AddObserver(this);
}

gfx::PointF PDFWebContentsHelper::ConvertHelper(const gfx::PointF& point_f,
                                                float scale) {
  if (!pdf_rwh_)
    return point_f;

  content::RenderWidgetHostView* view = pdf_rwh_->GetView();
  if (!view)
    return point_f;

  gfx::Vector2dF offset =
      view->TransformPointToRootCoordSpaceF(gfx::PointF()).OffsetFromOrigin();
  offset.Scale(scale);

  return point_f + offset;
}

gfx::PointF PDFWebContentsHelper::ConvertFromRoot(const gfx::PointF& point_f) {
  return ConvertHelper(point_f, -1.f);
}

gfx::PointF PDFWebContentsHelper::ConvertToRoot(const gfx::PointF& point_f) {
  return ConvertHelper(point_f, +1.f);
}

void PDFWebContentsHelper::SelectionChanged(const gfx::PointF& left,
                                            int32_t left_height,
                                            const gfx::PointF& right,
                                            int32_t right_height) {
  selection_left_ = left;
  selection_left_height_ = left_height;
  selection_right_ = right;
  selection_right_height_ = right_height;

  DidScroll();
}

void PDFWebContentsHelper::SetPluginCanSave(bool can_save) {
  client_->SetPluginCanSave(pdf_service_receivers_.GetCurrentTargetFrame(),
                            can_save);
}

void PDFWebContentsHelper::DidScroll() {
  if (!touch_selection_controller_client_manager_)
    InitTouchSelectionClientManager();

  if (touch_selection_controller_client_manager_) {
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
  }
}

void PDFWebContentsHelper::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  if (pdf_rwh_ == widget_host)
    pdf_rwh_ = nullptr;
}

bool PDFWebContentsHelper::SupportsAnimation() const {
  return false;
}

void PDFWebContentsHelper::MoveCaret(const gfx::PointF& position) {
  if (!remote_pdf_client_)
    return;
  remote_pdf_client_->SetCaretPosition(ConvertFromRoot(position));
}

void PDFWebContentsHelper::MoveRangeSelectionExtent(const gfx::PointF& extent) {
  if (!remote_pdf_client_)
    return;
  remote_pdf_client_->MoveRangeSelectionExtent(ConvertFromRoot(extent));
}

void PDFWebContentsHelper::SelectBetweenCoordinates(const gfx::PointF& base,
                                                    const gfx::PointF& extent) {
  if (!remote_pdf_client_)
    return;
  remote_pdf_client_->SetSelectionBounds(ConvertFromRoot(base),
                                         ConvertFromRoot(extent));
}

void PDFWebContentsHelper::OnSelectionEvent(ui::SelectionEventType event) {
  // Should be handled by `TouchSelectionControllerClientAura`.
  NOTREACHED();
}

void PDFWebContentsHelper::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  // Should be handled by `TouchSelectionControllerClientAura`.
  NOTREACHED();
}

std::unique_ptr<ui::TouchHandleDrawable>
PDFWebContentsHelper::CreateDrawable() {
  // We can return null here, as the manager will look after this.
  return nullptr;
}

void PDFWebContentsHelper::OnManagerWillDestroy(
    content::TouchSelectionControllerClientManager* manager) {
  DCHECK_EQ(touch_selection_controller_client_manager_, manager);
  manager->RemoveObserver(this);
  touch_selection_controller_client_manager_ = nullptr;
}

bool PDFWebContentsHelper::IsCommandIdEnabled(int command_id) const {
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

void PDFWebContentsHelper::ExecuteCommand(int command_id, int event_flags) {
  // TODO(wjmaclean, dsinclair): Need to communicate to PDFium to accept
  // cut/paste commands.
  switch (command_id) {
    case ui::TouchEditable::kCopy:
      GetWebContents().Copy();
      break;
  }
}

void PDFWebContentsHelper::RunContextMenu() {
  content::RenderFrameHost* focused_frame = GetWebContents().GetFocusedFrame();
  if (!focused_frame)
    return;

  content::RenderWidgetHost* widget = focused_frame->GetRenderWidgetHost();
  if (!widget || !widget->GetView())
    return;

  if (!touch_selection_controller_client_manager_)
    InitTouchSelectionClientManager();

  if (!touch_selection_controller_client_manager_)
    return;

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

bool PDFWebContentsHelper::ShouldShowQuickMenu() {
  return false;
}

std::u16string PDFWebContentsHelper::GetSelectedText() {
  return std::u16string();
}

void PDFWebContentsHelper::InitTouchSelectionClientManager() {
  content::RenderWidgetHostView* view =
      GetWebContents().GetRenderWidgetHostView();
  if (!view)
    return;

  touch_selection_controller_client_manager_ =
      view->GetTouchSelectionControllerClientManager();
  if (!touch_selection_controller_client_manager_)
    return;

  touch_selection_controller_client_manager_->AddObserver(this);
}

void PDFWebContentsHelper::HasUnsupportedFeature() {
  client_->OnPDFHasUnsupportedFeature(&GetWebContents());
}

void PDFWebContentsHelper::SaveUrlAs(const GURL& url,
                                     network::mojom::ReferrerPolicy policy) {
  client_->OnSaveURL(&GetWebContents());

  content::RenderFrameHost* rfh = GetWebContents().GetOuterWebContentsFrame();
  if (!rfh)
    return;

  content::Referrer referrer(url, policy);
  referrer = content::Referrer::SanitizeForRequest(url, referrer);
  GetWebContents().SaveFrame(url, referrer, rfh);
}

void PDFWebContentsHelper::UpdateContentRestrictions(
    int32_t content_restrictions) {
  client_->UpdateContentRestrictions(
      pdf_service_receivers_.GetCurrentTargetFrame(), content_restrictions);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PDFWebContentsHelper);

}  // namespace pdf
