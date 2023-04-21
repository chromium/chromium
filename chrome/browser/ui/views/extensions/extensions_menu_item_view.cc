// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int EXTENSION_CONTEXT_MENU = 13;
constexpr int EXTENSION_PINNING = 14;

void SetButtonIconWithColor(HoverButton* button,
                            const gfx::VectorIcon& icon,
                            SkColor icon_color,
                            SkColor disabled_icon_color) {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);
  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(icon, icon_color, icon_size));
  button->SetImageModel(
      views::Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(icon, disabled_icon_color, icon_size));
}

std::u16string GetPinButtonTooltip(bool is_force_pinned, bool is_pinned) {
  int tooltip_id = IDS_EXTENSIONS_PIN_TO_TOOLBAR;
  if (is_force_pinned) {
    tooltip_id = IDS_EXTENSIONS_PINNED_BY_ADMIN;
  } else if (is_pinned) {
    tooltip_id = IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR;
  }
  return l10n_util::GetStringUTF16(tooltip_id);
}

std::u16string GetPinButtonPressedAccText(bool is_pinned) {
  return l10n_util::GetStringUTF16(is_pinned ? IDS_EXTENSION_PINNED
                                             : IDS_EXTENSION_UNPINNED);
}

std::u16string GetSiteAccessToggleTooltip(bool is_on) {
  return l10n_util::GetStringUTF16(
      is_on ? IDS_EXTENSIONS_MENU_EXTENSION_SITE_ACCESS_TOGGLE_ON_TOOLTIP
            : IDS_EXTENSIONS_MENU_EXTENSION_SITE_ACCESS_TOGGLE_OFF_TOOLTIP);
}

}  // namespace
ExtensionMenuItemView::ExtensionMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller,
    bool allow_pinning)
    : browser_(browser),
      controller_(std::move(controller)),
      model_(ToolbarActionsModel::Get(browser_->profile())) {
  CHECK(!base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded);
  auto builder =
      views::Builder<ExtensionMenuItemView>(this)
          // Set so the extension button receives enter/exit on children to
          // retain hover status when hovering child views.
          .SetNotifyEnterExitOnChild(true)
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetIgnoreDefaultMainAxisMargins(true)
          .AddChildren(
              views::Builder<ExtensionsMenuButton>(
                  std::make_unique<ExtensionsMenuButton>(browser_,
                                                         controller_.get()))
                  .CopyAddressTo(&primary_action_button_)
                  .SetProperty(views::kFlexBehaviorKey, stretch_specification),
              views::Builder<HoverButton>(
                  std::make_unique<HoverButton>(
                      views::Button::PressedCallback(), std::u16string()))
                  .CopyAddressTo(&context_menu_button_)
                  .SetID(EXTENSION_CONTEXT_MENU)
                  .SetBorder(views::CreateEmptyBorder(
                      ChromeLayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN)))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP)));

  if (allow_pinning) {
    // Pin button should be in between `primary_action_button_` and
    // `context_menu_button_`.
    int index = 1;
    builder.AddChildAt(
        views::Builder<HoverButton>(
            std::make_unique<HoverButton>(
                base::BindRepeating(&ExtensionMenuItemView::OnPinButtonPressed,
                                    base::Unretained(this)),
                std::u16string()))
            .CopyAddressTo(&pin_button_)
            .SetID(EXTENSION_PINNING)
            .SetBorder(views::CreateEmptyBorder(
                ChromeLayoutProvider::Get()->GetDistanceMetric(
                    DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN))),
        index);
  }

  std::move(builder).BuildChildren();

  SetupContextMenuButton();
}

ExtensionMenuItemView::ExtensionMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewController> controller,
    views::Button::PressedCallback site_access_toggle_callback,
    views::Button::PressedCallback site_permissions_button_callback)
    : browser_(browser),
      controller_(std::move(controller)),
      model_(ToolbarActionsModel::Get(browser_->profile())) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded);
  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  const int icon_size =
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  const int horizontal_inset =
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN);
  const int icon_label_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  views::Builder<ExtensionMenuItemView>(this)
      // Set so the extension button receives enter/exit on children to
      // retain hover status when hovering child views.
      .SetNotifyEnterExitOnChild(true)
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetProperty(views::kFlexBehaviorKey, stretch_specification)
      .AddChildren(
          // Main row.
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kHorizontal)
              .SetIgnoreDefaultMainAxisMargins(true)
              .AddChildren(
                  // Primary action button.
                  views::Builder<ExtensionsMenuButton>(
                      std::make_unique<ExtensionsMenuButton>(browser_,
                                                             controller_.get()))
                      .CopyAddressTo(&primary_action_button_)
                      .SetProperty(views::kFlexBehaviorKey,
                                   stretch_specification),
                  // Site access toggle.
                  views::Builder<views::ToggleButton>()
                      .CopyAddressTo(&site_access_toggle_)
                      .SetCallback(site_access_toggle_callback),
                  // Context menu button.
                  views::Builder<HoverButton>(
                      std::make_unique<HoverButton>(
                          views::Button::PressedCallback(), std::u16string()))
                      .CopyAddressTo(&context_menu_button_)
                      .SetID(EXTENSION_CONTEXT_MENU)
                      .SetBorder(views::CreateEmptyBorder(
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN)))
                      .SetTooltipText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP))),
          // Secondary row.
          views::Builder<views::FlexLayoutView>().AddChildren(
              // Site permissions button.
              // TODO(crbug.com/998298): Compute title based on the
              // extension site access.
              // TODO(crbug.com/998298): Add tooltip after UX provides it.
              views::Builder<HoverButton>(
                  std::make_unique<HoverButton>(
                      site_permissions_button_callback,
                      /*icon_view=*/nullptr, u"site access", std::u16string(),
                      std::make_unique<views::ImageView>(
                          ui::ImageModel::FromVectorIcon(
                              vector_icons::kSubmenuArrowIcon,
                              ui::kColorIcon))))
                  .CopyAddressTo(&site_permissions_button_)
                  // Margin to align the main and secondary row text. Icon
                  // size and horizontal insets should be the values used by
                  // the extensions menu button.
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets::VH(0, icon_size + horizontal_inset))
                  // Border should be the same as the icon label
                  // spacing used by the extensions menu button.
                  .SetBorder(views::CreateEmptyBorder(
                      gfx::Insets::VH(0, icon_label_spacing)))))
      .BuildChildren();

  SetupContextMenuButton();
}

ExtensionMenuItemView::~ExtensionMenuItemView() = default;

void ExtensionMenuItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto* const color_provider = GetColorProvider();
  const SkColor icon_color = color_provider->GetColor(kColorExtensionMenuIcon);

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    bool is_pinned = model_ && model_->IsActionPinned(controller_->GetId());
    UpdateContextMenuButton(is_pinned);
  } else {
    SetButtonIconWithColor(
        context_menu_button_, kBrowserToolsIcon, icon_color,
        color_provider->GetColor(kColorExtensionMenuIconDisabled));
    if (pin_button_) {
      views::InkDrop::Get(pin_button_)->SetBaseColor(icon_color);
      bool is_pinned = model_ && model_->IsActionPinned(controller_->GetId());
      bool is_force_pinned =
          model_ && model_->IsActionForcePinned(controller_->GetId());
      UpdatePinButton(is_force_pinned, is_pinned);
    }
  }
}

void ExtensionMenuItemView::Update(
    SiteAccessToggleState site_access_toggle_state,
    SitePermissionsButtonState site_permissions_button_state) {
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    bool is_toggle_on = site_access_toggle_state == SiteAccessToggleState::kOn;
    std::u16string toggle_tooltip = GetSiteAccessToggleTooltip(is_toggle_on);
    site_access_toggle_->SetVisible(site_access_toggle_state !=
                                    SiteAccessToggleState::kHidden);
    site_access_toggle_->SetIsOn(is_toggle_on);
    site_access_toggle_->SetTooltipText(toggle_tooltip);
    site_access_toggle_->SetAccessibleName(toggle_tooltip);

    site_permissions_button_->SetVisible(site_permissions_button_state !=
                                         SitePermissionsButtonState::kHidden);
    site_permissions_button_->SetEnabled(site_permissions_button_state ==
                                         SitePermissionsButtonState::kEnabled);
    // TODO(crbug.com/1390952): Display the arrow icon only when site
    // permissions button is enabled.
  }

  view_controller()->UpdateState();
}

void ExtensionMenuItemView::UpdatePinButton(bool is_force_pinned,
                                            bool is_pinned) {
  if (!pin_button_ || !GetWidget()) {
    return;
  }

  pin_button_->SetTooltipText(GetPinButtonTooltip(is_force_pinned, is_pinned));
  // Extension pinning is not available in Incognito as it leaves a trace of
  // user activity.
  pin_button_->SetEnabled(!is_force_pinned &&
                          !browser_->profile()->IsOffTheRecord());

  const auto* const color_provider = GetColorProvider();
  const SkColor icon_color = color_provider->GetColor(
      is_pinned ? kColorExtensionMenuPinButtonIcon : kColorExtensionMenuIcon);
  const SkColor disabled_icon_color = color_provider->GetColor(
      is_pinned ? kColorExtensionMenuPinButtonIconDisabled
                : kColorExtensionMenuIconDisabled);
  SetButtonIconWithColor(pin_button_,
                         is_pinned ? views::kUnpinIcon : views::kPinIcon,
                         icon_color, disabled_icon_color);
}

void ExtensionMenuItemView::UpdateContextMenuButton(bool is_action_pinned) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);
  const auto* const color_provider = GetColorProvider();
  auto three_dot_icon = ui::ImageModel::FromVectorIcon(
      kBrowserToolsIcon, color_provider->GetColor(kColorExtensionMenuIcon),
      icon_size);

  // Show a pin button for the context menu normal state icon when the action is
  // pinned in the toolbar. All other states should look, and behave, the same.
  context_menu_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      is_action_pinned
          ? ui::ImageModel::FromVectorIcon(
                views::kPinIcon,
                color_provider->GetColor(kColorExtensionMenuPinButtonIcon),
                icon_size)
          : three_dot_icon);
  context_menu_button_->SetImageModel(views::Button::STATE_HOVERED,
                                      three_dot_icon);
  context_menu_button_->SetImageModel(views::Button::STATE_PRESSED,
                                      three_dot_icon);
}

void ExtensionMenuItemView::SetupContextMenuButton() {
  // Add a controller to the context menu
  context_menu_controller_ = std::make_unique<ExtensionContextMenuController>(
      controller_.get(),
      extensions::ExtensionContextMenuModel::ContextMenuSource::kMenuItem);

  context_menu_button_->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          context_menu_button_.get(),
          base::BindRepeating(&ExtensionMenuItemView::OnContextMenuPressed,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              context_menu_button_.get())));
}

void ExtensionMenuItemView::OnContextMenuPressed() {
  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu"));
  // TODO(crbug.com/998298): Cleanup the menu source type.
  context_menu_controller_->ShowContextMenuForViewImpl(
      context_menu_button_, context_menu_button_->GetMenuPosition(),
      ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void ExtensionMenuItemView::OnPinButtonPressed() {
  CHECK(model_);
  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.PinButtonPressed"));
  // Toggle pin visibility.
  bool is_action_pinned = model_->IsActionPinned(controller_->GetId());
  model_->SetActionVisibility(controller_->GetId(), !is_action_pinned);
  GetViewAccessibility().AnnounceText(
      GetPinButtonPressedAccText(is_action_pinned));
}

bool ExtensionMenuItemView::IsContextMenuRunningForTesting() const {
  return context_menu_controller_->IsMenuRunning();
}

BEGIN_METADATA(ExtensionMenuItemView, views::View)
END_METADATA
