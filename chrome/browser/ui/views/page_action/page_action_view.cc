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
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/page_action/page_action_view_params.h"
#include "chrome/browser/ui/views/page_action/page_action_view_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/single_animated_image_container.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget_delegate.h"

namespace page_actions {

namespace {

IconLabelBubbleView::AnimationStyle GetViewsAnimationStyle(
    PageActionAnimationStyle style) {
  switch (style) {
    case PageActionAnimationStyle::kStandard:
      return IconLabelBubbleView::AnimationStyle::kStandard;
    case PageActionAnimationStyle::kSlideAndCrossfade:
      return IconLabelBubbleView::AnimationStyle::kSlideAndCrossfade;
  }
}

}  // namespace

PageActionView::PageActionView(actions::ActionItem* action_item,
                               const PageActionViewParams& params,
                               PageActionIconType type,
                               ui::ElementIdentifier element_identifier)
    : IconLabelBubbleView(gfx::FontList(), params.icon_label_bubble_delegate),
      action_item_(action_item->GetAsWeakPtr()),
      icon_size_(params.icon_size),
      icon_insets_(params.icon_insets),
      type_(type) {
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

base::CallbackListSubscription PageActionView::AddVisibilityChangedCallback(
    VisibilityChangedCallback callback) {
  return visibility_changed_callbacks_.Add(std::move(callback));
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
  chip_shown_metric_recorded_ = false;
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
    SetIsChipShowingChangedCallback(base::DoNothing());
    SetImageAnimationStartedCallback(base::DoNothing());
    SetAnchoredMessageCloseCallback(base::DoNothing());
    SetClickCallback(base::DoNothing());
    SetAnchoredMessageExpandCallback(base::DoNothing());
    SetAnchoredMessageCollapseCallback(base::DoNothing());
    SetVisible(false);
  }
}

void PageActionView::OnPageActionModelChanged(
    const PageActionModelInterface& model) {
  const bool visible = model.GetVisible();
  const bool declared_changed = (declared_visible_ != visible);
  declared_visible_ = visible;
  SetEnabled(visible);
  SetVisible(visible);

  if (declared_changed) {
    visibility_changed_callbacks_.Notify(this);
  }

  if (visible) {
    SetLabel(model.GetText(), model.GetAccessibleName());
  }

  if (model.GetActionActive() && !highlight_) {
    highlight_ = AddAnchorHighlight();
  } else if (!model.GetActionActive()) {
    highlight_.reset();
  }

  const bool was_chip_visible = IsChipVisible();

  UpdateIconImage();
  UpdateAnimationState(model);

  if (visible && model.ShouldShowAnchoredMessage()) {
    CreateAndShowAnchoredMessage(model);
  } else if (anchored_message_ && anchored_message_widget_ &&
             !anchored_message_widget_->IsClosed()) {
    anchored_message_widget_->Close();
  }

  UpdateTooltipText();

  // Announce the chip only if announcements are enabled and the chip was
  // newly shown.
  if (model.GetShouldAnnounceChip() && !was_chip_visible && IsChipVisible()) {
    GetViewAccessibility().AnnounceAlert(label()->GetText());
  }
}

void PageActionView::UpdateAnimationState(
    const PageActionModelInterface& model) {
  const bool visible = model.GetVisible();

  // Configure views style and trailing icon first.
  const auto views_style = GetViewsAnimationStyle(model.GetAnimationStyle());

  // If hidden, reset the slide animation.
  if (!visible) {
    ResetSlideAnimation(/*show=*/false);
    NotifyIsChipShowingChange();
    return;
  }

  // Drive the transitions based on the animation.
  if (views_style == IconLabelBubbleView::AnimationStyle::kSlideAndCrossfade) {
    HandleSlideAndCrossfadeTransition(model);
  } else {
    HandleSuggestionChipTransition(model);
  }
}

void PageActionView::HandleSlideAndCrossfadeTransition(
    const PageActionModelInterface& model) {
  if (model.GetShowTrailingIcon()) {
    AnimateIn(/*string_id=*/std::nullopt);
  } else {
    AnimateOut();
  }
}

void PageActionView::HandleSuggestionChipTransition(
    const PageActionModelInterface& model) {
  if (model.ShouldShowSuggestionChip()) {
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
}

void PageActionView::OnPageActionModelWillBeDeleted(
    const PageActionModelInterface& model) {
  anchored_message_ = nullptr;
  anchored_message_widget_ = nullptr;
  observation_.Reset();
  action_item_controller_subscription_ = {};
  SetVisible(false);
}

views::BubbleAnchor PageActionView::GetBubbleAnchor() {
  return views::BubbleAnchor(this);
}

std::u16string PageActionView::GetTooltipText() const {
  return IconLabelBubbleView::GetTooltipText();
}

std::u16string PageActionView::GetAccessibleName() const {
  return IconLabelBubbleView::GetAccessibleName();
}

void PageActionView::SetVisible(bool visible) {
  IconLabelBubbleView::SetVisible(visible);
}

bool PageActionView::GetDeclaredVisible() const {
  return declared_visible_;
}

IconLabelBubbleView* PageActionView::GetIconLabelBubbleViewNotMigrated() {
  NOTREACHED();
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
  auto builder = actions::ActionInvocationContext::Builder()
                     .SetProperty(kPageActionTriggerKey, trigger_source)
                     .SetProperty(kPageActionEntryPointKey,
                                  PageActionEntryPoint::kSuggestionChip);
  if (auto side_panel_trigger =
          GetSidePanelOpenTriggerForPageAction(action_item_->GetActionId())) {
    builder = std::move(builder).SetProperty(kSidePanelOpenTriggerKey,
                                             *side_panel_trigger);
  }
  action_item_->InvokeAction(std::move(builder).Build());
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
    std::optional<page_actions::PageActionAnimationParams> params =
        observation_.GetSource()->GetImageAnimationParameters();
    CHECK(params.has_value());
    AnimateImage(params.value(), icon_color);
  }

  const int drawing_icon_size =
      icon_image.IsVectorIcon() ? icon_size_ : icon_image.Size().width();

  // If image does not have a vector icon, set it directly.
  if (icon_image.IsVectorIcon()) {
    SetImageModel(ui::ImageModel::FromVectorIcon(
        *icon_image.GetVectorIcon().vector_icon(), icon_color,
        drawing_icon_size));
  } else {
    SetImageModel(icon_image);
    // For non-vector icons, the border needs to be updated to accommodate the
    // icon, as the icon size may vary. For vector icons, the border gets
    // set on instantiation and does not need to be updated again.
    UpdateBorder();
  }

  // Add trailing icon if it is set.
  std::optional<ui::ImageModel> trailing_image_opt =
      observation_.GetSource()->GetTrailingImage();
  if (trailing_image_opt.has_value()) {
    const auto& trailing_image = trailing_image_opt.value();
    if (trailing_image.IsVectorIcon()) {
      SetCrossfadeImage(ui::ImageModel::FromVectorIcon(
          *trailing_image.GetVectorIcon().vector_icon(), icon_color,
          drawing_icon_size));
    } else {
      SetCrossfadeImage(trailing_image);
    }
  } else {
    SetCrossfadeImage(ui::ImageModel());
  }
}

void PageActionView::AnimateImage(
    const page_actions::PageActionAnimationParams& params,
    SkColor icon_color) {
  views::SingleAnimatedImageContainer::AnimationConfig config{
      .tween = params.tween,
      .duration = params.duration};

  if (params.start_offset != 0.0f || params.end_offset != 1.0f) {
    config.boundary = views::SingleAnimatedImageContainer::AnimationBoundary{
        .start_offset = params.start_offset, .end_offset = params.end_offset};
  }

  animated_image_container().PlayAnimation(
      {params.resource_id, icon_color,
       views::SingleAnimatedImageContainer::AnimationDirection::kForward,
       views::SingleAnimatedImageContainer::AnimationEndBehavior::kReset},
      config);
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
  UpdateTooltipText();
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
  if (!is_chip_showing) {
    chip_shown_metric_recorded_ = false;
  }
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
    UpdateTooltipText();
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
    UpdateBackground();
    UpdateIconImage();

    // Don't steal focus when shown
    anchored_message_widget_->ShowInactive();
  } else {
    anchored_message_ = nullptr;
  }

  UpdateTooltipText();
  anchored_message_visibility_changed_callbacks_.Notify(this);
}
void PageActionView::OnAnchoredMessageWidgetClose(
    views::Widget::ClosedReason closed_reason) {
  // If `anchored_message_` is already null, it means we are in the middle of
  // destroying the view (e.g. from ~PageActionView). In this case, early return
  // to avoid re-entrancy crashes.
  if (!anchored_message_) {
    return;
  }
  CHECK(anchored_message_widget_);
  anchored_message_ = nullptr;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PageActionView::CloseWidgetDeferred,
                                weak_factory_.GetWeakPtr(),
                                anchored_message_widget_->GetWeakPtr()));
  UpdateBackground();
  UpdateIconImage();
  UpdateTooltipText();
  anchored_message_visibility_changed_callbacks_.Notify(this);

  if (observation_.IsObserving() &&
      observation_.GetSource()->ShouldShowAnchoredMessage()) {
    CloseAnchoredMessage();
  }
}

void PageActionView::UpdateTooltipText() {
  if (!observation_.IsObserving() || !observation_.GetSource() ||
      !GetVisible()) {
    SetTooltipText(std::u16string());
    return;
  }

  std::u16string tooltip_text = observation_.GetSource()->GetTooltipText();
  if (IsAnchoredMessageVisible() && !tooltip_text.empty()) {
    tooltip_text = l10n_util::GetStringFUTF16(
        IDS_PAGE_ACTION_ANCHORED_MESSAGE_SHOWING, tooltip_text);
  }
  SetTooltipText(tooltip_text);
}

void PageActionView::CloseWidgetDeferred(
    base::WeakPtr<views::Widget> widget_to_close) {
  if (anchored_message_widget_ &&
      anchored_message_widget_.get() == widget_to_close.get()) {
    anchored_message_widget_.reset();
  }
}

void PageActionView::AnchoredMessageChipClick() {
  CHECK(click_callback_);
  click_callback_.Run(PageActionTrigger::kMouse);
  auto builder =
      actions::ActionInvocationContext::Builder()
          .SetProperty(kPageActionTriggerKey, PageActionTrigger::kMouse)
          .SetProperty(kPageActionEntryPointKey,
                       PageActionEntryPoint::kAnchoredMessage);
  if (auto side_panel_trigger =
          GetSidePanelOpenTriggerForPageAction(action_item_->GetActionId())) {
    builder = std::move(builder).SetProperty(kSidePanelOpenTriggerKey,
                                             *side_panel_trigger);
  }
  action_item_->InvokeAction(std::move(builder).Build());
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

void PageActionView::BeforeApplyLayout(const views::ProposedLayout& layout) {
  if (!GetVisible() || !IsChipVisible() || chip_shown_metric_recorded_ ||
      GetAnimationValue() != 1.0 || GetText().empty()) {
    return;
  }
  int label_width = 0;
  for (const auto& child_layout : layout.child_layouts) {
    if (child_layout.child_view == label()) {
      label_width = child_layout.bounds.width();
      break;
    }
  }
  MaybeRecordCollapsedMetrics(label_width);
}

void PageActionView::MaybeRecordCollapsedMetrics(int label_width) {
  int preferred_width =
      GetSizeForLabelWidth(label()->GetPreferredSize().width()).width();

  base::UmaHistogramEnumeration(
      "PageActionController.ChipCollapseAnalysisCount.ActionType", type_);

  if (label_width == 0) {
    base::UmaHistogramEnumeration(
        "PageActionController.Chip.CollapsedDueToSpace.ActionType", type_);
    base::UmaHistogramCounts1000(
        "PageActionController.Chip.CollapsedDueToSpace.PreferredWidth",
        preferred_width);
  }

  chip_shown_metric_recorded_ = true;
}

SkColor PageActionView::GetBackgroundColor() const {
  if (observation_.IsObserving() &&
      observation_.GetSource()->GetOverrideBackgroundColorId().has_value() &&
      GetColorProvider()) {
    return GetColorProvider()->GetColor(
        *observation_.GetSource()->GetOverrideBackgroundColorId());
  }
  return IconLabelBubbleView::GetBackgroundColor();
}

bool PageActionView::PaintedOnSolidBackground() const {
  return IconLabelBubbleView::PaintedOnSolidBackground() ||
         IsAnchoredMessageVisible();
}

gfx::RoundedCornersF PageActionView::GetCornerRadii() const {
  if (features::IsPageActionsElevatedToolbarEnabled()) {
    const float current_height =
        height() > 0 ? static_cast<float>(height())
                     : static_cast<float>(GetPreferredSize().height());
    return gfx::RoundedCornersF(current_height / 2.0f);
  }
  return IconLabelBubbleView::GetCornerRadii();
}

void PageActionView::UpdateBackground() {
  if (features::IsPageActionsElevatedToolbarEnabled()) {
    if (!GetWidget()) {
      return;
    }

    const bool painted_on_solid_background = PaintedOnSolidBackground();
    SetBackground(painted_on_solid_background
                      ? views::CreatePillBackground(GetBackgroundColor())
                      : nullptr);
    ConfigureInkDropForRefresh2023(this,
                                   painted_on_solid_background
                                       ? kColorOmniboxIconHover
                                       : kColorOmniboxActionIconHover,
                                   kColorOmniboxIconPressed);
    UpdateLabelColors();
    return;
  }
  IconLabelBubbleView::UpdateBackground();
}

BEGIN_METADATA(PageActionView)
END_METADATA

}  // namespace page_actions
