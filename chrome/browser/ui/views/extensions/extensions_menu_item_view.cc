// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kSecondaryIconSizeDp = 16;
// Set secondary item insets to get to square buttons.
constexpr gfx::Insets kSecondaryButtonInsets = gfx::Insets(
    (ExtensionsMenuItemView::kMenuItemHeightDp - kSecondaryIconSizeDp) / 2);
constexpr int EXTENSION_CONTEXT_MENU = 13;
constexpr int EXTENSION_PINNING = 14;
}  // namespace

// static
constexpr gfx::Size ExtensionsMenuItemView::kIconSize;
constexpr char ExtensionsMenuItemView::kClassName[];

ExtensionsMenuItemView::ExtensionsMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller,
    bool allow_pinning)
    : primary_action_button_(new ExtensionsMenuButton(browser,
                                                      this,
                                                      controller.get(),
                                                      allow_pinning)),
      controller_(std::move(controller)),
      model_(ToolbarActionsModel::Get(browser->profile())) {
  // Set so the extension button receives enter/exit on children to retain hover
  // status when hovering child views.
  SetNotifyEnterExitOnChild(true);

  context_menu_controller_ = std::make_unique<ExtensionContextMenuController>(
      nullptr, controller_.get());

  views::FlexLayout* layout_manager_ =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true);

  AddChildView(primary_action_button_);
  primary_action_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  if (primary_action_button_->CanShowIconInToolbar()) {
    auto pin_button = CreateBubbleMenuItem(
        EXTENSION_PINNING,
        base::BindRepeating(&ExtensionsMenuItemView::PinButtonPressed,
                            base::Unretained(this)));
    pin_button->SetBorder(views::CreateEmptyBorder(kSecondaryButtonInsets));
    // Extension pinning is not available in Incognito as it leaves a trace of
    // user activity.
    pin_button->SetEnabled(!browser->profile()->IsOffTheRecord());

    pin_button_ = pin_button.get();
    AddChildView(std::move(pin_button));
  }

  auto context_menu_button = CreateBubbleMenuItem(
      EXTENSION_CONTEXT_MENU, views::Button::PressedCallback());
  context_menu_button->SetBorder(
      views::CreateEmptyBorder(kSecondaryButtonInsets));
  context_menu_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP));
  context_menu_button->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          context_menu_button.get(),
          base::BindRepeating(&ExtensionsMenuItemView::ContextMenuPressed,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              context_menu_button.get())));
  context_menu_button_ = AddChildView(std::move(context_menu_button));
}

ExtensionsMenuItemView::~ExtensionsMenuItemView() = default;

const char* ExtensionsMenuItemView::GetClassName() const {
  return kClassName;
}

void ExtensionsMenuItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const SkColor icon_color =
      GetAdjustedIconColor(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_MenuIconColor));

  if (pin_button_)
    pin_button_->SetInkDropBaseColor(icon_color);
  views::SetImageFromVectorIconWithColor(context_menu_button_,
                                         kBrowserToolsIcon,
                                         kSecondaryIconSizeDp, icon_color);
  UpdatePinButton();
}

void ExtensionsMenuItemView::UpdatePinButton() {
  if (!pin_button_)
    return;
  pin_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IsPinned() ? IDS_EXTENSIONS_MENU_UNPIN_BUTTON_TOOLTIP
                 : IDS_EXTENSIONS_MENU_PIN_BUTTON_TOOLTIP));
  SkColor unpinned_icon_color =
      GetAdjustedIconColor(GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_MenuIconColor));
  SkColor icon_color =
      IsPinned() ? GetAdjustedIconColor(GetNativeTheme()->GetSystemColor(
                       ui::NativeTheme::kColorId_ProminentButtonColor))
                 : unpinned_icon_color;
  views::SetImageFromVectorIconWithColor(
      pin_button_, IsPinned() ? views::kUnpinIcon : views::kPinIcon,
      kSecondaryIconSizeDp, icon_color);
}

bool ExtensionsMenuItemView::IsContextMenuRunning() const {
  return context_menu_controller_->IsMenuRunning();
}

bool ExtensionsMenuItemView::IsPinned() const {
  // |model_| can be null in unit tests.
  return model_ && model_->IsActionPinned(controller_->GetId());
}

void ExtensionsMenuItemView::ContextMenuPressed() {
  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu"));
  // TODO(crbug.com/998298): Cleanup the menu source type.
  context_menu_controller_->ShowContextMenuForViewImpl(
      context_menu_button_, context_menu_button_->GetMenuPosition(),
      ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void ExtensionsMenuItemView::PinButtonPressed() {
  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.PinButtonPressed"));
  model_->SetActionVisibility(controller_->GetId(), !IsPinned());
}

ExtensionsMenuButton*
ExtensionsMenuItemView::primary_action_button_for_testing() {
  return primary_action_button_;
}

SkColor ExtensionsMenuItemView::GetAdjustedIconColor(SkColor icon_color) const {
  const SkColor background_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_BubbleBackground);
  if (background_color != SK_ColorTRANSPARENT) {
    return color_utils::BlendForMinContrast(icon_color, background_color).color;
  }
  return icon_color;
}
