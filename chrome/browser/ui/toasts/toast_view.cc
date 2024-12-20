// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_view.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace {
constexpr int kAnimationEntryDuration = 300;
constexpr int kAnimationExitDuration = 150;
constexpr int kAnimationHeightOffset = 50;
constexpr float kAnimationHeightScale = 0.5;

gfx::Transform GetScaleTransformation(gfx::Rect bounds) {
  gfx::Transform transform;
  transform.Translate(0,
                      bounds.CenterPoint().y() * (1 - kAnimationHeightScale));
  transform.Scale(1, kAnimationHeightScale);
  return transform;
}

bool IsCompatibleImageSize(const ui::ImageModel* image) {
  const auto intended_size = toasts::ToastView::GetIconSize();
  const auto image_size = image->Size();
  return image_size.width() == intended_size &&
         image_size.height() == intended_size;
}

// A simple `MenuModelAdapter` that runs `on_executed_command` whenever the
// command corresponding to a menu item is executed.
class ToastMenuModelAdapter : public views::MenuModelAdapter {
 public:
  ToastMenuModelAdapter(ui::MenuModel* menu_model,
                        base::RepeatingClosure on_menu_closed,
                        base::RepeatingClosure on_executed_command)
      : views::MenuModelAdapter(menu_model, std::move(on_menu_closed)),
        on_executed_command_(std::move(on_executed_command)) {}

  ~ToastMenuModelAdapter() override = default;

 private:
  // views::MenuModelAdapter:
  void ExecuteCommand(int id) override {
    views::MenuModelAdapter::ExecuteCommand(id);
    on_executed_command_.Run();
  }

  void ExecuteCommand(int id, int mouse_event_flags) override {
    views::MenuModelAdapter::ExecuteCommand(id, mouse_event_flags);
    on_executed_command_.Run();
  }

  base::RepeatingClosure on_executed_command_;
};

}  // namespace

namespace toasts {
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToastView, kToastViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToastView, kToastActionButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToastView, kToastCloseButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToastView, kToastMenuButton);

ToastView::ToastView(
    views::View* anchor_view,
    const std::u16string& toast_text,
    const gfx::VectorIcon& icon,
    const ui::ImageModel* image_override,
    bool render_toast_over_web_contents,
    base::RepeatingCallback<void(ToastCloseReason)> toast_close_callback)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::NONE),
      AnimationDelegateViews(this),
      toast_text_(toast_text),
      icon_(icon),
      image_override_(image_override),
      render_toast_over_web_contents_(render_toast_over_web_contents),
      toast_close_callback_(std::move(toast_close_callback)) {
  SetShowCloseButton(false);
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_corner_radius(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT));
  SetProperty(views::kElementIdentifierKey, kToastElementId);
  set_close_on_deactivate(false);
  SetProperty(views::kElementIdentifierKey, kToastViewId);
  SetAccessibleWindowRole(ax::mojom::Role::kAlert);
  SetAccessibleTitle(toast_text_);
}

ToastView::~ToastView() = default;

void ToastView::AddActionButton(const std::u16string& action_button_text,
                                base::RepeatingClosure action_button_callback) {
  CHECK(!has_action_button_);
  has_action_button_ = true;
  action_button_text_ = action_button_text;
  action_button_callback_ = std::move(action_button_callback);
}

void ToastView::AddCloseButton(base::RepeatingClosure close_callback) {
  CHECK(!has_close_button_);
  has_close_button_ = true;
  close_button_callback_ = std::move(close_callback);
}

void ToastView::AddMenu(std::unique_ptr<ui::MenuModel> model) {
  CHECK(!menu_model_);
  menu_model_ = std::move(model);
  menu_model_adapter_ = std::make_unique<ToastMenuModelAdapter>(
      menu_model_.get(),
      /*on_menu_closed=*/
      base::BindRepeating(&ToastView::OnMenuClosed, base::Unretained(this)),
      /*on_executed_command=*/
      base::BindRepeating(&ToastView::Close, base::Unretained(this),
                          ToastCloseReason::kMenuItemClick));
}

int ToastView::GetIconSize() {
  const ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
  return lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE);
}

void ToastView::Init() {
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets()))
      ->set_between_child_spacing(
          lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING));

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, lp->GetDistanceMetric(
                             DISTANCE_TOAST_BUBBLE_LEADING_ICON_SIDE_MARGINS)));

  label_ = AddChildView(
      std::make_unique<views::Label>(toast_text_, CONTEXT_TOAST_BODY_TEXT));
  label_->SetEnabledColorId(ui::kColorToastForeground);
  label_->SetMultiLine(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetAllowCharacterBreak(false);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetLineHeight(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT));
  int max_child_height =
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT);

  if (has_action_button_) {
    label_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(
            0, 0, 0,
            lp->GetDistanceMetric(
                DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING) -
                lp->GetDistanceMetric(
                    DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING)));

    action_button_ = AddChildView(std::make_unique<views::MdTextButton>(
        action_button_callback_.Then(
            base::BindRepeating(&ToastView::Close, base::Unretained(this),
                                ToastCloseReason::kActionButton)),
        action_button_text_));
    action_button_->SetEnabledTextColorIds(ui::kColorToastButton);
    action_button_->SetBgColorIdOverride(ui::kColorToastBackgroundProminent);
    action_button_->SetStrokeColorIdOverride(ui::kColorToastButton);
    action_button_->SetPreferredSize(gfx::Size(
        action_button_->GetPreferredSize().width(),
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON)));
    action_button_->SetStyle(ui::ButtonStyle::kProminent);
    action_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
    action_button_->SetProperty(views::kElementIdentifierKey,
                                kToastActionButton);
    SetInitiallyFocusedView(action_button_);
    max_child_height = std::max(
        max_child_height,
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON));
  }

  if (has_close_button_) {
    close_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
        close_button_callback_.Then(
            base::BindRepeating(&ToastView::Close, base::Unretained(this),
                                ToastCloseReason::kCloseButton)),
        vector_icons::kCloseChromeRefreshIcon,
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE),
        ui::kColorToastForeground));
    // Override the image button's border with the appropriate icon border size.
    const gfx::Insets insets =
        lp->GetInsetsMetric(views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON);
    close_button_->SetBorder(views::CreateEmptyBorder(insets));
    views::InstallCircleHighlightPathGenerator(close_button_);
    close_button_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_CLOSE));
    close_button_->SetProperty(views::kElementIdentifierKey, kToastCloseButton);
    close_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_TOAST_CLOSE_TOOLTIP));
    if (!HasConfiguredInitiallyFocusedView()) {
      SetInitiallyFocusedView(close_button_);
    }
    max_child_height =
        std::max(max_child_height,
                 lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE) +
                     insets.height());
  }

  if (menu_model_) {
    menu_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(&ToastView::OnMenuButtonClicked,
                            base::Unretained(this)),
        kBrowserToolsChromeRefreshIcon,
        /*dip_size=*/
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MENU_ICON_SIZE),
        ui::kColorToastForeground));
    views::InstallCircleHighlightPathGenerator(menu_button_);
    menu_button_->SetProperty(views::kElementIdentifierKey, kToastMenuButton);
    menu_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_TOAST_MENU_BUTTON_NAME));
    const gfx::Insets insets = menu_button_->GetInsets();
    const int left_margin =
        lp->GetDistanceMetric(
            DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_MENU_BUTTON_SPACING) -
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING) -
        insets.left();
    menu_button_->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, left_margin, 0, 0));
    max_child_height =
        std::max(max_child_height,
                 lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MENU_ICON_SIZE) +
                     insets.height());
  }

  // Height of the toast is set implicitly by adding margins depending on the
  // height of the tallest child. We cannot simply iterate over all children
  // here because the `ImageModel` for `ImageButton`s is only set once the theme
  // is available.
  const int total_vertical_margins =
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT) - max_child_height;
  const int top_margin = total_vertical_margins / 2;
  const int right_margin = [&]() {
    if (menu_button_) {
      return lp->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON);
    } else if (close_button_) {
      return lp->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON);
    } else if (action_button_) {
      return lp->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_ACTION_BUTTON);
    }
    return lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_LABEL);
  }();
  set_margins(gfx::Insets::TLBR(
      top_margin, lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_LEFT),
      total_vertical_margins - top_margin, right_margin));

  if (has_action_button_ || has_close_button_) {
    SetFocusTraversesOut(true);
  } else {
    set_focus_traversable_from_anchor_view(false);
    SetCanActivate(false);
  }
}

void ToastView::AnimationProgressed(const gfx::Animation* animation) {
  const double value = gfx::Tween::CalculateValue(
      height_animation_tween_, height_animation_.GetCurrentValue());
  const gfx::Rect current_bounds = gfx::Tween::RectValueBetween(
      value, starting_widget_bounds_, target_widget_bounds_);
  GetWidget()->SetBounds(current_bounds);
}

void ToastView::AnimateIn() {
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    return;
  }

  target_widget_bounds_ = GetWidget()->GetWindowBoundsInScreen();
  starting_widget_bounds_ =
      target_widget_bounds_ - gfx::Vector2d{0, kAnimationHeightOffset};
  height_animation_tween_ = gfx::Tween::ACCEL_5_70_DECEL_90;
  height_animation_.SetDuration(base::Milliseconds(kAnimationEntryDuration));
  height_animation_.Start();

  views::View* const bubble_frame_view = GetBubbleFrameView();
  bubble_frame_view->SetPaintToLayer();
  bubble_frame_view->layer()->SetFillsBoundsOpaquely(false);
  bubble_frame_view->SetTransform(
      GetScaleTransformation(bubble_frame_view->bounds()));
  bubble_frame_view->layer()->SetOpacity(0);
  GetDialogClientView()->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorToastBackgroundProminent));
  GetDialogClientView()->SetPaintToLayer();
  GetDialogClientView()->layer()->SetOpacity(0);
  views::AnimationBuilder()
      .Once()
      .SetDuration(base::Milliseconds(kAnimationEntryDuration))
      .SetTransform(bubble_frame_view, gfx::Transform(),
                    height_animation_tween_)
      .At(base::TimeDelta())
      .SetDuration(base::Milliseconds(50))
      .SetOpacity(bubble_frame_view, 1)
      .Then()
      .SetDuration(base::Milliseconds(150))
      .SetOpacity(GetDialogClientView(), 1);
}

void ToastView::Close(ToastCloseReason reason) {
  // Do not close if the menu is open - instead, remember the call to close it.
  if (menu_runner_) {
    pending_close_reason_ = reason;
    return;
  }

  // TODO(crbug.com/358610872): Log toast close reason metric.
  views::Widget::ClosedReason widget_closed_reason =
      views::Widget::ClosedReason::kUnspecified;
  switch (reason) {
    case ToastCloseReason::kCloseButton:
      widget_closed_reason = views::Widget::ClosedReason::kCloseButtonClicked;
      break;
    case ToastCloseReason::kActionButton:
      widget_closed_reason = views::Widget::ClosedReason::kAcceptButtonClicked;
      break;
    default:
      break;
  }

  toast_close_callback_.Run(reason);
  if (GetWidget()->IsVisible()) {
    AnimateOut(
        base::BindOnce(&views::Widget::CloseWithReason,
                       base::Unretained(GetWidget()), widget_closed_reason),
        reason != ToastCloseReason::kPreempted);
  } else {
    GetWidget()->CloseWithReason(widget_closed_reason);
  }
}

void ToastView::UpdateRenderToastOverWebContentsAndPaint(
    const bool render_toast_over_web_contents) {
  render_toast_over_web_contents_ = render_toast_over_web_contents;
  SizeToContents();
}

gfx::Rect ToastView::GetBubbleBounds() {
  views::View* anchor_view = GetAnchorView();
  if (!anchor_view) {
    return gfx::Rect();
  }

  const gfx::Size bubble_size =
      GetWidget()->GetContentsView()->GetPreferredSize();
  const gfx::Rect anchor_bounds = anchor_view->GetBoundsInScreen();
  const int x =
      anchor_bounds.x() + (anchor_bounds.width() - bubble_size.width()) / 2;
  // Take bubble out of its original bounds to cross "line of death", unless in
  // fullscreen mode where the top container isn't rendered.
  const int y = anchor_bounds.bottom() - (render_toast_over_web_contents_
                                              ? views::BubbleBorder::kShadowBlur
                                              : (bubble_size.height() / 2));
  return gfx::Rect(x, y, bubble_size.width(), bubble_size.height());
}

void ToastView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  set_color(color_provider->GetColor(ui::kColorToastBackgroundProminent));
  if (image_override_ != nullptr && IsCompatibleImageSize(image_override_)) {
    icon_view_->SetImage(*image_override_);
  } else {
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *icon_, color_provider->GetColor(ui::kColorToastForeground),
        GetIconSize()));
  }
}

void ToastView::AnimateOut(base::OnceClosure callback,
                           bool show_height_animation) {
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    std::move(callback).Run();
    return;
  }

  views::View* const bubble_frame_view = GetBubbleFrameView();
  if (!bubble_frame_view->layer()) {
    bubble_frame_view->SetPaintToLayer();
    bubble_frame_view->layer()->SetFillsBoundsOpaquely(false);
  }

  if (show_height_animation) {
    starting_widget_bounds_ = GetWidget()->GetWindowBoundsInScreen();
    target_widget_bounds_ =
        starting_widget_bounds_ - gfx::Vector2d{0, kAnimationHeightOffset};
    height_animation_tween_ = gfx::Tween::ACCEL_30_DECEL_20_85;
    height_animation_.SetDuration(base::Milliseconds(kAnimationExitDuration));
    height_animation_.Start();

    views::AnimationBuilder()
        .Once()
        .SetDuration(base::Milliseconds(kAnimationExitDuration))
        .SetTransform(bubble_frame_view,
                      GetScaleTransformation(bubble_frame_view->bounds()),
                      height_animation_tween_);
  }

  views::AnimationBuilder()
      .OnEnded(std::move(callback))
      .Once()
      .SetDuration(base::Milliseconds(100))
      .SetOpacity(GetDialogClientView(), 0)
      .Then()
      .SetDuration(base::Milliseconds(50))
      .SetOpacity(bubble_frame_view, 0);
}

void ToastView::OnMenuButtonClicked() {
  if (menu_runner_) {
    menu_runner_->Cancel();
    return;
  }
  menu_button_highlight_ = menu_button_->AddAnchorHighlight();
  menu_button_->SetState(views::Button::ButtonState::STATE_PRESSED);
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_adapter_->CreateMenu(), views::MenuRunner::FIXED_ANCHOR);
  menu_runner_->RunMenuAt(GetWidget(), /*button_controller=*/nullptr,
                          menu_button_->GetBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kNone);
}

void ToastView::OnMenuClosed() {
  menu_runner_.reset();
  menu_button_highlight_.reset();
  menu_button_->SetState(views::Button::ButtonState::STATE_NORMAL);
  if (pending_close_reason_) {
    Close(pending_close_reason_.value());
  }
}

BEGIN_METADATA(ToastView)
END_METADATA

}  // namespace toasts
