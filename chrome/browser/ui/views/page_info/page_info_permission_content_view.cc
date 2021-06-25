// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_permission_content_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/permissions/features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

namespace {
bool CanBeAllowedOnce(ContentSettingsType type) {
  return base::FeatureList::IsEnabled(
             permissions::features::kOneTimeGeolocationPermission) &&
         type == ContentSettingsType::GEOLOCATION;
}
}  // namespace

PageInfoPermissionContentView::PageInfoPermissionContentView(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate,
    ContentSettingsType type)
    : presenter_(presenter), type_(type), ui_delegate_(ui_delegate) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();

  // Use the same insets as buttons and permission rows in the main page for
  // consistency.
  const auto button_insets =
      layout_provider->GetInsetsMetric(INSETS_PAGE_INFO_HOVER_BUTTON);
  const int controls_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  auto* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager->SetOrientation(views::LayoutOrientation::kVertical);

  auto* permission_info_container =
      AddChildView(std::make_unique<views::View>());
  permission_info_container
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(button_insets);

  icon_ = permission_info_container->AddChildView(
      std::make_unique<NonAccessibleImageView>());

  auto* label_wrapper = permission_info_container->AddChildView(
      PageInfoViewFactory::CreateLabelWrapper());
  title_ = label_wrapper->AddChildView(
      std::make_unique<views::Label>(PageInfoUI::PermissionTypeToUIString(type),
                                     views::style::CONTEXT_DIALOG_BODY_TEXT));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  state_label_ = label_wrapper->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY));
  state_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  remember_setting_ =
      label_wrapper->AddChildView(std::make_unique<views::Checkbox>(
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_REMEMBER_THIS_SETTING),
          base::BindRepeating(
              &PageInfoPermissionContentView::OnRememberSettingPressed,
              base::Unretained(this)),
          views::style::CONTEXT_DIALOG_BODY_TEXT));
  remember_setting_->SetProperty(views::kMarginsKey,
                                 gfx::Insets(controls_spacing, 0, 0, 0));

  const int title_height = title_->GetPreferredSize().height();
  toggle_button_ = permission_info_container->AddChildView(
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &PageInfoPermissionContentView::OnToggleButtonPressed,
          base::Unretained(this))));
  toggle_button_->SetPreferredSize(
      gfx::Size(toggle_button_->GetPreferredSize().width(), title_height));

  // Calculate difference between label height and icon size to align icons
  // and label in the first row.
  const int margin =
      (title_height - GetLayoutConstant(PAGE_INFO_ICON_SIZE)) / 2;
  icon_->SetProperty(views::kMarginsKey, gfx::Insets(margin, 0));
  toggle_button_->SetProperty(views::kMarginsKey, gfx::Insets(margin, 0));

  AddChildView(PageInfoViewFactory::CreateSeparator());
  // TODO(olesiamarukhno): Add toolip for manage button.
  AddChildView(std::make_unique<PageInfoHoverButton>(
      base::BindRepeating(
          [](PageInfoPermissionContentView* view) {
            view->presenter_->OpenContentSettingsExceptions(view->type_);
          },
          this),
      PageInfoViewFactory::GetSiteSettingsIcon(),
      IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_MANAGE_BUTTON, std::u16string(), 0,
      std::u16string(), std::u16string()));

  presenter_->InitializeUiState(this);
}

PageInfoPermissionContentView::~PageInfoPermissionContentView() = default;

void PageInfoPermissionContentView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  auto permission_it =
      std::find_if(permission_info_list.begin(), permission_info_list.end(),
                   [=](PageInfo::PermissionInfo permission_info) {
                     return permission_info.type == type_;
                   });

  CHECK(permission_it != permission_info_list.end());

  permission_ = *permission_it;
  icon_->SetImage(PageInfoViewFactory::GetPermissionIcon(permission_));

  state_label_->SetText(PageInfoUI::PermissionActionToUIString(
      ui_delegate_, permission_.type, permission_.setting,
      permission_.default_setting, permission_.source,
      permission_.is_one_time));

  auto setting = permission_.setting == CONTENT_SETTING_DEFAULT
                     ? permission_.default_setting
                     : permission_.setting;
  toggle_button_->SetIsOn(setting == CONTENT_SETTING_ALLOW);

  remember_setting_->SetChecked(!permission_.is_one_time &&
                                permission_.setting != CONTENT_SETTING_DEFAULT);
  PreferredSizeChanged();
}

void PageInfoPermissionContentView::OnToggleButtonPressed() {
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
      if (CanBeAllowedOnce(permission_.type)) {
        permission_.is_one_time = true;
      }
      break;
    default:
      break;
  }
  PermissionChanged();
}

void PageInfoPermissionContentView::OnRememberSettingPressed() {
  switch (permission_.setting) {
    case CONTENT_SETTING_ALLOW:
      // If one-time permissions are supported, toggle is_one_time.
      // Otherwise, go directly to default.
      if (CanBeAllowedOnce(permission_.type)) {
        permission_.is_one_time = !permission_.is_one_time;
      } else {
        permission_.setting = CONTENT_SETTING_DEFAULT;
      }
      break;
    case CONTENT_SETTING_BLOCK:
      permission_.setting = CONTENT_SETTING_DEFAULT;
      break;
    case CONTENT_SETTING_DEFAULT:
      // When user checks the checkbox to remember the permission setting,
      // it should go to the "allow" state, only if default setting is
      // explicitly allow.
      if (permission_.default_setting == CONTENT_SETTING_ALLOW) {
        permission_.setting = CONTENT_SETTING_ALLOW;
      } else {
        permission_.setting = CONTENT_SETTING_BLOCK;
      }
      break;
    default:
      break;
  }
  PermissionChanged();
}

void PageInfoPermissionContentView::PermissionChanged() {
  presenter_->OnSitePermissionChanged(permission_.type, permission_.setting,
                                      permission_.is_one_time);
}
