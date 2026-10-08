// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_item_view.h"

#include <optional>
#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "ui/actions/actions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

ui::ImageModel StandardizeMenuIconSize(const ui::ImageModel& icon,
                                       int icon_size) {
  if (icon.IsVectorIcon()) {
    const ui::VectorIconModel& vector_model = icon.GetVectorIcon();
    if (vector_model.icon_size() != icon_size) {
      return ui::ImageModel::FromVectorIcon(*vector_model.vector_icon(),
                                            vector_model.color(), icon_size,
                                            vector_model.badge_icon());
    }
  }
  return icon;
}

bool ShouldShowNewBadge(BrowserWindowInterface* browser_window_interface,
                        const base::Feature& feature) {
  if (auto* const user_education =
          BrowserUserEducationInterface::From(browser_window_interface)) {
    return user_education->MaybeShowNewBadgeFor(feature);
  }
  return UserEducationService::MaybeShowNewBadge(
      browser_window_interface->GetProfile(), feature);
}

class AppMenuItemActionViewInterface
    : public views::MenuItemActionViewInterface {
 public:
  explicit AppMenuItemActionViewInterface(AppMenuItemView* action_view)
      : views::MenuItemActionViewInterface(action_view) {}
  AppMenuItemActionViewInterface(const AppMenuItemActionViewInterface&) =
      delete;
  AppMenuItemActionViewInterface& operator=(
      const AppMenuItemActionViewInterface&) = delete;
  ~AppMenuItemActionViewInterface() override = default;

  // views::MenuItemActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override {
    views::MenuItemActionViewInterface::ActionItemChangedImpl(action_item);

    auto* menu_item = views::AsViewClass<AppMenuItemView>(action_view());
    CHECK(menu_item);

    if (menu_item->text_override().has_value()) {
      menu_item->SetTitle(*menu_item->text_override());
    }

    const int default_icon_size = menu_item->default_icon_size();
    if (menu_item->icon_override().has_value()) {
      menu_item->SetIcon(
          action_item->GetActionId() == kActionProfileSubmenu
              ? *menu_item->icon_override()
              : StandardizeMenuIconSize(*menu_item->icon_override(),
                                        default_icon_size));
    } else if (!action_item->GetImage().IsEmpty()) {
      menu_item->SetIcon(
          StandardizeMenuIconSize(action_item->GetImage(), default_icon_size));
    }

    // Display shortcut text if the ActionItem has one.
    const ui::Accelerator& accel = action_item->GetAccelerator();
    if (accel.key_code() != ui::VKEY_UNKNOWN) {
      menu_item->SetMinorText(accel.GetShortcutText());
    }

    if (!action_item->GetAccessibleName().empty()) {
      menu_item->GetViewAccessibility().SetName(
          std::u16string(action_item->GetAccessibleName()));
    }
  }
};

}  // namespace

AppMenuItemView::AppMenuItemView(views::MenuItemView* parent,
                                 int command,
                                 Type type,
                                 actions::BaseAction* base_action,
                                 BrowserWindowInterface* browser_window)
    : views::MenuItemView(parent, command, type) {
  const auto* provider = ChromeLayoutProvider::Get();
  const bool is_notification =
      base_action->GetProperty(AppMenuActionItem::kDisplayTypeKey) ==
      AppMenuActionItem::DisplayType::kNotification;
  default_icon_size_ = provider->GetDistanceMetric(
      is_notification ? DISTANCE_ACTION_APP_MENU_NOTIFICATION_ICON_SIZE
                      : DISTANCE_ACTION_APP_MENU_DEFAULT_ICON_SIZE);

  if (std::u16string* text_override =
          base_action->GetProperty(AppMenuActionItem::kTextOverrideKey)) {
    text_override_ = *text_override;
  }

  if (ui::ImageModel* icon_override =
          base_action->GetProperty(AppMenuActionItem::kIconOverrideKey)) {
    icon_override_ = *icon_override;
  }

  if (std::u16string* secondary_text =
          base_action->GetProperty(AppMenuActionItem::kSecondaryTextKey)) {
    SetSecondaryTitle(*secondary_text);
  }

  if (const ui::ElementIdentifier element_id =
          base_action->GetProperty(views::kElementIdentifierKey)) {
    SetProperty(views::kElementIdentifierKey, element_id);
  }

  if (ui::ImageModel* minor_icon =
          base_action->GetProperty(AppMenuActionItem::kMinorIconKey)) {
    SetMinorIcon(*minor_icon);
  }

  if (const base::Feature* new_badge_feature =
          base_action->GetProperty(AppMenuActionItem::kNewBadgeFeatureKey)) {
    const bool show_new_badge =
        ShouldShowNewBadge(browser_window, *new_badge_feature);
    set_new_badge_type(show_new_badge
                           ? std::make_optional(ui::NewBadgeType::kNew)
                           : std::nullopt);
  }

  if (base_action->GetProperty(AppMenuActionItem::kIsAlertedKey)) {
    SetAlerted();
  }
}

AppMenuItemView::~AppMenuItemView() = default;

std::unique_ptr<views::ActionViewInterface>
AppMenuItemView::GetActionViewInterface() {
  return std::make_unique<AppMenuItemActionViewInterface>(this);
}

BEGIN_METADATA(AppMenuItemView)
END_METADATA
