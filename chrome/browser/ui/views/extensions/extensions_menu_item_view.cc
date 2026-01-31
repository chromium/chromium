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
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/extensions/extension_context_menu_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/common/extension_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory.h"
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

std::u16string GetPinButtonPressedAccText(bool is_pinned) {
  return l10n_util::GetStringUTF16(is_pinned ? IDS_EXTENSION_PINNED
                                             : IDS_EXTENSION_UNPINNED);
}

}  // namespace

ExtensionMenuItemView::ExtensionMenuItemView(
    Browser* browser,
    std::unique_ptr<ToolbarActionViewModel> view_model,
    bool allow_pinning)
    : browser_(browser),
      view_model_(std::move(view_model)),
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
                                                         view_model_.get()))
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
                      view_model_->GetActionName()))
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

    bool is_pinned = model_ && model_->IsActionPinned(view_model_->GetId());
    bool is_force_pinned =
        model_ && model_->IsActionForcePinned(view_model_->GetId());
    UpdatePinButton(is_force_pinned, is_pinned);
  }

  std::move(builder).BuildChildren();

  SetupContextMenuButton();
}

ExtensionMenuItemView::~ExtensionMenuItemView() = default;

void ExtensionMenuItemView::UpdatePinButton(bool is_force_pinned,
                                            bool is_pinned) {
  if (!pin_button_) {
    return;
  }

  pin_button_->SetTooltipText(GetPinButtonTooltip(is_force_pinned, is_pinned));
  pin_button_->GetViewAccessibility().SetName(GetPinButtonAccessibleName(
      is_force_pinned, is_pinned, view_model_->GetActionName()));
  // Extension pinning is not available in Incognito as it leaves a trace of
  // user activity.
  pin_button_->SetEnabled(!is_force_pinned &&
                          !browser_->profile()->IsOffTheRecord());

  // Update the icon based on whether the extension is pinned.
  const gfx::VectorIcon& icon = is_pinned ? kKeepOffIcon : kKeepIcon;
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

void ExtensionMenuItemView::SetupContextMenuButton() {
  // Add a controller to the context menu
  context_menu_controller_ = std::make_unique<ExtensionContextMenuController>(
      view_model_.get(), this,
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
}

void ExtensionMenuItemView::OnContextMenuPressed() {
  base::RecordAction(base::UserMetricsAction(
      "Extensions.Toolbar.MoreActionsButtonPressedFromMenu"));
  // TODO(crbug.com/41478477): Cleanup the menu source type.
  context_menu_controller_->ShowContextMenuForViewImpl(
      context_menu_button_, context_menu_button_->GetMenuPosition(),
      ui::mojom::MenuSourceType::kMouse);
}

void ExtensionMenuItemView::OnPinButtonPressed() {
  CHECK(model_);
  base::RecordAction(
      base::UserMetricsAction("Extensions.Toolbar.PinButtonPressed"));
  // Toggle action visibility.
  bool new_action_visibility = !model_->IsActionPinned(view_model_->GetId());
  model_->SetActionVisibility(view_model_->GetId(), new_action_visibility);
  GetViewAccessibility().AnnounceText(
      GetPinButtonPressedAccText(new_action_visibility));
}

bool ExtensionMenuItemView::IsContextMenuRunningForTesting() const {
  return context_menu_controller_->IsMenuRunning();
}

ExtensionsMenuButton*
ExtensionMenuItemView::primary_action_button_for_testing() {
  return primary_action_button_;
}

HoverButton* ExtensionMenuItemView::context_menu_button_for_testing() {
  return context_menu_button_;
}

HoverButton* ExtensionMenuItemView::pin_button_for_testing() {
  return pin_button_;
}

void ExtensionMenuItemView::OnContextMenuShown() {
  // Nothing to do.
}

void ExtensionMenuItemView::OnContextMenuClosed() {
  // Nothing to do.
}

BEGIN_METADATA(ExtensionMenuItemView)
END_METADATA
