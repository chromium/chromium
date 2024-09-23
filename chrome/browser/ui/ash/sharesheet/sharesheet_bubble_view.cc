// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegator.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_constants.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_expand_button.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_header_view.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_target_button.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/sharesheet/constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/image/image_skia.h"
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
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// TODO(crbug.com/40136695) Many of below values are sums of each other and
// can be removed.

// Sizes are in px.
constexpr int kButtonWidth = 92;
constexpr int kCornerRadius = 12;
constexpr int kBubbleTopPaddingFromWindow = 28;

constexpr int kMaxTargetsPerRow = 4;
constexpr int kMaxRowsForDefaultView = 2;

// TargetViewHeight is 2*kButtonHeight + kButtonPadding
constexpr int kTargetViewHeight = 216;
// TargetViewExpandedHeight is default_view_->GetPreferredSize().height() + apps
// list text + 2*kExpandedViewPaddingTop + expanded_view_->FirstRow().height().
// TODO(crbug.com/40136695): Update this to a layout that will allow us to get
// the height of the first row.
constexpr int kTargetViewExpandedHeight = 382;

constexpr int kExpandViewPaddingTop = 16;
constexpr int kExpandViewPaddingBottom = 8;

constexpr int kShortSpacing = 10;

constexpr auto kAnimateDelay = base::Milliseconds(100);
constexpr auto kQuickAnimateTime = base::Milliseconds(100);
constexpr auto kSlowAnimateTime = base::Milliseconds(200);

void SetUpTargetColumns(views::TableLayoutView* view) {
  for (int i = 0; i < kMaxTargetsPerRow; i++) {
    view->AddColumn(views::LayoutAlignment::kCenter,
                    views::LayoutAlignment::kStart, 0,
                    views::TableLayout::ColumnSize::kFixed, kButtonWidth, 0);
  }
}

bool IsKeyboardCodeArrow(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_LEFT;
}

void RecordMimeTypeMetric(const apps::IntentPtr& intent) {
  auto mime_types_to_record =
      ::sharesheet::SharesheetMetrics::GetMimeTypesFromIntentForMetrics(intent);
  for (auto& mime_type : mime_types_to_record) {
    ::sharesheet::SharesheetMetrics::RecordSharesheetMimeType(mime_type);
  }
}

}  // namespace

namespace ash {
namespace sharesheet {

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

    // TODO(crbug.com/40173521) Code clean up.
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
  raw_ptr<SharesheetBubbleView> owner_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observer_{this};
};

SharesheetBubbleView::SharesheetBubbleView(
    gfx::NativeWindow native_window,
    ::sharesheet::SharesheetServiceDelegator* delegator)
    : BubbleDialogDelegateView(nullptr,
                               views::BubbleBorder::TOP_LEFT,
                               views::BubbleBorder::DIALOG_SHADOW,
                               true),
      delegator_(delegator) {
  CHECK(native_window);
  CHECK(delegator_);

  SetID(SHARESHEET_BUBBLE_VIEW_ID);
  // We set the dialog role because views::BubbleDialogDelegate defaults this to
  // an alert dialog. This would make screen readers announce all of this dialog
  // which is undesirable.
  SetAccessibleWindowRole(ax::mojom::Role::kDialog);
  SetAccessibleTitle(l10n_util::GetStringUTF16(IDS_SHARESHEET_TITLE_LABEL));
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  set_parent_window(native_window);
  views::Widget* const widget =
      views::Widget::GetWidgetForNativeWindow(native_window);
  CHECK(widget);
  parent_view_ = widget->GetRootView();
  parent_widget_observer_ =
      std::make_unique<SharesheetParentWidgetObserver>(this, widget);

  InitBubble();
}

SharesheetBubbleView::~SharesheetBubbleView() {
  // TODO(crbug.com/40057260): While this is harmless, it should not be
  // necessary unless something fishy is happening with the behavior of layer
  // animations around widget teardown.
  if (close_callback_) {
    std::move(close_callback_).Run(views::Widget::ClosedReason::kUnspecified);
  }

  display::Screen::GetScreen()->RemoveObserver(this);
}

void SharesheetBubbleView::ShowBubble(
    std::vector<TargetInfo> targets,
    apps::IntentPtr intent,
    ::sharesheet::DeliveredCallback delivered_callback,
    ::sharesheet::CloseCallback close_callback) {
  intent_ = std::move(intent);
  delivered_callback_ = std::move(delivered_callback);
  close_callback_ = std::move(close_callback);

  main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  header_view_ =
      main_view_->AddChildView(std::make_unique<SharesheetHeaderView>(
          intent_->Clone(), delegator_->GetProfile()));
  body_view_ = main_view_->AddChildView(std::make_unique<views::View>());
  body_view_->SetID(BODY_VIEW_ID);
  body_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  footer_view_ = main_view_->AddChildView(std::make_unique<views::View>());
  footer_view_->SetID(FOOTER_VIEW_ID);
  auto* footer_layout =
      footer_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(kFooterDefaultVerticalPadding, 0)));
  footer_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  footer_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // There is always at least 1 target as Copy To Clipboard is always visible.
  CHECK_GT(targets.size(), 0u);
  header_body_separator_ =
      body_view_->AddChildView(std::make_unique<views::Separator>());
  header_body_separator_->SetColorId(cros_tokens::kCrosSysSeparator);

  const size_t targets_size = targets.size();
  auto scroll_view = std::make_unique<views::ScrollView>();
  scroll_view->SetContents(MakeScrollableTargetView(std::move(targets)));
  scroll_view->ClipHeightTo(kTargetViewHeight, kTargetViewExpandedHeight);
  body_view_->AddChildView(std::move(scroll_view));

  if (expanded_view_) {
    body_footer_separator_ =
        body_view_->AddChildView(std::make_unique<views::Separator>());
    body_footer_separator_->SetColorId(cros_tokens::kCrosSysSeparator);
    expand_button_ =
        footer_view_->AddChildView(std::make_unique<SharesheetExpandButton>(
            base::BindRepeating(&SharesheetBubbleView::ExpandButtonPressed,
                                base::Unretained(this))));
  } else if (targets_size <= kMaxTargetsPerRow * kMaxRowsForDefaultView) {
    // When we have between 1 and 8 targets inclusive. Update |footer_layout|
    // padding.
    footer_layout->set_inside_border_insets(
        gfx::Insets::VH(kFooterNoExtensionVerticalPadding, 0));
  }

  SetUpAndShowBubble();
}

void SharesheetBubbleView::ShowNearbyShareBubbleForArc(
    apps::IntentPtr intent,
    ::sharesheet::DeliveredCallback delivered_callback,
    ::sharesheet::CloseCallback close_callback) {
  // Disable close when clicking outside bubble for Nearby Share.
  close_on_deactivate_ = false;
  close_callback_ = std::move(close_callback);
  intent_ = std::move(intent);

  // Set up the bubble so that the nearby share dialog can be triggered within
  // the sharesheet.
  SetUpAndShowBubble();

  if (delivered_callback) {
    std::move(delivered_callback).Run(::sharesheet::SharesheetResult::kSuccess);
  }

  // When the Nearby Share target is shown, it will transform from the original
  // sharesheet bubble to the nearby share dialog. This animation requires an
  // original rectangle to transform from, so the size of the bubble cannot be
  // 0. In this instance, we have not populated the sharesheet with anything, as
  // it'll never be shown, so the dynamic sizing will set the height to 0. To
  // get around that, we set the height to 1, so there is a starting rectangle
  // to transform from.
  //
  // Having a height of "1" means that the animation for showing Nearby Share
  // from ARC++ is mostly a vertical expansion, instead of how it looks in a
  // normal sharesheet where there's a slight vertical and slight horizontal
  // change. We could try calculate the correct "empty" size of the sharesheet
  // and use that instead for a more consistent UI experience.
  height_ = 1;

  delegator_->OnTargetSelected(
      /*type=*/::sharesheet::TargetType::kAction,
      /*share_action_type=*/::sharesheet::ShareActionType::kNearbyShare,
      /*app_name=*/std::nullopt, /*intent=*/std::move(intent_),
      /*share_action_view=*/share_action_view_);
}

std::unique_ptr<views::View> SharesheetBubbleView::MakeScrollableTargetView(
    std::vector<TargetInfo> targets) {
  // Set up default and expanded views.
  auto default_view = std::make_unique<views::TableLayoutView>();
  default_view->SetProperty(views::kMarginsKey, gfx::Insets::VH(0, kSpacing));
  SetUpTargetColumns(default_view.get());
  default_view->AddPaddingRow(views::TableLayout::kFixedSize, kShortSpacing);

  std::unique_ptr<views::BoxLayoutView> expanded_view_container;
  views::TableLayoutView* expanded_view_table = nullptr;
  if (targets.size() > kMaxTargetsPerRow * kMaxRowsForDefaultView) {
    expanded_view_container = std::make_unique<views::BoxLayoutView>();
    expanded_view_container->SetProperty(views::kMarginsKey,
                                         gfx::Insets::VH(0, kSpacing));
    expanded_view_container->SetOrientation(
        views::BoxLayout::Orientation::kVertical);

    expanded_view_container
        ->AddChildView(CreateShareLabel(
            l10n_util::GetStringUTF16(IDS_SHARESHEET_APPS_LIST_LABEL),
            TypographyToken::kCrosHeadline1, cros_tokens::kCrosSysOnSurface,
            gfx::ALIGN_CENTER))
        ->SetProperty(views::kMarginsKey,
                      gfx::Insets::TLBR(kExpandViewPaddingTop, 0,
                                        kExpandViewPaddingBottom, 0));

    expanded_view_table = expanded_view_container->AddChildView(
        std::make_unique<views::TableLayoutView>());
    SetUpTargetColumns(expanded_view_table);
  }

  PopulateLayoutsWithTargets(std::move(targets), default_view.get(),
                             expanded_view_table);
  default_view->AddPaddingRow(views::TableLayout::kFixedSize, kShortSpacing);

  auto scrollable_view = std::make_unique<views::View>();
  auto* layout =
      scrollable_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  default_view_ = scrollable_view->AddChildView(std::move(default_view));
  default_view_->SetID(TARGETS_DEFAULT_VIEW_ID);
  if (expanded_view_container) {
    expanded_view_separator_ =
        scrollable_view->AddChildView(std::make_unique<views::Separator>());
    expanded_view_separator_->SetColorId(cros_tokens::kCrosSysSeparator);
    expanded_view_separator_->SetProperty(views::kMarginsKey,
                                          gfx::Insets::VH(0, kSpacing));
    expanded_view_ =
        scrollable_view->AddChildView(std::move(expanded_view_container));
    // |expanded_view_| is not visible by default.
    expanded_view_->SetVisible(false);
    expanded_view_separator_->SetVisible(false);
  }

  return scrollable_view;
}

void SharesheetBubbleView::PopulateLayoutsWithTargets(
    std::vector<TargetInfo> targets,
    views::TableLayoutView* default_view,
    views::TableLayoutView* expanded_view) {
  // Add first kMaxRowsForDefaultView*kMaxTargetsPerRow targets to
  // |default_view| and subsequent targets to |expanded_view|.
  size_t row_count = 0;
  size_t target_counter = 0;
  auto* view_for_target = default_view;
  for (auto& target : targets) {
    if (target_counter % kMaxTargetsPerRow == 0) {
      // When we've reached kMaxRowsForDefaultView switch to populating
      // |expanded_layout|.
      if (row_count == kMaxRowsForDefaultView) {
        DCHECK(expanded_view);
        view_for_target = expanded_view;
      }
      ++row_count;
      view_for_target->AddRows(1, views::TableLayout::kFixedSize);
    }
    ++target_counter;

    // Make a copy because value is needed after target is std::moved below.
    std::u16string display_name = target.display_name;
    std::u16string secondary_display_name =
        target.secondary_display_name.value_or(std::u16string());

    // Only apps are expected to have an |icon|, while share actions will
    // have a vector icon.
    std::optional<gfx::ImageSkia> icon = target.icon;
    const gfx::VectorIcon* vector_icon =
        delegator_->GetVectorIcon(target.share_action_type);

    view_for_target->AddChildView(std::make_unique<SharesheetTargetButton>(
        base::BindRepeating(&SharesheetBubbleView::TargetButtonPressed,
                            base::Unretained(this), target),
        display_name, secondary_display_name, icon, vector_icon,
        target.is_dlp_blocked));
  }
}

void SharesheetBubbleView::ShowActionView() {
  close_on_deactivate_ = false;
  constexpr float kShareActionScaleUpFactor = 0.9f;
  constexpr auto kShareActionScaleUpTime = base::Milliseconds(50);

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
  share_action_scoped_settings->SetTransitionDuration(kShareActionScaleUpTime);
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
void SharesheetBubbleView::CloseBubble(views::Widget::ClosedReason reason) {
  CloseWidgetWithAnimateFadeOut(reason);
}

bool SharesheetBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // We override this because when this is handled by the base class,
  // OnKeyPressed is not invoked when a user presses |VKEY_ESCAPE| if they have
  // not pressed |VKEY_TAB| first to focus the SharesheetBubbleView.
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  if (share_action_view_->GetVisible() &&
      active_share_action_type_.has_value() &&
      delegator_->OnAcceleratorPressed(accelerator,
                                       active_share_action_type_.value())) {
    return true;
  }

  // If the bubble is already in the process of closing, return early without
  // doing anything.
  if (is_bubble_closing_) {
    return true;
  }

  // If delivered_callback_ is not null at this point, then the sharesheet was
  // closed before a target was selected.
  if (delivered_callback_) {
    std::move(delivered_callback_).Run(::sharesheet::SharesheetResult::kCancel);
  }
  escape_pressed_ = true;
  ::sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(
      ::sharesheet::SharesheetMetrics::UserAction::kCancelledThroughEscPress);
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
      NOTREACHED_IN_MIGRATION();
      break;
  }

  const size_t default_views = default_view_->children().size();
  auto* expanded_view_table =
      show_expanded_view_ ? expanded_view_->children()[1].get() : nullptr;
  const size_t targets =
      default_views +
      (show_expanded_view_ ? expanded_view_table->children().size() : 0);
  const int new_target = static_cast<int>(keyboard_highlighted_target_) + delta;
  keyboard_highlighted_target_ = static_cast<size_t>(
      std::clamp(new_target, 0, static_cast<int>(targets) - 1));

  if (keyboard_highlighted_target_ < default_views) {
    default_view_->children()[keyboard_highlighted_target_]->RequestFocus();
  } else {
    expanded_view_table
        ->children()[keyboard_highlighted_target_ - default_views]
        ->RequestFocus();
  }
  return true;
}

std::unique_ptr<views::NonClientFrameView>
SharesheetBubbleView::CreateNonClientFrameView(views::Widget* widget) {
  // TODO(crbug.com/40136695) Replace this with layer->SetRoundedCornerRadius.
  auto bubble_border =
      std::make_unique<views::BubbleBorder>(arrow(), GetShadow());
  bubble_border->SetColor(color());
  bubble_border->SetCornerRadius(kCornerRadius);
  auto frame =
      views::BubbleDialogDelegateView::CreateNonClientFrameView(widget);
  static_cast<views::BubbleFrameView*>(frame.get())
      ->SetBubbleBorder(std::move(bubble_border));
  return frame;
}

gfx::Size SharesheetBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(width_, height_);
}

void SharesheetBubbleView::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  // Catch widgets that are closing due to the user clicking out of the bubble.
  // If |close_on_deactivate_| we should close the bubble here.
  if (!active && close_on_deactivate_ && !is_bubble_closing_) {
    if (delivered_callback_) {
      std::move(delivered_callback_)
          .Run(::sharesheet::SharesheetResult::kCancel);
    }
    auto user_action = ::sharesheet::SharesheetMetrics::UserAction::
        kCancelledThroughClickingOut;
    auto closed_reason = views::Widget::ClosedReason::kLostFocus;
    if (escape_pressed_) {
      user_action = ::sharesheet::SharesheetMetrics::UserAction::
          kCancelledThroughEscPress;
      closed_reason = views::Widget::ClosedReason::kEscKeyPressed;
    }
    ::sharesheet::SharesheetMetrics::RecordSharesheetActionMetrics(user_action);
    CloseWidgetWithAnimateFadeOut(closed_reason);
  }
}

void SharesheetBubbleView::OnDisplayTabletStateChanged(
    display::TabletState state) {
  if (display::IsTabletStateChanging(state)) {
    // Do nothing if the tablet state still in the process of transition.
    return;
  }

  UpdateAnchorPosition();
}

void SharesheetBubbleView::InitBubble() {
  // This disables the default deactivation behaviour in
  // BubbleDialogDelegateView. Close on deactivation behaviour is managed by the
  // SharesheetBubbleView with the |close_on_deactivate_| member.
  set_close_on_deactivate(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  // Margins must be set to 0 or share_action_view will have undesired margins.
  set_margins(gfx::Insets());

  auto main_view = std::make_unique<views::View>();
  main_view_ = AddChildView(std::move(main_view));

  auto share_action_view = std::make_unique<views::View>();
  share_action_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  share_action_view_ = AddChildView(std::move(share_action_view));
  share_action_view_->SetID(SHARE_ACTION_VIEW_ID);
  share_action_view_->SetVisible(false);
}

void SharesheetBubbleView::SetUpAndShowBubble() {
  main_view_->SetFocusBehavior(View::FocusBehavior::NEVER);
  views::BubbleDialogDelegateView::CreateBubble(base::WrapUnique(this));
  GetWidget()->GetRootView()->DeprecatedLayoutImmediately();
  RecordMimeTypeMetric(intent_);
  ShowWidgetWithAnimateFadeIn();

  UpdateAnchorPosition();
  display::Screen::GetScreen()->AddObserver(this);
}

void SharesheetBubbleView::ExpandButtonPressed() {
  show_expanded_view_ = !show_expanded_view_;

  // Scrollview has separators that overlaps with |header_body_separator_| and
  // |body_footer_separator_| to create a double line when both are visible, so
  // when scrollview is expanded we hide our separators.
  if (header_body_separator_)
    header_body_separator_->SetVisible(!show_expanded_view_);
  body_footer_separator_->SetVisible(!show_expanded_view_);

  expanded_view_->SetVisible(show_expanded_view_);
  expanded_view_separator_->SetVisible(show_expanded_view_);

  if (show_expanded_view_) {
    body_view_->SetPreferredSize(
        gfx::Size(body_view_->width(), kTargetViewExpandedHeight));
    expand_button_->SetToExpandedState();
    AnimateToExpandedState();
  } else {
    body_view_->SetPreferredSize(gfx::Size(
        body_view_->width(), default_view_->GetPreferredSize().height()));
    expand_button_->SetToDefaultState();
  }
  SizeToPreferredSize();
  ResizeBubble(kDefaultBubbleWidth, main_view_->GetPreferredSize().height());
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
  if (!intent_) {
    return;
  }
  auto type = target.type;
  if (type == ::sharesheet::TargetType::kAction) {
    active_share_action_type_ = target.share_action_type.value();
  } else {
    intent_->activity_name = target.activity_name;
  }
  delegator_->OnTargetSelected(
      /*type=*/type, /*share_action_type=*/target.share_action_type,
      /*app_name=*/target.launch_name, /*intent=*/std::move(intent_),
      /*share_action_view=*/share_action_view_);
  if (delivered_callback_) {
    std::move(delivered_callback_)
        .Run(::sharesheet::SharesheetResult::kSuccess);
  }
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
  height_ = main_view_->GetPreferredSize().height();
  PreferredSizeChanged();
}

void SharesheetBubbleView::ShowWidgetWithAnimateFadeIn() {
  constexpr float kSharesheetScaleUpFactor = 0.8f;
  constexpr auto kSharesheetScaleUpTime = base::Milliseconds(150);

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
  if (is_bubble_closing_) {
    return;
  }

  // Don't attempt to react to tablet mode changes while the sharesheet is
  // closing.
  display::Screen::GetScreen()->RemoveObserver(this);
  is_bubble_closing_ = true;
  ui::Layer* layer = View::GetWidget()->GetLayer();

  constexpr auto kSharesheetOpacityFadeOutTime = base::Milliseconds(80);
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

  // Run |close_callback_| after the widget closes.
  if (close_callback_) {
    std::move(close_callback_).Run(closed_reason);
  }
  // Bubble is deleted here.
  delegator_->OnBubbleClosed(active_share_action_type_);
}

BEGIN_METADATA(SharesheetBubbleView)
END_METADATA

}  // namespace sharesheet
}  // namespace ash
