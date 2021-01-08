// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/i18n/rtl.h"
#include "base/scoped_observation.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegate.h"
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
constexpr int kBubbleTopPaddingFromWindow = 36;
constexpr int kDefaultBubbleWidth = 416;
constexpr int kNoExtensionBubbleHeight = 340;
constexpr int kDefaultBubbleHeight = 380;
constexpr int kExpandedBubbleHeight = 522;
constexpr int kMaxTargetsPerRow = 4;
constexpr int kMaxRowsForDefaultView = 2;

// TargetViewHeight is 2*kButtonHeight + kButtonPadding
constexpr int kTargetViewHeight = 216;
constexpr int kTargetViewExpandedHeight = 382;

constexpr int kExpandViewTitleLabelHeight = 22;
constexpr int kExpandViewPaddingTop = 16;
constexpr int kExpandViewPaddingBottom = 8;

constexpr int kShortSpacing = 20;
constexpr int kSpacing = 24;
constexpr int kTitleLineHeight = 24;

constexpr SkColor kShareTitleColor = gfx::kGoogleGrey900;
constexpr SkColor kShareTargetTitleColor = gfx::kGoogleGrey700;

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
  UpdateAnchorPosition();

  CreateBubble();
}

SharesheetBubbleView::~SharesheetBubbleView() = default;

void SharesheetBubbleView::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::mojom::IntentPtr intent,
    sharesheet::CloseCallback close_callback) {
  intent_ = std::move(intent);
  close_callback_ = std::move(close_callback);

  main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /* inside_border_insets */ gfx::Insets(),
      /* between_child_spacing */ 0, /* collapse_margins_spacing */ true));

  // Add Title label
  auto* title = main_view_->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL),
      ash::CONTEXT_SHARESHEET_BUBBLE_TITLE, ash::STYLE_SHARESHEET));
  title->SetLineHeight(kTitleLineHeight);
  title->SetEnabledColor(kShareTitleColor);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetProperty(views::kMarginsKey, gfx::Insets(kSpacing));

  // Add content preview text descriptor.
  if (base::FeatureList::IsEnabled(features::kSharesheetContentPreviews) &&
      (intent_->file_urls.has_value())) {
    // Remove the margin under the title for file_title
    title->SetProperty(views::kMarginsKey,
                       gfx::Insets(kSpacing, kSpacing, 0, kSpacing));
    // This displays text data only for the first file_url provided.
    auto* file_title = main_view_->AddChildView(std::make_unique<views::Label>(
        base::ASCIIToUTF16(
            (intent_->file_urls.value().front().ExtractFileName())),
        ash::CONTEXT_SHARESHEET_BUBBLE_BODY_SECONDARY));
    file_title->SetLineHeight(kTitleLineHeight);
    file_title->SetEnabledColor(kShareTargetTitleColor);
    file_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    file_title->SetProperty(views::kMarginsKey,
                            gfx::Insets(3, kSpacing, kSpacing, kSpacing));
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
    height_ = kNoExtensionBubbleHeight;
    expand_button_->SetVisible(false);
    expand_button_separator_->SetVisible(false);
  }
  UpdateAnchorPosition();

  // Expanding the sharesheet is needed for content previews
  if (base::FeatureList::IsEnabled(features::kSharesheetContentPreviews)) {
    ResizeBubble(kDefaultBubbleWidth, GetBubbleHeight());
  }
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
    base::string16 display_name = target.display_name;
    base::string16 secondary_display_name =
        target.secondary_display_name.value_or(base::string16());
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

// This function is called from a ShareAction or after an app launches.
void SharesheetBubbleView::CloseBubble() {
  if (!is_bubble_closing_) {
    CloseWidgetWithAnimateFadeOut(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }
}

void SharesheetBubbleView::OnKeyEvent(ui::KeyEvent* event) {
  // Ignore key press if bubble is closing.
  // TODO(crbug.com/1141741) Update to OnKeyPressed.
  if (!IsKeyboardCodeArrow(event->key_code()) ||
      event->type() != ui::ET_KEY_RELEASED || default_view_ == nullptr ||
      is_bubble_closing_) {
    if (event->key_code() == ui::VKEY_ESCAPE && !is_bubble_closing_)
      escape_pressed_ = true;
    View::OnKeyEvent(event);
    return;
  }

  int delta = 0;
  switch (event->key_code()) {
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

  View::OnKeyEvent(event);
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
    if (close_callback_) {
      std::move(close_callback_).Run(sharesheet::SharesheetResult::kCancel);
    }
    auto action = escape_pressed_ ? sharesheet::SharesheetMetrics::UserAction::
                                        kCancelledThroughEscPress
                                  : sharesheet::SharesheetMetrics::UserAction::
                                        kCancelledThroughClickingOut;
    sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(action);
    CloseWidgetWithAnimateFadeOut(views::Widget::ClosedReason::kLostFocus);
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
  if (type == sharesheet::TargetType::kAction)
    active_target_ = target.launch_name;
  else
    intent_->activity_name = target.activity_name;
  delegate_->OnTargetSelected(target.launch_name, type, std::move(intent_),
                              share_action_view_);
  intent_.reset();
  if (close_callback_)
    std::move(close_callback_).Run(sharesheet::SharesheetResult::kSuccess);
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

int SharesheetBubbleView::GetBubbleHeight() {
  int height =
      show_expanded_view_ ? kExpandedBubbleHeight : kDefaultBubbleHeight;

  if (base::FeatureList::IsEnabled(features::kSharesheetContentPreviews)) {
    height += kTitleLineHeight + 3;
  }
  return height;
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
