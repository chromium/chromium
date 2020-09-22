// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_web_contents_helper.h"

#include <utility>

#include "base/memory/ptr_util.h"
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

PDFWebContentsHelper::PDFWebContentsHelper(
    content::WebContents* web_contents,
    std::unique_ptr<PDFWebContentsHelperClient> client)
    : content::WebContentsObserver(web_contents),
      pdf_service_receivers_(web_contents, this),
      client_(std::move(client)) {}

PDFWebContentsHelper::~PDFWebContentsHelper() {
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
}

gfx::PointF PDFWebContentsHelper::ConvertHelper(const gfx::PointF& point_f,
                                                float scale) const {
  gfx::PointF origin_f;
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (view) {
    origin_f = view->TransformPointToRootCoordSpaceF(gfx::PointF());
    origin_f.Scale(scale);
  }

  return gfx::PointF(point_f.x() + origin_f.x(), point_f.y() + origin_f.y());
}

gfx::PointF PDFWebContentsHelper::ConvertFromRoot(
    const gfx::PointF& point_f) const {
  return ConvertHelper(point_f, -1.f);
}

gfx::PointF PDFWebContentsHelper::ConvertToRoot(
    const gfx::PointF& point_f) const {
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
  client_->SetPluginCanSave(web_contents(), can_save);
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

void PDFWebContentsHelper::OnSelectionEvent(ui::SelectionEventType event) {}

void PDFWebContentsHelper::OnDragUpdate(const gfx::PointF& position) {}

std::unique_ptr<ui::TouchHandleDrawable>
PDFWebContentsHelper::CreateDrawable() {
  // We can return null here, as the manager will look after this.
  return std::unique_ptr<ui::TouchHandleDrawable>();
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
      web_contents()->Copy();
      break;
  }
}

void PDFWebContentsHelper::RunContextMenu() {
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();

  if (!view)
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

  gfx::PointF origin = view->TransformPointToRootCoordSpaceF(gfx::PointF());
  anchor_point.Offset(-origin.x(), -origin.y());
  view->GetRenderWidgetHost()->ShowContextMenuAtPoint(
      gfx::ToRoundedPoint(anchor_point), ui::MENU_SOURCE_TOUCH_EDIT_MENU);

  // Hide selection handles after getting rect-between-bounds from touch
  // selection controller; otherwise, rect would be empty and the above
  // calculations would be invalid.
  touch_selection_controller->HideAndDisallowShowingAutomatically();
}

bool PDFWebContentsHelper::ShouldShowQuickMenu() {
  return false;
}

base::string16 PDFWebContentsHelper::GetSelectedText() {
  return base::string16();
}

void PDFWebContentsHelper::InitTouchSelectionClientManager() {
  content::RenderWidgetHostView* view =
      web_contents()->GetRenderWidgetHostView();
  if (!view)
    return;

  touch_selection_controller_client_manager_ =
      view->GetTouchSelectionControllerClientManager();
  if (!touch_selection_controller_client_manager_)
    return;

  touch_selection_controller_client_manager_->AddObserver(this);
}

void PDFWebContentsHelper::HasUnsupportedFeature() {
  client_->OnPDFHasUnsupportedFeature(web_contents());
}

void PDFWebContentsHelper::SaveUrlAs(const GURL& url,
                                     blink::mojom::ReferrerPtr referrer) {
  client_->OnSaveURL(web_contents());
  web_contents()->SaveFrame(url, referrer.To<content::Referrer>());
}

void PDFWebContentsHelper::UpdateContentRestrictions(
    int32_t content_restrictions) {
  client_->UpdateContentRestrictions(web_contents(), content_restrictions);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PDFWebContentsHelper)

}  // namespace pdf
