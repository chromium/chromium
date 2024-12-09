// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/font_list.h"

namespace {
bool ShouldShow(base::WeakPtr<actions::ActionItem> action_item,
                page_actions::PageActionModel* model) {
  return action_item.get() && action_item->GetEnabled() &&
         action_item->GetVisible() && model && model->show_requested();
}
}  // namespace

namespace page_actions {

PageActionView::PageActionView(actions::ActionItem* action_item,
                               IconLabelBubbleView::Delegate* parent_delegate)
    : IconLabelBubbleView(gfx::FontList(), parent_delegate),
      action_item_(action_item->GetAsWeakPtr()) {
  CHECK(action_item_->GetActionId().has_value());
}

PageActionView::~PageActionView() = default;

void PageActionView::OnNewActiveController(PageActionController* controller) {
  observation_.Reset();
  if (controller) {
    controller->AddObserver(action_item_->GetActionId().value(), observation_);
  }
  SetVisible(ShouldShow(action_item_, observation_.GetSource()));
}

void PageActionView::OnPageActionModelChanged(PageActionModel* model) {
  SetVisible(ShouldShow(action_item_, model));
}

void PageActionView::OnPageActionModelWillBeDeleted(PageActionModel* model) {
  observation_.Reset();
  SetVisible(false);
}

std::unique_ptr<views::ActionViewInterface>
PageActionView::GetActionViewInterface() {
  return std::make_unique<PageActionViewInterface>(this,
                                                   observation_.GetSource());
}

actions::ActionId PageActionView::GetActionId() const {
  return action_item_->GetActionId().value();
}

BEGIN_METADATA(PageActionView)
END_METADATA

PageActionViewInterface::PageActionViewInterface(PageActionView* action_view,
                                                 PageActionModel* model)
    : views::LabelButtonActionViewInterface(action_view),
      action_view_(action_view),
      model_(model) {}

PageActionViewInterface::~PageActionViewInterface() = default;

void PageActionViewInterface::ActionItemChangedImpl(
    actions::ActionItem* action_item) {
  views::LabelButtonActionViewInterface::ActionItemChangedImpl(action_item);
  action_view_->SetVisible(ShouldShow(action_item->GetAsWeakPtr(), model_));
}

}  // namespace page_actions
