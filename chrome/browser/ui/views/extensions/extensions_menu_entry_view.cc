// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_entry_view.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/vector_icons.h"

namespace {

views::Builder<HoverButton> GetSitePermissionsButtonBuilder(
    views::Button::PressedCallback callback,
    bool is_enterprise,
    int small_icon_size,
    int icon_size,
    int icon_label_spacing,
    int button_icon_label_spacing) {
  auto button_builder =
      views::Builder<HoverButton>(
          std::make_unique<HoverButton>(std::move(callback), std::u16string()))
          .SetLabelStyle(views::style::STYLE_BODY_5)
          .SetEnabledTextColors(kColorExtensionsMenuSecondaryText)
          .SetTextColor(views::Button::ButtonState::STATE_DISABLED,
                        kColorExtensionsMenuSecondaryText)
          .SetImageLabelSpacing(button_icon_label_spacing)
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
                           kColorExtensionMenuIcon, small_icon_size));
  } else {
    // Add right-aligned arrow icon for non-enterprise extensions when the
    // button is not disabled.
    auto arrow_icon = ui::ImageModel::FromVectorIcon(
        vector_icons::kSubmenuArrowChromeRefreshIcon, kColorExtensionMenuIcon,
        small_icon_size);

    button_builder.SetHorizontalAlignment(gfx::ALIGN_RIGHT)
        .SetImageModel(views::Button::ButtonState::STATE_NORMAL, arrow_icon)
        .SetImageModel(views::Button::ButtonState::STATE_DISABLED,
                       ui::ImageModel());
  }

  return button_builder;
}

}  // namespace

DEFINE_ELEMENT_IDENTIFIER_VALUE(kExtensionsMenuEntryViewElementId);

ExtensionsMenuEntryView::ExtensionsMenuEntryView(
    Browser* browser,
    bool is_enterprise,
    ToolbarActionViewModel* view_model,
    views::Button::PressedCallback action_button_callback,
    base::RepeatingCallback<void(bool)> site_access_toggle_callback,
    views::Button::PressedCallback site_permissions_button_callback)
    : extension_id_(view_model->GetId()) {
  CHECK(base::FeatureList::IsEnabled(
      extensions_features::kExtensionsMenuAccessControl));

  ChromeLayoutProvider* const provider = ChromeLayoutProvider::Get();
  const int icon_size =
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_EXTENSION_ICON_SIZE);
  const int small_icon_size = provider->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SMALL_SIZE);
  const int icon_label_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int button_icon_label_spacing =
      provider->GetDistanceMetric(DISTANCE_EXTENSIONS_MENU_LABEL_ICON_SPACING);
  const int horizontal_spacing =
      provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL_LIST);
  const gfx::Insets icon_padding =
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON);

  views::Builder<ExtensionsMenuEntryView>(this)
      // Set so the extension button receives enter/exit on children to
      // retain hover status when hovering child views.
      .SetNotifyEnterExitOnChild(true)
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetProperty(views::kElementIdentifierKey,
                   kExtensionsMenuEntryViewElementId)
      .AddChildren(
          // Main row.
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kHorizontal)
              .SetIgnoreDefaultMainAxisMargins(true)
              .AddChildren(
                  // Action button.
                  views::Builder<HoverButton>(
                      std::make_unique<HoverButton>(
                          std::move(action_button_callback), std::u16string()))
                      .CopyAddressTo(&action_button_)
                      // Button is always visible.
                      .SetVisible(true)
                      .SetTitleTextStyle(views::style::STYLE_BODY_3_EMPHASIS,
                                         ui::kColorDialogBackground,
                                         kColorExtensionsMenuText)
                      .SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(
                                       views::LayoutOrientation::kHorizontal,
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
                      // Remove the button's border since we are adding margins
                      // in between menu items.
                      .SetBorder(views::CreateEmptyBorder(gfx::Insets(0)))
                      .SetFocusRingCornerRadius(provider->GetCornerRadiusMetric(
                          views::ShapeContextTokens::
                              kExtensionsMenuButtonRadius))
                      .SetFocusBehavior(views::View::FocusBehavior::ALWAYS),
                  // Site access toggle.
                  views::Builder<views::ToggleButton>()
                      .CopyAddressTo(&site_access_toggle_)
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(0, horizontal_spacing, 0, 0))
                      .SetAccessibleName(l10n_util::GetStringFUTF16(
                          IDS_EXTENSIONS_MENU_EXTENSION_SITE_ACCESS_TOGGLE_ACCESSIBLE_NAME,
                          view_model->GetActionName()))
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
                      .SetProperty(
                          views::kMarginsKey,
                          gfx::Insets::TLBR(0, horizontal_spacing, 0, 0))
                      // Override the hover button border since we are
                      // adding vertical spacing in between menu entries.
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
                  small_icon_size, icon_size, icon_label_spacing,
                  button_icon_label_spacing)
                  .CopyAddressTo(&site_permissions_button_)))
      .BuildChildren();

  SetupContextMenuButton(view_model);

  // By default, the button's accessible description is set to the button's
  // tooltip text. This is the accepted workaround to ensure only accessible
  // name is announced by a screenreader rather than tooltip text and
  // accessible name.
  site_access_toggle_->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  site_permissions_button_->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);

  // Add rounded corners to the site permissions button.
  site_permissions_button_->SetFocusRingCornerRadius(
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kExtensionsMenuButtonRadius));
}

ExtensionsMenuEntryView::~ExtensionsMenuEntryView() = default;

void ExtensionsMenuEntryView::Update(
    ExtensionsMenuViewModel::MenuEntryState entry_state) {
  site_access_toggle_->SetVisible(
      entry_state.site_access_toggle.status !=
      ExtensionsMenuViewModel::ControlState::Status::kHidden);
  site_access_toggle_->SetIsOn(entry_state.site_access_toggle.is_on);
  site_access_toggle_->SetTooltipText(
      entry_state.site_access_toggle.tooltip_text);

  site_permissions_button_->SetVisible(
      entry_state.site_permissions_button.status !=
      ExtensionsMenuViewModel::ControlState::Status::kHidden);
  site_permissions_button_->SetEnabled(
      entry_state.site_permissions_button.status ==
      ExtensionsMenuViewModel::ControlState::Status::kEnabled);
  site_permissions_button_->SetText(entry_state.site_permissions_button.text);
  site_permissions_button_->SetTooltipText(
      entry_state.site_permissions_button.tooltip_text);
  site_permissions_button_->GetViewAccessibility().SetName(
      entry_state.site_permissions_button.accessible_name);

  // Update button size after changing its contents so it fits in the menu
  // entry row.
  site_permissions_button_->PreferredSizeChanged();

  UpdateActionButton(entry_state.action_button);
  UpdateContextMenuButton(entry_state.context_menu_button);
}

void ExtensionsMenuEntryView::UpdateActionButton(
    const ExtensionsMenuViewModel::ControlState& button_state) {
  action_button_->SetImageModel(views::Button::STATE_NORMAL, button_state.icon);
  action_button_->SetText(button_state.text);
  action_button_->SetTooltipText(button_state.tooltip_text);
  action_button_->SetAccessibleName(button_state.accessible_name);
  action_button_->SetEnabled(
      button_state.status ==
      ExtensionsMenuViewModel::ControlState::Status::kEnabled);
}

void ExtensionsMenuEntryView::UpdateContextMenuButton(
    ExtensionsMenuViewModel::ControlState button_state) {
  const int icon_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_EXTENSIONS_MENU_BUTTON_ICON_SIZE);
  auto three_dot_icon = ui::ImageModel::FromVectorIcon(
      kBrowserToolsChromeRefreshIcon, kColorExtensionMenuIcon, icon_size);

  // Show a pin button for the context menu normal state icon when the action is
  // pinned in the toolbar. All other states should look, and behave, the same.
  context_menu_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      button_state.is_on ? ui::ImageModel::FromVectorIcon(
                               kKeepIcon, kColorExtensionMenuIcon, icon_size)
                         : three_dot_icon);
  context_menu_button_->SetImageModel(views::Button::STATE_HOVERED,
                                      three_dot_icon);
  context_menu_button_->SetImageModel(views::Button::STATE_PRESSED,
                                      three_dot_icon);
  context_menu_button_->GetViewAccessibility().SetName(
      button_state.accessible_name);
}

void ExtensionsMenuEntryView::SetupContextMenuButton(
    ToolbarActionViewModel* view_model) {
  // Add a controller to the context menu
  context_menu_controller_ = std::make_unique<ExtensionContextMenuController>(
      view_model, this,
      extensions::ExtensionContextMenuModel::ContextMenuSource::kMenuItem);

  context_menu_button_->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          context_menu_button_.get(),
          base::BindRepeating(&ExtensionsMenuEntryView::OnContextMenuPressed,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              context_menu_button_.get())));

  // By default, the button's accessible description is set to the button's
  // tooltip text. This is the accepted workaround to ensure only accessible
  // name is announced by a screenreader rather than tooltip text and
  // accessible name.
  context_menu_button_->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
}

void ExtensionsMenuEntryView::OnContextMenuPressed() {
  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu"));
  // TODO(crbug.com/41478477): Cleanup the menu source type.
  context_menu_controller_->ShowContextMenuForViewImpl(
      context_menu_button_, context_menu_button_->GetMenuPosition(),
      ui::mojom::MenuSourceType::kMouse);
}

bool ExtensionsMenuEntryView::IsContextMenuRunningForTesting() const {
  return context_menu_controller_->IsMenuRunning();
}

void ExtensionsMenuEntryView::OnContextMenuShown() {
  // Nothing to do.
}

void ExtensionsMenuEntryView::OnContextMenuClosed() {
  // Nothing to do.
}

BEGIN_METADATA(ExtensionsMenuEntryView)
END_METADATA
