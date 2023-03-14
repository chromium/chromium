// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_permission_content_view.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

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

  // Add extra details as sublabel.
  std::u16string detail = ui_delegate_->GetPermissionDetail(type);
  if (!detail.empty()) {
    auto detail_label = std::make_unique<views::Label>(
        detail, views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
    detail_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label_wrapper->AddChildView(std::move(detail_label));
  }

  remember_setting_ =
      label_wrapper->AddChildView(std::make_unique<views::Checkbox>(
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_REMEMBER_THIS_SETTING),
          base::BindRepeating(
              &PageInfoPermissionContentView::OnRememberSettingPressed,
              base::Unretained(this)),
          views::style::CONTEXT_DIALOG_BODY_TEXT));
  remember_setting_->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(controls_spacing, 0, 0, 0));

  const int title_height = title_->GetPreferredSize().height();
  toggle_button_ = permission_info_container->AddChildView(
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &PageInfoPermissionContentView::OnToggleButtonPressed,
          base::Unretained(this))));
  toggle_button_->SetAccessibleName(
      l10n_util::GetStringFUTF16(IDS_PAGE_INFO_SELECTOR_TOOLTIP,
                                 PageInfoUI::PermissionTypeToUIString(type)));
  toggle_button_->SetPreferredSize(
      gfx::Size(toggle_button_->GetPreferredSize().width(), title_height));

  // Calculate difference between label height and icon size to align icons
  // and label in the first row.
  const int margin =
      (title_height - GetLayoutConstant(PAGE_INFO_ICON_SIZE)) / 2;
  icon_->SetProperty(views::kMarginsKey, gfx::Insets::VH(margin, 0));
  toggle_button_->SetProperty(views::kMarginsKey, gfx::Insets::VH(margin, 0));

  AddChildView(PageInfoViewFactory::CreateSeparator());
  // TODO(crbug.com/1225563): Consider to use permission specific text.
  AddChildView(std::make_unique<RichHoverButton>(
      base::BindRepeating(
          [](PageInfoPermissionContentView* view) {
            view->presenter_->OpenContentSettingsExceptions(view->type_);
          },
          this),
      PageInfoViewFactory::GetSiteSettingsIcon(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_MANAGE_BUTTON),
      std::u16string(),
      l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_MANAGE_BUTTON_TOOLTIP),
      std::u16string(), PageInfoViewFactory::GetLaunchIcon()));

  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoPermissionContentView::~PageInfoPermissionContentView() = default;

void PageInfoPermissionContentView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  auto permission_it = base::ranges::find(permission_info_list, type_,
                                          &PageInfo::PermissionInfo::type);

  CHECK(permission_it != permission_info_list.end());

  permission_ = *permission_it;
  icon_->SetImage(PageInfoViewFactory::GetPermissionIcon(permission_));

  std::u16string auto_blocked_label =
      PageInfoUI::PermissionAutoBlockedToUIString(ui_delegate_, permission_);
  // TODO(olesiamarukhno): For pending request if available show a longer
  // version of auto-block explanation here instead (same as in content
  // settings bubble).
  if (!auto_blocked_label.empty()) {
    state_label_->SetText(auto_blocked_label);
  } else {
    state_label_->SetText(
        PageInfoUI::PermissionStateToUIString(ui_delegate_, permission_));
  }

  toggle_button_->SetIsOn(PageInfoUI::IsToggleOn(permission_));
  remember_setting_->SetChecked(!permission_.is_one_time &&
                                permission_.setting != CONTENT_SETTING_DEFAULT);
  remember_setting_->SetVisible(
      permissions::PermissionUtil::IsPermission(type_) &&
      permissions::PermissionUtil::CanPermissionBeAllowedOnce(
          permission_.type) &&
      (permission_.default_setting != CONTENT_SETTING_BLOCK ||
       permission_.setting != CONTENT_SETTING_DEFAULT));
  PreferredSizeChanged();
}

void PageInfoPermissionContentView::OnToggleButtonPressed() {
  PageInfoUI::ToggleBetweenAllowAndBlock(permission_);
  PermissionChanged();
}

void PageInfoPermissionContentView::OnRememberSettingPressed() {
  PageInfoUI::ToggleBetweenRememberAndForget(permission_);
  PermissionChanged();
}

void PageInfoPermissionContentView::PermissionChanged() {
  presenter_->OnSitePermissionChanged(permission_.type, permission_.setting,
                                      permission_.is_one_time);
}
