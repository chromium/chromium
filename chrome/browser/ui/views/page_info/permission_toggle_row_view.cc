// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/page_info_row_view.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

PermissionToggleRowView::PermissionToggleRowView(
    ChromePageInfoUiDelegate* delegate,
    const PageInfo::PermissionInfo& permission)
    : permission_(permission) {
  SetUseDefaultFillLayout(true);
  row_view_ = AddChildView(std::make_unique<PageInfoRowView>());
  row_view_->SetTitle(PageInfoUI::PermissionTypeToUIString(permission.type));
  row_view_->SetIcon(PageInfoViewFactory::GetPermissionIcon(permission));

  // Add extra details as sublabel.
  std::u16string detail = delegate->GetPermissionDetail(permission.type);
  if (!detail.empty())
    row_view_->AddSecondaryLabel(detail);

  // Show the permission decision reason, if it was not the user.
  // TODO(olesiamarukhno): Add correct handling of the managed states: add
  // showing correct state text "allowed" instead of "allow", "not allowed"
  // instead of "block"; update tooltip for managed state; update the reason
  // label to not include managed state, maybe merge it with permission detail.
  std::u16string reason =
      PageInfoUI::PermissionDecisionReasonToUIString(delegate, permission);
  if (!reason.empty())
    row_view_->AddSecondaryLabel(reason);

  if (permission.source == content_settings::SETTING_SOURCE_USER) {
    InitForUserSource();
  } else {
    InitForManagedSource(delegate);
  }
}

PermissionToggleRowView::~PermissionToggleRowView() = default;

void PermissionToggleRowView::AddObserver(
    PermissionSelectorRowObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionToggleRowView::PermissionChanged(
    const PageInfo::PermissionInfo& permission) {
  // Change the permission icon to reflect the selected setting.
  row_view_->SetIcon(PageInfoViewFactory::GetPermissionIcon(permission));

  for (PermissionSelectorRowObserver& observer : observer_list_) {
    observer.OnPermissionChanged(permission);
  }
}

void PermissionToggleRowView::OnToggleButtonPressed() {
  switch (permission_.setting) {
    case CONTENT_SETTING_ALLOW:
      permission_.setting = permission_.is_one_time ? CONTENT_SETTING_DEFAULT
                                                    : CONTENT_SETTING_BLOCK;
      permission_.is_one_time = false;
      break;
    case CONTENT_SETTING_BLOCK:
      permission_.setting = CONTENT_SETTING_ALLOW;
      permission_.is_one_time = false;
      break;
    case CONTENT_SETTING_DEFAULT:
      permission_.setting = CONTENT_SETTING_ALLOW;
      // If one-time permissions are supported, permission should go from
      // default state to allow once state, not directly to allow.
      if (base::FeatureList::IsEnabled(
              permissions::features::kOneTimeGeolocationPermission) &&
          permission_.type == ContentSettingsType::GEOLOCATION) {
        permission_.is_one_time = true;
      }
      break;
    default:
      break;
  }
  PermissionChanged(permission_);
}

void PermissionToggleRowView::InitForUserSource() {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto toggle_button = std::make_unique<views::ToggleButton>(
      base::BindRepeating(&PermissionToggleRowView::OnToggleButtonPressed,
                          base::Unretained(this)));
  toggle_button->SetIsOn(permission_.setting == CONTENT_SETTING_ALLOW);
  toggle_button->SetPreferredSize({toggle_button->GetPreferredSize().width(),
                                   row_view_->GetFirstLineHeight()});
  toggle_button->SetProperty(views::kMarginsKey,
                             gfx::Insets(0, icon_label_spacing));
  row_view_->AddControl(std::move(toggle_button));

  auto subpage_button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(
          [=](PermissionToggleRowView* row) {
            // TODO(olesiamarukhno): Add opening permissions page.
          },
          base::Unretained(this)),
      vector_icons::kSubmenuArrowIcon);
  views::InstallCircleHighlightPathGenerator(subpage_button.get());
  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);
  subpage_button->SetMinimumImageSize({icon_size, icon_size});

  // If type isn't supported by the PermissionManager, don't show the button
  // that opens subpage. Instead, use a spacer view to align this row with
  // other rows.
  if (!permissions::PermissionUtil::IsPermission(permission_.type)) {
    auto spacer_view = std::make_unique<views::View>();
    spacer_view->SetPreferredSize(subpage_button->GetMinimumImageSize());
    row_view_->AddControl(std::move(spacer_view));
    subpage_button->SetVisible(false);
  }

  row_view_->AddControl(std::move(subpage_button));
}

void PermissionToggleRowView::InitForManagedSource(
    ChromePageInfoUiDelegate* delegate) {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto state_label = std::make_unique<views::Label>(
      PageInfoUI::PermissionActionToUIString(
          delegate, permission_.type, permission_.setting,
          permission_.default_setting, permission_.source,
          permission_.is_one_time),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  state_label->SetProperty(views::kMarginsKey,
                           gfx::Insets(0, icon_label_spacing));
  std::u16string reason =
      PageInfoUI::PermissionDecisionReasonToUIString(delegate, permission_);
  if (!reason.empty())
    state_label->SetTooltipText(reason);
  row_view_->AddControl(std::move(state_label));

  auto managed_icon = std::make_unique<NonAccessibleImageView>();
  managed_icon->SetImage(PageInfoViewFactory::GetManagedIcon());
  row_view_->AddControl(std::move(managed_icon));
}
