// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/i18n/rtl.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_content_previews.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_expand_button.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_target_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// TODO(crbug.com/1097623) Many of below values are sums of each other and
// can be removed.

// Sizes are in px.
constexpr int kButtonPadding = 8;
constexpr int kButtonWidth = 92;
constexpr int kCornerRadius = 12;
constexpr int kBubbleTopPaddingFromWindow = 28;
constexpr int kDefaultBubbleWidth = 416;

// kDefaultBubbleBodyHeight = kTargetViewHeight + 2*kShortSpacing +
// SharesheetExpandButton.kHeight + kShortSpacing
constexpr int kDefaultBubbleBodyHeight = 308;

// kExpandedBubbleBodyHeight = kTargetViewHeight + kShortSpacing +
// kExpandViewPaddingTop + kExpandViewTitleLabelHeight +
// SharesheetTargetButton.kButtonHeight + kShortSpacing +
// SharesheetExpandButton.kHeight + kShortSpacing
constexpr int kExpandedBubbleBodyHeight = 450;

// kNoExtensionBubbleBodyHeight = kTargetViewHeight + kSmallSpacing +
// kNoExtensionBottomPadding
constexpr int kNoExtensionBubbleBodyHeight = 268;

constexpr int kMaxTargetsPerRow = 4;
constexpr int kMaxRowsForDefaultView = 2;

// TargetViewHeight is 2*kButtonHeight + kButtonPadding
constexpr int kTargetViewHeight = 216;
constexpr int kTargetViewExpandedHeight = 382;

constexpr int kExpandViewTitleLabelHeight = 22;
constexpr int kExpandViewPaddingTop = 16;
constexpr int kExpandViewPaddingBottom = 8;

constexpr int kShortSpacing = 20;

constexpr SkColor kShareTargetTitleColor = gfx::kGoogleGrey700;
constexpr SkColor kShareTitleColor = gfx::kGoogleGrey900;

constexpr auto kAnimateDelay = base::TimeDelta::FromMilliseconds(100);
constexpr auto kQuickAnimateTime = base::TimeDelta::FromMilliseconds(100);
constexpr auto kSlowAnimateTime = base::TimeDelta::FromMilliseconds(200);

// Resize Percentage.
constexpr int kStretchy = 1.0;

enum { kColumnSetIdTitle, kColumnSetIdTargets, kColumnSetIdZeroState };

void SetUpTargetColumnSet(views::GridLayout* layout) {
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetIdTargets);
  for (int i = 0; i < kMaxTargetsPerRow; i++) {
    cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                  views::GridLayout::ColumnSize::kFixed, kButtonWidth, 0);
  }
}

bool IsKeyboardCodeArrow(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_LEFT;
}

}  // namespace

class SharesheetBubbleView::SharesheetParentWidgetObserver
    : public views::WidgetObserver {
 public:
  SharesheetParentWidgetObserver(SharesheetBubbleView* owner,
                                 views::Widget* widget)
      : owner_(owner) {
    observer_.Observe(widget);
  }
  ~SharesheetParentWidgetObserver() override = default;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    DCHECK(observer_.IsObservingSource(widget));
    observer_.Reset();
    // |this| may be destroyed here!

    // TODO(crbug.com/1188938) Code clean up.
    // There should be something here telling SharesheetBubbleView
    // that its parent widget is closing and therefore it should
    // also close. Or we should try to inherit the widget changes from
    // BubbleDialogDelegate and not have this class here at all.
  }

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& bounds) override {
    owner_->UpdateAnchorPosition();
  }

 private:
  SharesheetBubbleView* owner_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observer_{this};
};

SharesheetBubbleView::SharesheetBubbleView(
    gfx::NativeWindow native_window,
    sharesheet::SharesheetServiceDelegate* delegate)
    : delegate_(delegate) {
  set_parent_window(native_window);
  parent_widget_observer_ = std::make_unique<SharesheetParentWidgetObserver>(
      this, views::Widget::GetWidgetForNativeWindow(native_window));
  parent_view_ =
      views::Widget::GetWidgetForNativeWindow(native_window)->GetRootView();
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  UpdateAnchorPosition();

  CreateBubble();
}

SharesheetBubbleView::~SharesheetBubbleView() = default;

void SharesheetBubbleView::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::mojom::IntentPtr intent,
    sharesheet::DeliveredCallback delivered_callback) {
  intent_ = std::move(intent);
  delivered_callback_ = std::move(delivered_callback);

  main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /* inside_border_insets */ gfx::Insets(),
      /* between_child_spacing */ 0, /* collapse_margins_spacing */ true));

  std::unique_ptr<views::Label> share_title_view =
      std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL),
          ash::CONTEXT_SHARESHEET_BUBBLE_TITLE, ash::STYLE_SHARESHEET);

  share_title_view->SetLineHeight(SharesheetBubbleView::kTitleLineHeight);
  share_title_view->SetEnabledColor(kShareTitleColor);
  share_title_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  if (targets.empty() ||
      !(base::FeatureList::IsEnabled(features::kSharesheetContentPreviews))) {
    // Only the share title is displayed if there are no targets or if the
    // content previews flag is off.
    share_title_view->SetProperty(
        views::kMarginsKey,
        gfx::Insets(SharesheetBubbleView::kSpacing, kSpacing, kSpacing,
                    SharesheetBubbleView::kSpacing));
    share_title_view_ = main_view_->AddChildView(std::move(share_title_view));
  } else {
    // Adds view for content previews including the title, text descriptor
    // and image preview.
    content_previews_ =
        main_view_->AddChildView(std::make_unique<SharesheetContentPreviews>(
            intent_->Clone(), delegate_->GetProfile(),
            std::move(share_title_view)));
  }

  if (targets.empty()) {
    auto* image =
        main_view_->AddChildView(std::make_unique<views::ImageView>());
    image->SetImage(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_SHARESHEET_EMPTY));
    image->SetProperty(views::kMarginsKey, gfx::Insets(0, 0, kSpacing, 0));
    auto* zero_state_label =
        main_view_->AddChildView(std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(IDS_SHARESHEET_ZERO_STATE_LABEL),
            ash::CONTEXT_SHARESHEET_BUBBLE_BODY, ash::STYLE_SHARESHEET));
    zero_state_label->SetLineHeight(kShortSpacing);
  } else {
    auto scroll_view = std::make_unique<views::ScrollView>();
    scroll_view->SetContents(MakeScrollableTargetView(std::move(targets)));
    scroll_view->ClipHeightTo(kTargetViewHeight, kTargetViewExpandedHeight);
    main_view_->AddChildView(std::move(scroll_view));

    expand_button_separator_ =
        main_view_->AddChildView(std::make_unique<views::Separator>());
    expand_button_ =
        main_view_->AddChildView(std::make_unique<SharesheetExpandButton>(
            base::BindRepeating(&SharesheetBubbleView::ExpandButtonPressed,
                                base::Unretained(this))));
    expand_button_->SetProperty(views::kMarginsKey,
                                gfx::Insets(kShortSpacing, kSpacing));
  }

  main_view_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  main_view_->RequestFocus();
  main_view_->GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL));
  views::BubbleDialogDelegateView::CreateBubble(this);
  GetWidget()->GetRootView()->Layout();
  RecordFormFactorMetric();
  ShowWidgetWithAnimateFadeIn();

  if (expanded_view_ == nullptr || expanded_view_->children().size() > 1) {
    SetToDefaultBubbleSizing();
  } else {
    width_ = kDefaultBubbleWidth;
    height_ = kNoExtensionBubbleBodyHeight + GetBubbleHeadHeight();
    expand_button_->SetVisible(false);
    expand_button_separator_->SetVisible(false);
  }
  UpdateAnchorPosition();
}

void SharesheetBubbleView::ShowNearbyShareBubble(
    apps::mojom::IntentPtr intent,
    sharesheet::DeliveredCallback delivered_callback) {
  ShowBubble({}, std::move(intent), std::move(delivered_callback));
  if (delivered_callback_) {
    std::move(delivered_callback_).Run(sharesheet::SharesheetResult::kSuccess);
  }
  delegate_->OnTargetSelected(
      l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME),
      sharesheet::TargetType::kAction, std::move(intent_), share_action_view_);
}

std::unique_ptr<views::View> SharesheetBubbleView::MakeScrollableTargetView(
    std::vector<TargetInfo> targets) {
  // Set up default and expanded views.
  auto default_view = std::make_unique<views::View>();
  default_view->SetProperty(views::kMarginsKey, gfx::Insets(0, kSpacing));
  auto* default_layout =
      default_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  SetUpTargetColumnSet(default_layout);

  auto expanded_view = std::make_unique<views::View>();
  expanded_view->SetProperty(views::kMarginsKey, gfx::Insets(0, kSpacing));
  auto* expanded_layout =
      expanded_view->SetLayoutManager(std::make_unique<views::GridLayout>());
  SetUpTargetColumnSet(expanded_layout);
  views::ColumnSet* cs_expanded_view =
      expanded_layout->AddColumnSet(kColumnSetIdTitle);
  cs_expanded_view->AddColumn(/* h_align */ views::GridLayout::FILL,
                              /* v_align */ views::GridLayout::CENTER,
                              /* resize_percent */ kStretchy,
                              views::GridLayout::ColumnSize::kUsePreferred,
                              /* fixed_width */ 0, /* min_width */ 0);
  // Add Extended View Title
  expanded_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                 kExpandViewPaddingTop);
  expanded_layout->StartRow(views::GridLayout::kFixedSize, kColumnSetIdTitle,
                            kExpandViewTitleLabelHeight);
  auto* apps_list_label =
      expanded_layout->AddView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_SHARESHEET_APPS_LIST_LABEL),
          ash::CONTEXT_SHARESHEET_BUBBLE_BODY, ash::STYLE_SHARESHEET));
  apps_list_label->SetLineHeight(kExpandViewTitleLabelHeight);
  apps_list_label->SetEnabledColor(kShareTargetTitleColor);
  apps_list_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  expanded_layout->AddPaddingRow(views::GridLayout::kFixedSize,
                                 kExpandViewPaddingBottom);

  PopulateLayoutsWithTargets(std::move(targets), default_layout,
                             expanded_layout);
  default_layout->AddPaddingRow(views::GridLayout::kFixedSize, kShortSpacing);

  auto scrollable_view = std::make_unique<views::View>();
  auto* layout =
      scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  default_view_ = scrollable_view->AddChildView(std::move(default_view));
  expanded_view_separator_ =
      scrollable_view->AddChildView(std::make_unique<views::Separator>());
  expanded_view_separator_->SetProperty(views::kMarginsKey,
                                        gfx::Insets(0, kSpacing));
  expanded_view_ = scrollable_view->AddChildView(std::move(expanded_view));

  // Expanded view is not visible by default.
  expanded_view_->SetVisible(false);
  expanded_view_separator_->SetVisible(false);

  return scrollable_view;
}

void SharesheetBubbleView::PopulateLayoutsWithTargets(
    std::vector<TargetInfo> targets,
    views::GridLayout* default_layout,
    views::GridLayout* expanded_layout) {
  // Add first kMaxRowsForDefaultView*kMaxTargetsPerRow targets to
  // |default_view| and subsequent targets to |expanded_view|.
  size_t row_count = 0;
  size_t target_counter = 0;
  auto* layout_for_target = default_layout;
  for (auto& target : targets) {
    if (target_counter % kMaxTargetsPerRow == 0) {
      // When we've reached kMaxRowsForDefaultView switch to populating
      // |expanded_layout|.
      if (row_count == kMaxRowsForDefaultView) {
        layout_for_target = expanded_layout;
        // Do not add a padding row if we are at the first row of
        // |default_layout| or |expanded_layout|.
      } else if (row_count != 0) {
        layout_for_target->AddPaddingRow(views::GridLayout::kFixedSize,
                                         kButtonPadding);
      }
      ++row_count;
      layout_for_target->StartRow(views::GridLayout::kFixedSize,
                                  kColumnSetIdTargets);
    }
    ++target_counter;

    // Make a copy because value is needed after target is std::moved below.
    std::u16string display_name = target.display_name;
    std::u16string secondary_display_name =
        target.secondary_display_name.value_or(std::u16string());
    base::Optional<gfx::ImageSkia> icon = target.icon;

    auto target_view = std::make_unique<SharesheetTargetButton>(
        base::BindRepeating(&SharesheetBubbleView::TargetButtonPressed,
                            base::Unretained(this),
                            base::Passed(std::move(target))),
        display_name, secondary_display_name, icon,
        delegate_->GetVectorIcon(display_name));

    layout_for_target->AddView(std::move(target_view));
  }
}

void SharesheetBubbleView::ShowActionView() {
  constexpr float kShareActionScaleUpFactor = 0.9f;

  main_view_->SetPaintToLayer();
  ui::Layer* main_view_layer = main_view_->layer();
  main_view_layer->SetFillsBoundsOpaquely(false);
  main_view_layer->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));
  // |main_view_| opacity fade out.
  auto scoped_settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
      main_view_layer->GetAnimator());
  scoped_settings->SetTransitionDuration(kQuickAnimateTime);
  scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);
  main_view_layer->SetOpacity(0.0f);
  main_view_->SetVisible(false);

  share_action_view_->SetPaintToLayer();
  ui::Layer* share_action_view_layer = share_action_view_->layer();
  share_action_view_layer->SetFillsBoundsOpaquely(false);
  share_action_view_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadius));

  share_action_view_->SetVisible(true);
  share_action_view_layer->SetOpacity(0.0f);
  gfx::Transform transform = gfx::GetScaleTransform(
      gfx::Rect(share_action_view_layer->size()).CenterPoint(),
      kShareActionScaleUpFactor);
  share_action_view_layer->SetTransform(transform);
  auto share_action_scoped_settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(
          share_action_view_layer->GetAnimator());
  share_action_scoped_settings->SetPreemptionStrategy(
      ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);

  // |share_action_view_| scale fade in.
  share_action_scoped_settings->SetTransitionDuration(kSlowAnimateTime);
  share_action_scoped_settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN_2);
  // Set##name kicks off the animation with the TransitionDuration and
  // TweenType currently set. See ui/compositor/layer_animator.cc Set##name.
  share_action_view_layer->SetTransform(gfx::Transform());
  // |share_action_view_| opacity fade in.
  share_action_scoped_settings->SetTransitionDuration(kQuickAnimateTime);
  share_action_scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);
  share_action_view_layer->SetOpacity(1.0f);

  // Delay |share_action_view_| animate so that we can see |main_view_| fade out
  // first.
  share_action_view_layer->GetAnimator()->SchedulePauseForProperties(
      kAnimateDelay, ui::LayerAnimationElement::TRANSFORM |
                         ui::LayerAnimationElement::OPACITY);
}

void SharesheetBubbleView::ResizeBubble(const int& width, const int& height) {
  auto old_bounds = gfx::RectF(width_, height_);
  width_ = width;
  height_ = height;

  // Animate from the old bubble to the new bubble.
  ui::Layer* layer = View::GetWidget()->GetLayer();
  const gfx::Transform transform =
      gfx::TransformBetweenRects(old_bounds, gfx::RectF(width, height));
  layer->SetTransform(transform);
  auto scoped_settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());
  scoped_settings->SetTransitionDuration(kSlowAnimateTime);
  scoped_settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN_2);
  layer->GetAnimator()->SchedulePauseForProperties(
      kAnimateDelay, ui::LayerAnimationElement::TRANSFORM);

  UpdateAnchorPosition();

  layer->SetTransform(gfx::Transform());
}

// CloseBubble is called from a ShareAction or after an app launches.
void SharesheetBubbleView::CloseBubble() {
  if (!is_bubble_closing_) {
    CloseWidgetWithAnimateFadeOut(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }
}

bool SharesheetBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // We override this because when this is handled by the base class,
  // OnKeyPressed is not invoked when a user presses |VKEY_ESCAPE| if they have
  // not pressed |VKEY_TAB| first to focus the SharesheetBubbleView.
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  if (share_action_view_->GetVisible() &&
      delegate_->OnAcceleratorPressed(accelerator, active_target_)) {
    return true;
  }
  // If delivered_callback_ is not null at this point, then the sharesheet was
  // closed before a target was selected.
  if (delivered_callback_) {
    std::move(delivered_callback_).Run(sharesheet::SharesheetResult::kCancel);
  }
  escape_pressed_ = true;
  sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(
      sharesheet::SharesheetMetrics::UserAction::kCancelledThroughEscPress);
  CloseWidgetWithAnimateFadeOut(views::Widget::ClosedReason::kEscKeyPressed);

  return true;
}

bool SharesheetBubbleView::OnKeyPressed(const ui::KeyEvent& event) {
  // Ignore key press if it's not an arrow or bubble is closing.
  if (!IsKeyboardCodeArrow(event.key_code()) || default_view_ == nullptr ||
      is_bubble_closing_) {
    if (event.key_code() == ui::VKEY_ESCAPE && !is_bubble_closing_) {
      escape_pressed_ = true;
    }
    return false;
  }

  int delta = 0;
  switch (event.key_code()) {
    case ui::VKEY_UP:
      delta = -kMaxTargetsPerRow;
      break;
    case ui::VKEY_DOWN:
      delta = kMaxTargetsPerRow;
      break;
    case ui::VKEY_LEFT:
      delta = base::i18n::IsRTL() ? 1 : -1;
      break;
    case ui::VKEY_RIGHT:
      delta = base::i18n::IsRTL() ? -1 : 1;
      break;
    default:
      NOTREACHED();
      break;
  }

  const size_t default_views = default_view_->children().size();
  // The -1 here and +1 below account for the app list label.
  const size_t targets =
      default_views +
      (show_expanded_view_ ? (expanded_view_->children().size() - 1) : 0);
  const int new_target = int{keyboard_highlighted_target_} + delta;
  keyboard_highlighted_target_ =
      size_t{base::ClampToRange(new_target, 0, int{targets} - 1)};

  if (keyboard_highlighted_target_ < default_views) {
    default_view_->children()[keyboard_highlighted_target_]->RequestFocus();
  } else {
    expanded_view_->children()[keyboard_highlighted_target_ + 1 - default_views]
        ->RequestFocus();
  }
  return true;
}

ax::mojom::Role SharesheetBubbleView::GetAccessibleWindowRole() {
  // We override the role because the base class sets it to alert dialog.
  // This would make screen readers repeatedly announce the whole of the
  // |sharesheet_bubble_view| which is undesirable.
  return ax::mojom::Role::kDialog;
}

std::unique_ptr<views::NonClientFrameView>
SharesheetBubbleView::CreateNonClientFrameView(views::Widget* widget) {
  // TODO(crbug.com/1097623) Replace this with layer->SetRoundedCornerRadius.
  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow(), color());
  bubble_border->SetCornerRadius(kCornerRadius);
  auto frame =
      views::BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

gfx::Size SharesheetBubbleView::CalculatePreferredSize() const {
  return gfx::Size(width_, height_);
}

void SharesheetBubbleView::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  // Catch widgets that are closing due to the user clicking out of the bubble.
  // If |user_selection_made_| we should not close the bubble here as it will be
  // closed in a different code path.
  if (!active && !user_selection_made_ && !is_bubble_closing_) {
    if (delivered_callback_) {
      std::move(delivered_callback_).Run(sharesheet::SharesheetResult::kCancel);
    }
    auto user_action =
        sharesheet::SharesheetMetrics::UserAction::kCancelledThroughClickingOut;
    auto closed_reason = views::Widget::ClosedReason::kLostFocus;
    if (escape_pressed_) {
      user_action =
          sharesheet::SharesheetMetrics::UserAction::kCancelledThroughEscPress;
      closed_reason = views::Widget::ClosedReason::kEscKeyPressed;
    }
    sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(user_action);
    CloseWidgetWithAnimateFadeOut(closed_reason);
  }
}

void SharesheetBubbleView::CreateBubble() {
  set_close_on_deactivate(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Margins must be set to 0 or share_action_view will have undesired margins.
  set_margins(gfx::Insets());

  auto main_view = std::make_unique<views::View>();
  main_view_ = AddChildView(std::move(main_view));

  auto share_action_view = std::make_unique<views::View>();
  share_action_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0, true));
  share_action_view_ = AddChildView(std::move(share_action_view));
  share_action_view_->SetVisible(false);
}

void SharesheetBubbleView::ExpandButtonPressed() {
  show_expanded_view_ = !show_expanded_view_;
  ResizeBubble(kDefaultBubbleWidth, GetBubbleHeight());

  // Scrollview has separators that overlaps with |expand_button_separator_|
  // to create a double line when both are visible, so when scrollview is
  // expanded we hide our separator.
  expand_button_separator_->SetVisible(!show_expanded_view_);
  expanded_view_->SetVisible(show_expanded_view_);
  expanded_view_separator_->SetVisible(show_expanded_view_);

  if (show_expanded_view_) {
    expand_button_->SetToExpandedState();
    AnimateToExpandedState();
  } else {
    expand_button_->SetToDefaultState();
  }
}

void SharesheetBubbleView::AnimateToExpandedState() {
  expanded_view_->SetVisible(true);
  expanded_view_->SetPaintToLayer();
  ui::Layer* expanded_view_layer = expanded_view_->layer();
  expanded_view_layer->SetFillsBoundsOpaquely(false);
  expanded_view_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kCornerRadius));
  expanded_view_layer->SetOpacity(0.0f);
  // |expanded_view_| opacity fade in.
  auto scoped_settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
      expanded_view_layer->GetAnimator());
  scoped_settings->SetTransitionDuration(kQuickAnimateTime);
  scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);

  expanded_view_layer->SetOpacity(1.0f);
}

void SharesheetBubbleView::TargetButtonPressed(TargetInfo target) {
  user_selection_made_ = true;
  auto type = target.type;
  if (type == sharesheet::TargetType::kAction) {
    active_target_ = target.launch_name;
  } else {
    intent_->activity_name = target.activity_name;
  }
  delegate_->OnTargetSelected(target.launch_name, type, std::move(intent_),
                              share_action_view_);
  if (delivered_callback_) {
    std::move(delivered_callback_).Run(sharesheet::SharesheetResult::kSuccess);
  }
  intent_.reset();
}

void SharesheetBubbleView::UpdateAnchorPosition() {
  // If |width_| is not set, set to default value.
  if (width_ == 0) {
    SetToDefaultBubbleSizing();
  }

  // Horizontally centered
  int x_within_parent_view = parent_view_->GetMirroredXInView(
      (parent_view_->bounds().width() - width_) / 2);
  // Get position in screen, taking parent view origin into account. This is
  // 0,0 in fullscreen on the primary display, but not on secondary displays, or
  // in Hosted App windows.
  gfx::Point origin = parent_view_->GetBoundsInScreen().origin();
  origin += gfx::Vector2d(x_within_parent_view, kBubbleTopPaddingFromWindow);

  // SetAnchorRect will CalculatePreferredSize when called.
  SetAnchorRect(gfx::Rect(origin, gfx::Size()));
}

void SharesheetBubbleView::SetToDefaultBubbleSizing() {
  width_ = kDefaultBubbleWidth;
  height_ = GetBubbleHeight();
}

void SharesheetBubbleView::ShowWidgetWithAnimateFadeIn() {
  constexpr float kSharesheetScaleUpFactor = 0.8f;
  constexpr auto kSharesheetScaleUpTime =
      base::TimeDelta::FromMilliseconds(150);

  views::Widget* widget = View::GetWidget();
  ui::Layer* layer = widget->GetLayer();

  layer->SetOpacity(0.0f);
  widget->ShowInactive();
  gfx::Transform transform = gfx::GetScaleTransform(
      gfx::Rect(layer->size()).CenterPoint(), kSharesheetScaleUpFactor);
  layer->SetTransform(transform);
  auto scoped_settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());

  scoped_settings->SetTransitionDuration(kSharesheetScaleUpTime);
  scoped_settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  layer->SetTransform(gfx::Transform());

  scoped_settings->SetTransitionDuration(kQuickAnimateTime);
  scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);
  layer->SetOpacity(1.0f);
  widget->Activate();
}

void SharesheetBubbleView::CloseWidgetWithAnimateFadeOut(
    views::Widget::ClosedReason closed_reason) {
  constexpr auto kSharesheetOpacityFadeOutTime =
      base::TimeDelta::FromMilliseconds(80);

  is_bubble_closing_ = true;
  ui::Layer* layer = View::GetWidget()->GetLayer();

  auto scoped_settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());
  scoped_settings->SetTweenType(gfx::Tween::Type::LINEAR);
  scoped_settings->SetTransitionDuration(kSharesheetOpacityFadeOutTime);
  // This aborts any running animations and starts the current one.
  scoped_settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer->SetOpacity(0.0f);
  // We are closing the native widget during the close animation which results
  // in destroying the layer and the animation and the observer not calling
  // back. Thus it is safe to use base::Unretained here.
  scoped_settings->AddObserver(new ui::ClosureAnimationObserver(
      base::BindOnce(&SharesheetBubbleView::CloseWidgetWithReason,
                     base::Unretained(this), closed_reason)));
}

void SharesheetBubbleView::CloseWidgetWithReason(
    views::Widget::ClosedReason closed_reason) {
  View::GetWidget()->CloseWithReason(closed_reason);

  // Bubble is deleted here.
  delegate_->OnBubbleClosed(active_target_);
}

// TODO(crbug.com/1097623): Rename this function.
int SharesheetBubbleView::GetBubbleHeight() {
  int height = (show_expanded_view_ ? kExpandedBubbleBodyHeight
                                    : kDefaultBubbleBodyHeight) +
               GetBubbleHeadHeight();
  return height;
}

int SharesheetBubbleView::GetBubbleHeadHeight() {
  // |head_height| is the max height of |content_previews_| and
  // |share_title_view_|.
  int head_height = 0;
  if (content_previews_) {
    // The bubble height is increased by the height of the additional lines from
    // text preview.
    head_height = content_previews_->GetTitleViewHeight();
  }
  if (share_title_view_) {
    head_height = share_title_view_->GetProperty(views::kMarginsKey)->height() +
                  share_title_view_->GetPreferredSize().height();
  }
  return head_height;
}

void SharesheetBubbleView::RecordFormFactorMetric() {
  auto form_factor =
      ash::TabletMode::Get()->InTabletMode()
          ? sharesheet::SharesheetMetrics::FormFactor::kTablet
          : sharesheet::SharesheetMetrics::FormFactor::kClamshell;
  sharesheet::SharesheetMetrics::RecordSharesheetFormFactor(form_factor);
}

BEGIN_METADATA(SharesheetBubbleView, views::BubbleDialogDelegateView)
END_METADATA
