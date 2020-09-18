// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"

#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/browser/ui/in_product_help/in_product_help.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_colors.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/metrics.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// Cycle duration of ink drop pulsing animation used for in-product help.
constexpr base::TimeDelta kFeaturePromoPulseDuration =
    base::TimeDelta::FromMilliseconds(800);

// Max inset for pulsing animation.
constexpr float kFeaturePromoPulseInsetDip = 3.0f;

// An InkDropMask used to animate the size of the BrowserAppMenuButton's ink
// drop. This is used when showing in-product help.
class PulsingInkDropMask : public views::AnimationDelegateViews,
                           public views::InkDropMask {
 public:
  PulsingInkDropMask(views::View* layer_container,
                     const gfx::Size& layer_size,
                     const gfx::Insets& margins,
                     float normal_corner_radius,
                     float max_inset)
      : AnimationDelegateViews(layer_container),
        views::InkDropMask(layer_size),
        layer_container_(layer_container),
        margins_(margins),
        normal_corner_radius_(normal_corner_radius),
        max_inset_(max_inset),
        throb_animation_(this) {
    throb_animation_.SetThrobDuration(kFeaturePromoPulseDuration);
    throb_animation_.StartThrobbing(-1);
  }

 private:
  // views::InkDropMask:
  void OnPaintLayer(const ui::PaintContext& context) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);

    ui::PaintRecorder recorder(context, layer()->size());

    gfx::RectF bounds(layer()->bounds());
    bounds.Inset(margins_);

    const float current_inset =
        throb_animation_.CurrentValueBetween(0.0f, max_inset_);
    bounds.Inset(gfx::InsetsF(current_inset));
    const float corner_radius = normal_corner_radius_ - current_inset;

    recorder.canvas()->DrawRoundRect(bounds, corner_radius, flags);
  }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK_EQ(animation, &throb_animation_);
    layer()->SchedulePaint(gfx::Rect(layer()->size()));

    // This is a workaround for crbug.com/935808: for scale factors >1,
    // invalidating the mask layer doesn't cause the whole layer to be repainted
    // on screen. TODO(crbug.com/935808): remove this workaround once the bug is
    // fixed.
    layer_container_->SchedulePaint();
  }

  // The View that contains the InkDrop layer we're masking. This must outlive
  // our instance.
  views::View* const layer_container_;

  // Margins between the layer bounds and the visible ink drop. We use this
  // because sometimes the View we're masking is larger than the ink drop we
  // want to show.
  const gfx::Insets margins_;

  // Normal corner radius of the ink drop without animation. This is also the
  // corner radius at the largest instant of the animation.
  const float normal_corner_radius_;

  // Max inset, used at the smallest instant of the animation.
  const float max_inset_;

  gfx::ThrobAnimation throb_animation_;
};

}  // namespace

// static
bool BrowserAppMenuButton::g_open_app_immediately_for_testing = false;

BrowserAppMenuButton::BrowserAppMenuButton(ToolbarView* toolbar_view)
    : AppMenuButton(toolbar_view), toolbar_view_(toolbar_view) {
  SetInkDropMode(InkDropMode::ON);
  SetHorizontalAlignment(gfx::ALIGN_RIGHT);

  SetInkDropVisibleOpacity(kToolbarInkDropVisibleOpacity);
}

BrowserAppMenuButton::~BrowserAppMenuButton() {}

void BrowserAppMenuButton::SetTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  type_and_severity_ = type_and_severity;

  UpdateIcon();
  UpdateTextAndHighlightColor();
}

void BrowserAppMenuButton::SetPromoFeature(
    base::Optional<InProductHelpFeature> promo_feature) {
  if (promo_feature_ == promo_feature)
    return;

  promo_feature_ = promo_feature;

  // We override GetInkDropBaseColor() and CreateInkDropMask(), returning the
  // promo values if we are showing an in-product help promo. Calling
  // HostSizeChanged() will force the new mask and color to be fetched.
  //
  // TODO(collinbaker): Consider adding explicit way to recreate mask instead of
  // relying on HostSizeChanged() to do so.
  GetInkDrop()->HostSizeChanged(size());

  views::InkDropState next_state;
  if (promo_feature_ || IsMenuShowing()) {
    // If we are showing a promo, we must use the ACTIVATED state to show the
    // highlight. Otherwise, if the menu is currently showing, we need to keep
    // the ink drop in the ACTIVATED state.
    next_state = views::InkDropState::ACTIVATED;
  } else {
    // If we are not showing a promo and the menu is hidden, we use the
    // DEACTIVATED state.
    next_state = views::InkDropState::DEACTIVATED;
    // TODO(collinbaker): this is brittle since we don't know if something else
    // should keep this ACTIVATED or in some other state. Consider adding code
    // to track the correct state and restore to that.
  }
  GetInkDrop()->AnimateToState(next_state);

  UpdateIcon();
  SchedulePaint();
}

void BrowserAppMenuButton::ShowMenu(int run_types) {
  if (IsMenuShowing())
    return;

#if defined(OS_CHROMEOS)
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (keyboard_client->is_keyboard_visible())
    keyboard_client->HideKeyboard(ash::HideReason::kSystem);
#endif

  Browser* browser = toolbar_view_->browser();

  bool alert_reopen_tab_items =
      promo_feature_ == InProductHelpFeature::kReopenTab;

  RunMenu(
      std::make_unique<AppMenuModel>(toolbar_view_, browser,
                                     toolbar_view_->app_menu_icon_controller()),
      browser, run_types, alert_reopen_tab_items);
}

void BrowserAppMenuButton::OnThemeChanged() {
  UpdateTextAndHighlightColor();
  AppMenuButton::OnThemeChanged();
}

void BrowserAppMenuButton::UpdateIcon() {
  bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  if (base::FeatureList::IsEnabled(features::kUseTextForUpdateButton)) {
    const gfx::VectorIcon& icon =
        touch_ui ? kBrowserToolsTouchIcon : kBrowserToolsIcon;
    for (auto state : kButtonStates) {
      SkColor icon_color =
          toolbar_view_->app_menu_icon_controller()->GetIconColor(
              GetForegroundColor(state));
      SetImageModel(state, ui::ImageModel::FromVectorIcon(icon, icon_color));
    }
    return;
  }
  for (auto state : kButtonStates) {
    SetImageModel(state,
                  toolbar_view_->app_menu_icon_controller()->GetIconImage(
                      touch_ui, GetForegroundColor(state)));
  }
}

void BrowserAppMenuButton::UpdateTextAndHighlightColor() {
  int tooltip_message_id;
  base::string16 text;
  if (type_and_severity_.severity == AppMenuIconController::Severity::NONE) {
    tooltip_message_id = IDS_APPMENU_TOOLTIP;
  } else if (type_and_severity_.type ==
             AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    tooltip_message_id = IDS_APPMENU_TOOLTIP_UPDATE_AVAILABLE;
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_UPDATE);
  } else {
    tooltip_message_id = IDS_APPMENU_TOOLTIP_ALERT;
    text = l10n_util::GetStringUTF16(IDS_APP_MENU_BUTTON_ERROR);
  }

  base::Optional<SkColor> color;
  switch (type_and_severity_.severity) {
    case AppMenuIconController::Severity::NONE:
      break;
    case AppMenuIconController::Severity::LOW:
      color = AdjustHighlightColorForContrast(
          GetThemeProvider(), gfx::kGoogleGreen300, gfx::kGoogleGreen600,
          gfx::kGoogleGreen050, gfx::kGoogleGreen900);

      break;
    case AppMenuIconController::Severity::MEDIUM:
      color = AdjustHighlightColorForContrast(
          GetThemeProvider(), gfx::kGoogleYellow300, gfx::kGoogleYellow600,
          gfx::kGoogleYellow050, gfx::kGoogleYellow900);

      break;
    case AppMenuIconController::Severity::HIGH:
      color = AdjustHighlightColorForContrast(
          GetThemeProvider(), gfx::kGoogleRed300, gfx::kGoogleRed600,
          gfx::kGoogleRed050, gfx::kGoogleRed900);

      break;
  }

  SetTooltipText(l10n_util::GetStringUTF16(tooltip_message_id));

  if (base::FeatureList::IsEnabled(features::kUseTextForUpdateButton))
    SetHighlight(text, color);
}

const char* BrowserAppMenuButton::GetClassName() const {
  return "BrowserAppMenuButton";
}

SkColor BrowserAppMenuButton::GetForegroundColor(ButtonState state) const {
  return promo_feature_
             ? GetFeaturePromoHighlightColorForToolbar(GetThemeProvider())
             : ToolbarButton::GetForegroundColor(state);
}

bool BrowserAppMenuButton::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool BrowserAppMenuButton::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool BrowserAppMenuButton::CanDrop(const ui::OSExchangeData& data) {
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu))
    return false;
  return BrowserActionDragData::CanDrop(data,
                                        toolbar_view_->browser()->profile());
}

void BrowserAppMenuButton::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(!weak_factory_.HasWeakPtrs());
  int run_types = views::MenuRunner::FOR_DROP;
  if (event.IsKeyEvent())
    run_types |= views::MenuRunner::SHOULD_SHOW_MNEMONICS;

  if (!g_open_app_immediately_for_testing) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BrowserAppMenuButton::ShowMenu,
                       weak_factory_.GetWeakPtr(), run_types),
        base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
  } else {
    ShowMenu(run_types);
  }
}

int BrowserAppMenuButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserAppMenuButton::OnDragExited() {
  weak_factory_.InvalidateWeakPtrs();
}

int BrowserAppMenuButton::OnPerformDrop(const ui::DropTargetEvent& event) {
  return ui::DragDropTypes::DRAG_MOVE;
}

std::unique_ptr<views::InkDropHighlight>
BrowserAppMenuButton::CreateInkDropHighlight() const {
  return CreateToolbarInkDropHighlight(this);
}

std::unique_ptr<views::InkDropMask> BrowserAppMenuButton::CreateInkDropMask()
    const {
  if (promo_feature_) {
    // This gets the latest ink drop insets. |SetTrailingMargin()| is called
    // whenever our margins change (i.e. due to the window maximizing or
    // minimizing) and updates our internal padding property accordingly.
    const gfx::Insets ink_drop_insets = GetToolbarInkDropInsets(this);
    const float corner_radius =
        (height() - ink_drop_insets.top() - ink_drop_insets.bottom()) / 2.0f;
    return std::make_unique<PulsingInkDropMask>(ink_drop_container(), size(),
                                                ink_drop_insets, corner_radius,
                                                kFeaturePromoPulseInsetDip);
  }

  return AppMenuButton::CreateInkDropMask();
}

SkColor BrowserAppMenuButton::GetInkDropBaseColor() const {
  return promo_feature_
             ? GetFeaturePromoHighlightColorForToolbar(GetThemeProvider())
             : AppMenuButton::GetInkDropBaseColor();
}

base::string16 BrowserAppMenuButton::GetTooltipText(const gfx::Point& p) const {
  // Suppress tooltip when IPH is showing.
  if (promo_feature_)
    return base::string16();

  return AppMenuButton::GetTooltipText(p);
}

void BrowserAppMenuButton::OnTouchUiChanged() {
  UpdateColorsAndInsets();
  PreferredSizeChanged();
}
