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
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
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
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int EXTENSION_CONTEXT_MENU = 13;
constexpr int EXTENSION_PINNING = 14;

std::u16string GetPinButtonTooltip(bool is_force_pinned, bool is_pinned) {
  int tooltip_id = IDS_EXTENSIONS_PIN_TO_TOOLBAR;
  if (is_force_pinned) {
    tooltip_id = IDS_EXTENSIONS_PINNED_BY_ADMIN;
  } else if (is_pinned) {
    tooltip_id = IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR;
  }
  return l10n_util::GetStringUTF16(tooltip_id);
}

std::u16string GetPinButtonAccessibleName(
    bool is_force_pinned,
    bool is_pinned,
    const std::u16string& extension_name) {
  int tooltip_id = IDS_EXTENSIONS_PIN_TO_TOOLBAR_ACCESSIBLE_NAME;
  if (is_force_pinned) {
    tooltip_id = IDS_EXTENSIONS_PINNED_BY_ADMIN_ACCESSIBLE_NAME;
  } else if (is_pinned) {
    tooltip_id = IDS_EXTENSIONS_UNPIN_FROM_TOOLBAR_ACCESSIBLE_NAME;
  }
  return l10n_util::GetStringFUTF16(tooltip_id, extension_name);
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

std::u16string GetSitePermissionsButtonTooltip(
    bool is_enterprise,
    ExtensionMenuItemView::SitePermissionsButtonAccess button_access) {
  if (is_enterprise) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_MENU_MAIN_PAGE_ENTERPRISE_EXTENSION_SITE_ACCESS_TOOLTIP);
  }

  if (button_access !=
      ExtensionMenuItemView::SitePermissionsButtonAccess::kNone) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_TOOLTIP);
  }

  // No tooltip is shown.
  return std::u16string();
}

std::u16string GetSitePermissionsButtonAccName(
    bool is_enterprise,
    ExtensionMenuItemView::SitePermissionsButtonAccess button_access,
    std::u16string& button_text) {
  if (is_enterprise) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_MENU_MAIN_PAGE_ENTERPRISE_EXTENSION_SITE_ACCESS_ACCESSIBLE_NAME,
        button_text);
  }

  if (button_access !=
      ExtensionMenuItemView::SitePermissionsButtonAccess::kNone) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_SITE_ACCESS_ACCESSIBLE_NAME,
        button_text);
  }

  return button_text;
}

views::Builder<HoverButton> GetSitePermissionsButtonBuilder(
    views::Button::PressedCallback callback,
    bool is_enterprise,
    int small_icon_size,
    int icon_size,
    int icon_label_spacing) {
  auto button_builder =
      views::Builder<HoverButton>(
          std::make_unique<HoverButton>(std::move(callback), std::u16string()))
          .SetTitleTextStyle(views::style::STYLE_BODY_5,
                             ui::kColorDialogBackground,
                             kColorExtensionsMenuSecondaryText)
          // Align the main and secondary row text by adding the primary
          // action button's icon size as margin.
          .SetProperty(views::kMarginsKey, gfx::Insets::VH(0, icon_size))
          // Border should be the same as the space between icon and
          // label in the primary action button.
          .SetBorder(
              views::CreateEmptyBorder(gfx::Insets::VH(0, icon_label_spacing)));

  if (is_enterprise) {
    // Add left-aligned business icon for enterprise extensions.
    button_builder.SetHorizontalAlignment(gfx::ALIGN_LEFT)
        .SetImageModel(views::Button::ButtonState::STATE_NORMAL,
                       ui::ImageModel::FromVectorIcon(
                           vector_icons::kBusinessChromeRefreshIcon,
                           ui::kColorIcon, small_icon_size));

  } else {
    // Add right-aligned arrow icon for non-enterprise extensions when the
    // button is not disabled.
    auto arrow_icon = ui::ImageModel::FromVectorIcon(
        vector_icons::kSubmenuArrowChromeRefreshIcon, ui::kColorIcon,
        small_icon_size);

    button_builder.SetHorizontalAlignment(gfx::ALIGN_RIGHT)
        .SetImageModel(views::Button::ButtonState::STATE_NORMAL, arrow_icon)
        .SetImageModel(views::Button::ButtonState::STATE_DISABLED,
                       ui::ImageModel());
  }

  return button_builder;
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

  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);

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
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded)),
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
                      controller_->GetActionName()))
                  .SetImageModel(views::Button::STATE_NORMAL,
                                 ui::ImageModel::FromVectorIcon(
                                     kBrowserToolsChromeRefreshIcon,
                                     kColorExtensionMenuIcon, icon_size))
                  .SetImageModel(
                      views::Button::STATE_DISABLED,
                      ui::ImageModel::FromVectorIcon(
                          kBrowserToolsChromeRefreshIcon,
                          kColorExtensionMenuIconDisabled, icon_size)));

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

    // By default, the button's accessible description is set to the button's
    // tooltip text. For the pin button, we only want the accessible name to be
    // read on accessibility mode since it includes the tooltip text. Thus we
    // set the accessible description.
    pin_button_->GetViewAccessibility().SetDescription(
        std::u16string(),
        ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);

    bool is_pinned = model_ && model_->IsActionPinned(controller_->GetId());
    bool is_force_pinned =
        model_ && model_->IsActionForcePinned(controller_->GetId());
    UpdatePinButton(is_force_pinned, is_pinned);
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

  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  const int icon_size =
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  const int small_icon_size = provider->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SMALL_SIZE);
  const int icon_label_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int horizontal_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
  const gfx::Insets icon_padding =
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON);

  views::Builder<ExtensionMenuItemView>(this)
      // Set so the extension button receives enter/exit on children to
      // retain hover status when hovering child views.
      .SetNotifyEnterExitOnChild(true)
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetProperty(views::kBoxLayoutFlexKey,
                   views::BoxLayoutFlexSpecification())
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
                      .SetTitleTextStyle(views::style::STYLE_BODY_3_EMPHASIS,
                                         ui::kColorDialogBackground,
                                         kColorExtensionsMenuText)
                      .SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(
                                       views::LayoutOrientation::kHorizontal,
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded)),
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
                      // Override the hover button border since we are
                      // adding vertical spacing in between menu items.
                      // Instead, set the border to be the padding around the
                      // icon when hovering.
                      .SetBorder(views::CreateEmptyBorder(icon_padding))
                      .SetTooltipText(l10n_util::GetStringUTF16(
                          IDS_EXTENSIONS_MENU_EXTENSION_CONTEXT_MENU_BUTTON_TOOLTIP))
                      // TODO(crbug.com/40857680): Context menu button can
                      // be an ImageButton instead of HoverButton since we
                      // manually add a circle highlight. Change this once
                      // kExtensionsMenuAccessControl is rolled out and we
                      // remove the older menu implementation, which relies
                      // on `context_menu_button_` being a HoverButton.
                      .CustomConfigure(base::BindOnce([](HoverButton* view) {
                        InstallCircleHighlightPathGenerator(view);
                      }))),
          // Secondary row.
          views::Builder<views::FlexLayoutView>().AddChildren(
              GetSitePermissionsButtonBuilder(
                  std::move(site_permissions_button_callback), is_enterprise,
                  small_icon_size, icon_size, icon_label_spacing)
                  .CopyAddressTo(&site_permissions_button_)))
      .BuildChildren();

  SetupContextMenuButton();

  // By default, the button's accessible description is set to the button's
  // tooltip text. This is the accepted workaround to ensure only accessible
  // name is announced by a screenreader rather than tooltip text and
  // accessible name.
  site_access_toggle_->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  site_permissions_button_->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
}

ExtensionMenuItemView::~ExtensionMenuItemView() = default;

void ExtensionMenuItemView::Update(
    SiteAccessToggleState site_access_toggle_state,
    SitePermissionsButtonState site_permissions_button_state,
    SitePermissionsButtonAccess site_permissions_button_access,
    bool is_enterprise) {
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
    site_permissions_button_->SetText(site_permissions_text);
    site_permissions_button_->SetTooltipText(GetSitePermissionsButtonTooltip(
        is_enterprise, site_permissions_button_access));
    site_permissions_button_->GetViewAccessibility().SetName(
        GetSitePermissionsButtonAccName(is_enterprise,
                                        site_permissions_button_access,
                                        site_permissions_text));

    // Update button size after changing its contents so it fits in the menu
    // item row.
    site_permissions_button_->PreferredSizeChanged();
  }

  view_controller()->UpdateState();
}

void ExtensionMenuItemView::UpdatePinButton(bool is_force_pinned,
                                            bool is_pinned) {
  if (!pin_button_) {
    return;
  }

  pin_button_->SetTooltipText(GetPinButtonTooltip(is_force_pinned, is_pinned));
  pin_button_->GetViewAccessibility().SetName(GetPinButtonAccessibleName(
      is_force_pinned, is_pinned, controller_->GetActionName()));
  // Extension pinning is not available in Incognito as it leaves a trace of
  // user activity.
  pin_button_->SetEnabled(!is_force_pinned &&
                          !browser_->profile()->IsOffTheRecord());

  // Update the icon based on whether the extension is pinned.
  const gfx::VectorIcon& icon = is_pinned ? kKeepFilledIcon : kKeepIcon;
  const ui::ColorId icon_color_id =
      is_pinned ? kColorExtensionMenuPinButtonIcon : kColorExtensionMenuIcon;
  const ui::ColorId disabled_icon_color_id =
      is_pinned ? kColorExtensionMenuPinButtonIconDisabled
                : kColorExtensionMenuIconDisabled;
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);

  pin_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(icon, icon_color_id, icon_size));
  pin_button_->SetImageModel(
      views::Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(icon, disabled_icon_color_id, icon_size));
}

void ExtensionMenuItemView::UpdateContextMenuButton(bool is_action_pinned) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);
  auto three_dot_icon = ui::ImageModel::FromVectorIcon(
      kBrowserToolsChromeRefreshIcon, kColorExtensionMenuIcon, icon_size);

  // Show a pin button for the context menu normal state icon when the action is
  // pinned in the toolbar. All other states should look, and behave, the same.
  context_menu_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      is_action_pinned
          ? ui::ImageModel::FromVectorIcon(
                kKeepIcon, kColorExtensionMenuPinButtonIcon, icon_size)
          : three_dot_icon);
  context_menu_button_->SetImageModel(views::Button::STATE_HOVERED,
                                      three_dot_icon);
  context_menu_button_->SetImageModel(views::Button::STATE_PRESSED,
                                      three_dot_icon);
  context_menu_button_->GetViewAccessibility().SetName(
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

  // By default, the button's accessible description is set to the button's
  // tooltip text. This is the accepted workaround to ensure only accessible
  // name is announced by a screenreader rather than tooltip text and
  // accessible name.
  context_menu_button_->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    bool is_action_pinned = model_->IsActionPinned(controller_->GetId());
    UpdateContextMenuButton(is_action_pinned);
  }
}

void ExtensionMenuItemView::OnContextMenuPressed() {
  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu"));
  // TODO(crbug.com/41478477): Cleanup the menu source type.
  context_menu_controller_->ShowContextMenuForViewImpl(
      context_menu_button_, context_menu_button_->GetMenuPosition(),
      ui::MenuSourceType::MENU_SOURCE_MOUSE);
}

void ExtensionMenuItemView::OnPinButtonPressed() {
  CHECK(model_);
  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.PinButtonPressed"));
  // Toggle action visibility.
  bool new_action_visibility = !model_->IsActionPinned(controller_->GetId());
  model_->SetActionVisibility(controller_->GetId(), new_action_visibility);
  GetViewAccessibility().AnnounceText(
      GetPinButtonPressedAccText(new_action_visibility));
}

bool ExtensionMenuItemView::IsContextMenuRunningForTesting() const {
  return context_menu_controller_->IsMenuRunning();
}

BEGIN_METADATA(ExtensionMenuItemView)
END_METADATA
