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
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"
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

std::u16string GetContextMenuAccessibleName(bool is_pinned) {
  int tooltip_id =
      is_pinned
          ? IDS_EXTENSIONS_MENU_EXTENSION_CONTEXT_MENU_BUTTON_PINNED_ACCESSIBLE_NAME
          : IDS_EXTENSIONS_MENU_EXTENSION_CONTEXT_MENU_BUTTON_ACCESSIBLE_NAME;
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

std::u16string GetSitePermissionsButtonText(
    ExtensionMenuItemView::SitePermissionsButtonAccess button_access) {
  int label_id;
  switch (button_access) {
    case ExtensionMenuItemView::SitePermissionsButtonAccess::kNone:
      label_id = IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_NONE;
      break;
    case ExtensionMenuItemView::SitePermissionsButtonAccess::kOnClick:
      label_id = IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_CLICK;
      break;
    case ExtensionMenuItemView::SitePermissionsButtonAccess::kOnSite:
      label_id = IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_SITE;
      break;
    case ExtensionMenuItemView::SitePermissionsButtonAccess::kOnAllSites:
      label_id =
          IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ON_ALL_SITES;
      break;
  }
  return l10n_util::GetStringUTF16(label_id);
}

const gfx::VectorIcon& GetPinIcon(bool is_pinned) {
  if (is_pinned) {
    return features::IsChromeRefresh2023() ? kKeepPinFilledChromeRefreshIcon
                                           : views::kUnpinIcon;
  }
  return features::IsChromeRefresh2023() ? kKeepPinChromeRefreshIcon
                                         : views::kPinIcon;
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
                      IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP))
                  .SetAccessibleName(l10n_util::GetStringFUTF16(
                      IDS_EXTENSIONS_MENU_CONTEXT_MENU_TOOLTIP_ACCESSIBLE_NAME,
                      controller_->GetActionName())));

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
    bool is_enterprise,
    std::unique_ptr<ToolbarActionViewController> controller,
    base::RepeatingCallback<void(bool)> site_access_toggle_callback,
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
  const int small_icon_size = provider->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SMALL_SIZE);
  const int icon_label_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int menu_item_vertical_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);
  const int horizontal_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);

  auto site_permissions_button_icon =
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          features::IsChromeRefresh2023()
              ? vector_icons::kSubmenuArrowChromeRefreshIcon
              : vector_icons::kSubmenuArrowIcon,
          ui::kColorIcon,
          features::IsChromeRefresh2023()
              ? small_icon_size
              : gfx::GetDefaultSizeOfVectorIcon(
                    vector_icons::kSubmenuArrowIcon)));
  site_permissions_button_icon_ = site_permissions_button_icon.get();

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
              // Spacing between menu items is done by setting the top margin.
              // Horizontal margins are added by the parent view.
              .SetInteriorMargin(
                  gfx::Insets::TLBR(menu_item_vertical_spacing, 0, 0, 0))
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
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(0, horizontal_spacing, 0, 0))
                      .SetAccessibleName(l10n_util::GetStringFUTF16(
                          IDS_EXTENSIONS_MENU_EXTENSION_SITE_ACCESS_TOGGLE_ACCESSIBLE_NAME,
                          controller_->GetActionName()))
                      .SetCallback(base::BindRepeating(
                          [](views::ToggleButton* toggle_button,
                             base::RepeatingCallback<void(bool)>
                                 site_access_toggle_callback) {
                            site_access_toggle_callback.Run(
                                toggle_button->GetIsOn());
                          },
                          site_access_toggle_, site_access_toggle_callback)),
                  // Context menu button.
                  views::Builder<HoverButton>(
                      std::make_unique<HoverButton>(
                          views::Button::PressedCallback(), std::u16string()))
                      .CopyAddressTo(&context_menu_button_)
                      .SetID(EXTENSION_CONTEXT_MENU)
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(0, horizontal_spacing, 0, 0))
                      .SetBorder(views::CreateEmptyBorder(
                          ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_EXTENSIONS_MENU_BUTTON_MARGIN)))
                      .SetTooltipText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_EXTENSION_CONTEXT_MENU_BUTTON_TOOLTIP))),
          // Secondary row.
          views::Builder<views::FlexLayoutView>().AddChildren(
              // Site permissions button.
              // TODO(crbug.com/1390952): Enterprise icon should appear to the
              // left of the label, instead of the right. HoverButton should
              // take care of this, but for some reason it doesn't.
              views::Builder<HoverButton>(
                  std::make_unique<HoverButton>(
                      site_permissions_button_callback,
                      is_enterprise
                          ? std::make_unique<views::ImageView>(
                                ui::ImageModel::FromVectorIcon(
                                    features::IsChromeRefresh2023()
                                        ? vector_icons::
                                              kBusinessChromeRefreshIcon
                                        : vector_icons::kBusinessIcon,
                                    ui::kColorIcon, small_icon_size))
                          : nullptr,
                      std::u16string(), std::u16string(),
                      std::move(site_permissions_button_icon)))
                  .CopyAddressTo(&site_permissions_button_)
                  // Align the main and secondary row text by adding the primary
                  // action button's icon size as margin.
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets::VH(0, icon_size))
                  // Border should be the same as the space between icon and
                  // label in the primary action button.
                  .SetBorder(views::CreateEmptyBorder(
                      gfx::Insets::VH(0, icon_label_spacing)))
                  .SetTooltipText(l10n_util::GetStringUTF16(
                      IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_TOOLTIP))))
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
        context_menu_button_,
        features::IsChromeRefresh2023() ? kBrowserToolsChromeRefreshIcon
                                        : kBrowserToolsIcon,
        icon_color, color_provider->GetColor(kColorExtensionMenuIconDisabled));
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
    SitePermissionsButtonState site_permissions_button_state,
    SitePermissionsButtonAccess site_permissions_button_access) {
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    bool is_toggle_on = site_access_toggle_state == SiteAccessToggleState::kOn;
    site_access_toggle_->SetVisible(site_access_toggle_state !=
                                    SiteAccessToggleState::kHidden);
    site_access_toggle_->SetIsOn(is_toggle_on);
    site_access_toggle_->SetTooltipText(
        GetSiteAccessToggleTooltip(is_toggle_on));

    site_permissions_button_->SetVisible(site_permissions_button_state !=
                                         SitePermissionsButtonState::kHidden);
    site_permissions_button_->SetEnabled(site_permissions_button_state ==
                                         SitePermissionsButtonState::kEnabled);
    std::u16string site_permissions_text =
        GetSitePermissionsButtonText(site_permissions_button_access);
    site_permissions_button_->SetTitleText(site_permissions_text);
    site_permissions_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ACCESSIBLE_NAME,
        site_permissions_text));
    site_permissions_button_icon_->SetVisible(
        site_permissions_button_state == SitePermissionsButtonState::kEnabled);
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
  SetButtonIconWithColor(pin_button_, GetPinIcon(is_pinned), icon_color,
                         disabled_icon_color);
}

void ExtensionMenuItemView::UpdateContextMenuButton(bool is_action_pinned) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);
  const auto* const color_provider = GetColorProvider();
  auto three_dot_icon = ui::ImageModel::FromVectorIcon(
      features::IsChromeRefresh2023() ? kBrowserToolsChromeRefreshIcon
                                      : kBrowserToolsIcon,
      color_provider->GetColor(kColorExtensionMenuIcon), icon_size);

  // Show a pin button for the context menu normal state icon when the action is
  // pinned in the toolbar. All other states should look, and behave, the same.
  context_menu_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      is_action_pinned
          ? ui::ImageModel::FromVectorIcon(
                features::IsChromeRefresh2023() ? kKeepPinChromeRefreshIcon
                                                : views::kUnpinIcon,
                color_provider->GetColor(kColorExtensionMenuPinButtonIcon),
                icon_size)
          : three_dot_icon);
  context_menu_button_->SetImageModel(views::Button::STATE_HOVERED,
                                      three_dot_icon);
  context_menu_button_->SetImageModel(views::Button::STATE_PRESSED,
                                      three_dot_icon);
  context_menu_button_->SetAccessibleName(
      GetContextMenuAccessibleName(is_action_pinned));
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
