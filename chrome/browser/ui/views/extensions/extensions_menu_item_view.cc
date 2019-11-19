// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
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
constexpr int EXTENSION_CONTEXT_MENU = 13;
constexpr int EXTENSION_PINNING = 14;
}  // namespace

ExtensionsMenuItemView::ExtensionsMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller)
    : primary_action_button_(
          new ExtensionsMenuButton(browser, this, controller.get())),
      controller_(std::move(controller)),
      model_(ToolbarActionsModel::Get(browser->profile())) {
  // Set so the extension button receives enter/exit on children to retain hover
  // status when hovering child views.
  set_notify_enter_exit_on_child(true);

  context_menu_controller_ = std::make_unique<ExtensionContextMenuController>(
      nullptr, controller_.get());

  views::FlexLayout* layout_manager_ =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetIgnoreDefaultMainAxisMargins(true);

  AddChildView(primary_action_button_);
  primary_action_button_->SetProperty(
      views::kFlexBehaviorKey, views::FlexSpecification::ForSizeRule(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));

  const SkColor icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->GetSystemColor(
          ui::NativeTheme::kColorId_DefaultIconColor);

  auto pin_button = CreateBubbleMenuItem(EXTENSION_PINNING, this);
  pin_button->set_ink_drop_base_color(icon_color);

  pin_button_ = pin_button.get();
  AddChildView(std::move(pin_button));

  auto context_menu_button =
      CreateBubbleMenuItem(EXTENSION_CONTEXT_MENU, nullptr);
  views::SetImageFromVectorIcon(context_menu_button.get(), kBrowserToolsIcon,
                                kSecondaryIconSizeDp, icon_color);
  context_menu_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP));
  context_menu_button->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          context_menu_button.get(), this,
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              context_menu_button.get())));

  context_menu_button_ = context_menu_button.get();
  AddChildView(std::move(context_menu_button));
  UpdatePinButton();
}

ExtensionsMenuItemView::~ExtensionsMenuItemView() = default;

void ExtensionsMenuItemView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender->GetID() == EXTENSION_PINNING) {
    model_->SetActionVisibility(controller_->GetId(), !IsPinned());
    return;
  } else if (sender->GetID() == EXTENSION_CONTEXT_MENU) {
    // TODO(crbug.com/998298): Cleanup the menu source type.
    context_menu_controller_->ShowContextMenuForViewImpl(
        sender, sender->GetMenuPosition(),
        ui::MenuSourceType::MENU_SOURCE_MOUSE);
    return;
  }
  NOTREACHED();
}

void ExtensionsMenuItemView::UpdatePinButton() {
  pin_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IsPinned() ? IDS_EXTENSIONS_MENU_UNPIN_BUTTON_TOOLTIP
                 : IDS_EXTENSIONS_MENU_PIN_BUTTON_TOOLTIP));
  SkColor unpinned_icon_color =
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
          ? gfx::kGoogleGrey500
          : gfx::kChromeIconGrey;
  SkColor icon_color = IsPinned()
                           ? GetNativeTheme()->GetSystemColor(
                                 ui::NativeTheme::kColorId_ProminentButtonColor)
                           : unpinned_icon_color;
  views::SetImageFromVectorIcon(
      pin_button_, IsPinned() ? views::kUnpinIcon : views::kPinIcon,
      kSecondaryIconSizeDp, icon_color);
}

bool ExtensionsMenuItemView::IsContextMenuRunning() {
  return context_menu_controller_->IsMenuRunning();
}

bool ExtensionsMenuItemView::IsPinned() {
  // |model_| can be null in unit tests.
  if (!model_)
    return false;
  return model_->IsActionPinned(controller_->GetId());
}

ExtensionsMenuButton*
ExtensionsMenuItemView::primary_action_button_for_testing() {
  return primary_action_button_;
}
