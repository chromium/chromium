// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_view.h"

#include <algorithm>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/single_animated_image_container.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_delegate.h"

namespace page_actions {

PageActionView::PageActionView(actions::ActionItem* action_item,
                               const PageActionViewParams& params,
                               ui::ElementIdentifier element_identifier)
    : IconLabelBubbleView(gfx::FontList(), params.icon_label_bubble_delegate),
      action_item_(action_item->GetAsWeakPtr()),
      icon_size_(params.icon_size),
      icon_insets_(params.icon_insets) {
  CHECK(action_item_->GetActionId().has_value());
  SetUpForAnimation(base::Milliseconds(600));

  SetProperty(views::kElementIdentifierKey, element_identifier);

  if (params.font_list) {
    SetFontList(*params.font_list);
  }

  image_container_view()->SetFlipCanvasOnPaintForRTLUI(true);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  SetVisible(false);
  label()->SetVisible(false);
  SetUseTonalColorsWhenExpanded(true);
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
  UpdateBorder();
  SetExpandedLabelAdditionalInsets(views::Inset1D(4, 8));

  label_visibility_changed_subscription_ =
      label()->AddVisibleChangedCallback(base::BindRepeating(
          &PageActionView::OnLabelVisibilityChanged, base::Unretained(this)));
}

PageActionView::~PageActionView() {
  // If this is currently highlighted, destroying the `ScopedAnchorHighlight`
  // might attempt to trigger an ink drop change even though we're
  // mid-destruction. Disable the ink drop to prevent this.
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);

  if (anchored_message_) {
    CHECK(anchored_message_widget_);
    anchored_message_ = nullptr;
    anchored_message_widget_ = nullptr;
  }
}

bool PageActionView::IsChipVisible() const {
  return ShouldShowLabel();
}

bool PageActionView::IsAnchoredMessageVisible() const {
  return (anchored_message_ != nullptr);
}

base::CallbackListSubscription PageActionView::AddChipVisibilityChangedCallback(
    ChipVisibilityChanged callback) {
  return chip_visibility_changed_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription
PageActionView::AddAnchoredMessageVisibilityChangedCallback(
    AnchoredMessageVisibilityCallback callback) {
  return anchored_message_visibility_changed_callbacks_.Add(
      std::move(callback));
}

void PageActionView::SetIsChipShowingChangedCallback(
    IsChipShowingChangedCallback callback) {
  is_chip_showing_changed_callback_ = std::move(callback);
  last_notified_is_chip_showing_.reset();
}

void PageActionView::SetImageAnimationStartedCallback(
    ImageAnimationStartedCallback callback) {
  image_animation_started_callback_ = std::move(callback);
}

void PageActionView::SetAnchoredMessageCloseCallback(
    base::RepeatingClosure callback) {
  anchored_message_close_callback_ = std::move(callback);
}

void PageActionView::SetClickCallback(
    base::RepeatingCallback<void(PageActionTrigger)> callback) {
  click_callback_ = std::move(callback);
}

void PageActionView::SetAnchoredMessageExpandCallback(
    base::RepeatingClosure callback) {
  anchored_message_expand_callback_ = std::move(callback);
}

void PageActionView::SetAnchoredMessageCollapseCallback(
    base::RepeatingClosure callback) {
  anchored_message_collapse_callback_ = std::move(callback);
}

void PageActionView::OnNewActiveController(PageActionController* controller) {
  observation_.Reset();
  action_item_controller_subscription_ = {};
  if (controller) {
    controller->RegisterCallbacks(PassKey(),
                                  action_item_->GetActionId().value(), this);

    controller->AddObserver(action_item_->GetActionId().value(), observation_);
    // TODO(crbug.com/388524315): Have the controller manage its own ActionItem
    // observation. See bug for more explanation.
    action_item_controller_subscription_ =
        controller->CreateActionItemSubscription(action_item_.get());
    OnPageActionModelChanged(*observation_.GetSource());
  } else {
    SetIsChipShowingChangedCallback(base::NullCallback());
    SetAnchoredMessageCloseCallback(base::NullCallback());
    SetClickCallback(base::NullCallback());
    SetVisible(false);
  }
}

void PageActionView::OnPageActionModelChanged(
    const PageActionModelInterface& model) {
  const bool visible = model.GetVisible();
  SetEnabled(visible);
  SetVisible(visible);

  if (visible) {
    SetLabel(model.GetText(), model.GetAccessibleName());
    SetTooltipText(model.GetTooltipText());
    UpdateIconImage();
  }

  if (model.GetActionActive() && !highlight_) {
    highlight_ = AddAnchorHighlight();
  } else if (!model.GetActionActive()) {
    highlight_.reset();
  }

  const bool was_chip_visible = IsChipVisible();
  if (!visible) {
    ResetSlideAnimation(/*show=*/false);
    NotifyIsChipShowingChange();
  } else if (model.ShouldShowSuggestionChip()) {
    if (model.GetShouldAnimateChipIn()) {
      AnimateIn(/*string_id=*/std::nullopt);
    } else {
      ResetSlideAnimation(/*show=*/true);
      NotifyIsChipShowingChange();
    }
  } else if (model.GetShouldAnimateChipOut()) {
    AnimateOut();
  } else {
    ResetSlideAnimation(/*show=*/false);
    NotifyIsChipShowingChange();
  }

  if (visible && model.ShouldShowAnchoredMessage()) {
    CreateAndShowAnchoredMessage(model);
  } else if (anchored_message_ && anchored_message_widget_) {
    anchored_message_ = nullptr;
    anchored_message_widget_ = nullptr;
  }

  // Announce the chip only if announcements are enabled and the chip was
  // newly shown.
  if (model.GetShouldAnnounceChip() && !was_chip_visible && IsChipVisible()) {
    GetViewAccessibility().AnnounceAlert(label()->GetText());
  }
}

void PageActionView::OnPageActionModelWillBeDeleted(
    const PageActionModelInterface& model) {
  anchored_message_ = nullptr;
  anchored_message_widget_ = nullptr;
  observation_.Reset();
  action_item_controller_subscription_ = {};
  SetVisible(false);
}

actions::ActionId PageActionView::GetActionId() const {
  return action_item_->GetActionId().value();
}

void PageActionView::OnThemeChanged() {
  IconLabelBubbleView::OnThemeChanged();
  UpdateIconImage();
}

void PageActionView::OnTouchUiChanged() {
  IconLabelBubbleView::OnTouchUiChanged();
  UpdateIconImage();
}

void PageActionView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && details.child == this) {
    UpdateIconImage();
  }
}

void PageActionView::UpdateBorder() {
  gfx::Insets border_insets = icon_insets_;
  if (observation_.IsObserving() &&
      !observation_.GetSource()->GetImage().IsVectorIcon()) {
    border_insets = GetInsetsForNonVectorIcon();
  }
  SetBorder(views::CreateEmptyBorder(border_insets));
}

bool PageActionView::ShouldShowSeparator() const {
  return false;
}

bool PageActionView::ShouldShowLabelAfterAnimation() const {
  return ShouldShowLabel();
}

bool PageActionView::ShouldUpdateInkDropOnClickCanceled() const {
  return true;
}

void PageActionView::NotifyClick(const ui::Event& event) {
  if (IsAnchoredMessageVisible()) {
    return;
  }

  PageActionTrigger trigger_source;
  if (event.IsMouseEvent()) {
    trigger_source = PageActionTrigger::kMouse;
  } else if (event.IsKeyEvent()) {
    trigger_source = PageActionTrigger::kKeyboard;
  } else {
    CHECK(event.IsGestureEvent());
    trigger_source = PageActionTrigger::kGesture;
  }

  // Click is expected to only happen when the page action is visible.
  // Therefore, the click metric should be recorded before executing the click
  // callback since that may change the page action visibility.
  CHECK(click_callback_);
  click_callback_.Run(trigger_source);

  IconLabelBubbleView::NotifyClick(event);
  action_item_->InvokeAction(
      actions::ActionInvocationContext::Builder()
          .SetProperty(kPageActionTriggerKey,
                       static_cast<std::underlying_type_t<PageActionTrigger>>(
                           trigger_source))
          .Build());
}

void PageActionView::AnimationEnded(const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);
  NotifyIsChipShowingChange();
}

void PageActionView::UpdateIconImage() {
  if (!GetVisible() || observation_.GetSource() == nullptr ||
      observation_.GetSource()->GetImage().IsEmpty()) {
    return;
  }

  const auto& icon_image = observation_.GetSource()->GetImage();
  const SkColor icon_color = observation_.GetSource()->GetColorSource() ==
                                     PageActionColorSource::kForeground
                                 ? GetForegroundColor()
                                 : views::GetCascadingAccentColor(this);

  if (observation_.GetSource()->GetShouldAnimateImage()) {
    int resource_id = observation_.GetSource()->GetImageAnimationResourceId();
    AnimateImage(resource_id, icon_color);
  }

  // If image does not have a vector icon, set it directly.
  if (icon_image.IsVectorIcon()) {
    SetImageModel(ui::ImageModel::FromVectorIcon(
        *icon_image.GetVectorIcon().vector_icon(), icon_color, icon_size_));
  } else {
    SetImageModel(icon_image);
    // For non-vector icons, the border needs to be updated to accommodate the
    // icon, as the icon size may vary. For vector icons, the border gets
    // set on instantiation and does not need to be updated again.
    UpdateBorder();
  }
}

void PageActionView::AnimateImage(int resource_id, SkColor icon_color) {
  views::SingleAnimatedImageContainer::AnimationConfig config{
      .direction =
          views::SingleAnimatedImageContainer::AnimationDirection::kForward,
      .end_behavior =
          views::SingleAnimatedImageContainer::AnimationEndBehavior::kReset};

  animated_image_container().PlayAnimation({resource_id, icon_color}, config);
  image_animation_started_callback_.Run();
}

const gfx::Insets PageActionView::GetInsetsForNonVectorIcon() const {
  const gfx::Size image_size = observation_.GetSource()->GetImage().Size();

  const int horizontal_padding =
      (icon_size_ + icon_insets_.width() - image_size.width()) / 2;
  const int vertical_padding =
      (icon_size_ + icon_insets_.height() - image_size.height()) / 2;

  CHECK(horizontal_padding >= 0)
      << "Horizontal size of image exceeds maximum.\nIcon Size: " << icon_size_
      << "\nInsets: " << icon_insets_.width()
      << "\nImage Size: " << image_size.width();
  CHECK(vertical_padding >= 0)
      << "Vertical size of image exceeds maximum.\nIcon Size: " << icon_size_
      << "\nInsets: " << icon_insets_.height()
      << "\nImage Size: " << image_size.height();

  return gfx::Insets::VH(std::max(vertical_padding, 0),
                         std::max(horizontal_padding, 0));
}

void PageActionView::SetModel(PageActionModelInterface* model) {
  observation_.Reset();
  observation_.Observe(model);
}

gfx::Size PageActionView::GetMinimumSize() const {
  gfx::Size icon_preferred_size = image_container_view()->GetPreferredSize();
  if (observation_.IsObserving() &&
      !observation_.GetSource()->GetImage().IsVectorIcon()) {
    const gfx::Insets insets = GetInsetsForNonVectorIcon();
    icon_preferred_size.Enlarge(insets.width(), insets.height());
  } else {
    icon_preferred_size.Enlarge(icon_insets_.width(), icon_insets_.height());
  }

  return icon_preferred_size;
}

bool PageActionView::IsBubbleShowing() const {
  return observation_.IsObserving() &&
         observation_.GetSource()->GetActionItemIsShowingBubble();
}

bool PageActionView::IsTriggerableEvent(const ui::Event& event) {
  if (IsAnchoredMessageVisible()) {
    return false;
  }

  // Returns whether the bubble should be shown given the event. Only trigger an
  // action when action UI isn't already showing (managed at the
  // IconLabelBubbleView level), and if mouse input, when event is a left button
  // click.
  if (event.IsMouseEvent()) {
    // IconLabelBubbleView allows any mouse click to be triggerable event so
    // need to manually check here.
    return IconLabelBubbleView::IsTriggerableEvent(event) &&
           ((GetTriggerableEventFlags() & event.flags()) != 0);
  }

  return IconLabelBubbleView::IsTriggerableEvent(event);
}

void PageActionView::OnLabelVisibilityChanged() {
  if (!GetVisible()) {
    chip_visibility_changed_callbacks_.Notify(this);
    return;
  }
  UpdateBackground();
  UpdateLabelColors();
  UpdateIconImage();
  chip_visibility_changed_callbacks_.Notify(this);
}

views::View* PageActionView::GetLabelForTesting() {
  return label();
}

gfx::SlideAnimation& PageActionView::GetSlideAnimationForTesting() {
  return slide_animation_;
}

void PageActionView::NotifyIsChipShowingChange() {
  const bool is_chip_showing = IsChipVisible();
  if (last_notified_is_chip_showing_ == is_chip_showing) {
    return;
  }
  last_notified_is_chip_showing_ = is_chip_showing;
  // Defer to avoid re-entrancy into PageActionModel::NotifyChange().
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(is_chip_showing_changed_callback_, is_chip_showing));
}

void PageActionView::CreateAndShowAnchoredMessage(
    const PageActionModelInterface& model) {
  const std::u16string chip_text(label()->GetText());

  if (anchored_message_) {
    anchored_message_->UpdateContent(model);
    return;
  }

  auto message_delegate = std::make_unique<AnchoredMessageBubbleView>(
      views::BubbleAnchor(this), model, *this);
  anchored_message_ = message_delegate.get();

  anchored_message_widget_ =
      base::WrapUnique(views::BubbleDialogDelegate::CreateBubbleDeprecated(
          std::move(message_delegate),
          views::Widget::InitParams::CLIENT_OWNS_WIDGET));

  if (anchored_message_widget_) {
    anchored_message_widget_->MakeCloseSynchronous(
        base::BindOnce(&PageActionView::OnAnchoredMessageWidgetClose,
                       weak_factory_.GetWeakPtr()));

    // Don't steal focus when shown
    anchored_message_widget_->ShowInactive();
  } else {
    anchored_message_ = nullptr;
  }

  anchored_message_visibility_changed_callbacks_.Notify(this);
}
void PageActionView::OnAnchoredMessageWidgetClose(
    views::Widget::ClosedReason closed_reason) {
  CHECK(anchored_message_);
  CHECK(anchored_message_widget_);
  anchored_message_ = nullptr;
  anchored_message_widget_.reset();
  anchored_message_visibility_changed_callbacks_.Notify(this);
}

void PageActionView::AnchoredMessageChipClick() {
  CHECK(click_callback_);
  click_callback_.Run(PageActionTrigger::kMouse);
  action_item_->InvokeAction(
      actions::ActionInvocationContext::Builder()
          .SetProperty(kPageActionTriggerKey,
                       static_cast<std::underlying_type_t<PageActionTrigger>>(
                           PageActionTrigger::kMouse))
          .Build());
  anchored_message_close_callback_.Run();
}

void PageActionView::CloseAnchoredMessage() {
  anchored_message_close_callback_.Run();
}

void PageActionView::AnchoredMessageExpanded() {
  anchored_message_expand_callback_.Run();
}

void PageActionView::AnchoredMessageCollapsed() {
  anchored_message_collapse_callback_.Run();
}

AnchoredMessageBubbleView* PageActionView::GetAnchoredMessageForTesting() {
  return anchored_message_;
}

BEGIN_METADATA(PageActionView)
END_METADATA

}  // namespace page_actions
