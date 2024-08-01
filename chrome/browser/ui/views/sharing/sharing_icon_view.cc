// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing/sharing_icon_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace {
// Progress state when the full length of the animation text is visible.
constexpr double kAnimationTextFullLengthShownProgressState = 0.5;
}  // namespace

SharingIconView::SharingIconView(
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate,
    GetControllerCallback get_controller_callback,
    GetBubbleCallback get_bubble_callback)
    : PageActionIconView(/*command_updater=*/nullptr,
                         /*command_id=*/0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "ClickToCall"),  // Naming corresponds to
                                          // PageActionIconType.
      get_controller_callback_(std::move(get_controller_callback)),
      get_bubble_callback_(std::move(get_bubble_callback)) {
  SetVisible(false);
  SetUpForInOutAnimation();

  auto* controller = GetController();
  if (controller && !controller->HasAccessibleUi()) {
    // This should rarely be true. One example where it is true is the
    // SmsRemoteFetcherUiController: crrev.com/c/2964059 stopped all UI
    // from being shown and removed the accessible name. Setting the state
    // to ignored is needed to stop the UI from being shown to assistive
    // technologies.
    GetViewAccessibility().SetIsIgnored(true);
    return;
  }
}

SharingIconView::~SharingIconView() = default;

SharingUiController* SharingIconView::GetController() const {
  content::WebContents* web_contents = GetWebContents();
  return web_contents ? get_controller_callback_.Run(web_contents) : nullptr;
}

void SharingIconView::StartLoadingAnimation() {
  if (loading_animation_)
    return;

  loading_animation_ = true;
  AnimateIn(IDS_BROWSER_SHARING_OMNIBOX_SENDING_LABEL);
  SchedulePaint();
}

void SharingIconView::StopLoadingAnimation() {
  if (!loading_animation_)
    return;

  loading_animation_ = false;
  UnpauseAnimation();
  SchedulePaint();
}

void SharingIconView::UpdateImpl() {
  auto* controller = GetController();
  if (!controller)
    return;

  GetViewAccessibility().SetName(
      controller->GetTextForTooltipAndAccessibleName());

  // To ensure that we reset error icon badge.
  if (!GetVisible()) {
    should_show_error_ = controller->HasSendFailed();
    UpdateIconImage();
  }

  if (controller->is_loading())
    StartLoadingAnimation();
  else
    StopLoadingAnimation();

  if (last_controller_ != controller) {
    ResetSlideAnimation(/*show=*/false);
  }

  last_controller_ = controller;

  const bool is_bubble_showing = IsBubbleShowing();
  const bool is_visible =
      is_bubble_showing || loading_animation_ || label()->GetVisible();

  SetVisible(is_visible);
  UpdateInkDrop(is_bubble_showing);
}

void SharingIconView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation->is_animating() &&
      GetAnimationValue() >= kAnimationTextFullLengthShownProgressState &&
      loading_animation_) {
    PauseAnimation();
  }
  UpdateOpacity();
  return PageActionIconView::AnimationProgressed(animation);
}

void SharingIconView::AnimationEnded(const gfx::Animation* animation) {
  PageActionIconView::AnimationEnded(animation);
  UpdateOpacity();

  auto* controller = GetController();
  if (controller && should_show_error_ != controller->HasSendFailed()) {
    should_show_error_ = controller->HasSendFailed();
    UpdateIconImage();
    controller->MaybeShowErrorDialog();
  }
  Update();
}

void SharingIconView::UpdateOpacity() {
  if (!IsShrinking()) {
    DestroyLayer();
    SetTextSubpixelRenderingEnabled(true);
    return;
  }

  if (!layer()) {
    SetPaintToLayer();
    SetTextSubpixelRenderingEnabled(false);
    layer()->SetFillsBoundsOpaquely(false);
  }

  int kLargeNumber = 100;
  layer()->SetOpacity(GetWidthBetween(0, kLargeNumber) /
                      static_cast<float>(kLargeNumber));
}

void SharingIconView::UpdateInkDrop(bool activate) {
  auto target_state =
      activate ? views::InkDropState::ACTIVATED : views::InkDropState::HIDDEN;
  if (views::InkDrop::Get(this)->GetInkDrop()->GetTargetInkDropState() !=
      target_state)
    views::InkDrop::Get(this)->AnimateToState(target_state, /*event=*/nullptr);
}

bool SharingIconView::IsTriggerableEvent(const ui::Event& event) {
  // We do nothing when the icon is clicked.
  return false;
}

const gfx::VectorIcon& SharingIconView::GetVectorIconBadge() const {
  return should_show_error_ ? vector_icons::kBlockedBadgeIcon : gfx::kNoneIcon;
}

void SharingIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

views::BubbleDialogDelegate* SharingIconView::GetBubble() const {
  auto* controller = GetController();
  return controller ? get_bubble_callback_.Run(controller->dialog()) : nullptr;
}

const gfx::VectorIcon& SharingIconView::GetVectorIcon() const {
  auto* controller = GetController();
  return controller ? controller->GetVectorIcon() : gfx::kNoneIcon;
}

void SharingIconView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  auto* controller = GetController();
  if (controller && !controller->HasAccessibleUi()) {
    return;
  }
  PageActionIconView::GetAccessibleNodeData(node_data);
}

BEGIN_METADATA(SharingIconView)
END_METADATA
