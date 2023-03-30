// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"

#include "base/observer_list.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

PermissionToggleRowView::PermissionToggleRowView(
    ChromePageInfoUiDelegate* delegate,
    PageInfoNavigationHandler* navigation_handler,
    const PageInfo::PermissionInfo& permission,
    bool should_show_spacer_view)
    : permission_(permission),
      delegate_(delegate),
      navigation_handler_(navigation_handler) {
  SetUseDefaultFillLayout(true);
  row_view_ = AddChildView(std::make_unique<PageInfoRowView>());
  row_view_->SetTitle(PageInfoUI::PermissionTypeToUIString(permission.type));

  // Add extra details as sublabel.
  std::u16string detail = delegate->GetPermissionDetail(permission.type);
  if (!detail.empty())
    row_view_->AddSecondaryLabel(detail);

  if (permission.source == content_settings::SETTING_SOURCE_USER) {
    // If permission is not allowed because of security reasons, show a label
    // with explanations instead of the controls.
    std::u16string reason =
        delegate->GetAutomaticallyBlockedReason(permission_.type);
    if (!reason.empty()) {
      row_view_->AddControl(std::make_unique<views::Label>(
          delegate->GetAutomaticallyBlockedReason(permission_.type),
          views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    } else {
      InitForUserSource(should_show_spacer_view);
    }
  } else {
    InitForManagedSource(delegate);
  }
  // Set flex rule, defined in `PageInfoRowView`, to wrap the subtitle text but
  // size the parent view to match the content.
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(base::BindRepeating(
                  &PageInfoRowView::FlexRule, base::Unretained(row_view_))));
  UpdateUiOnPermissionChanged();
}

PermissionToggleRowView::~PermissionToggleRowView() = default;

void PermissionToggleRowView::AddObserver(
    PermissionToggleRowViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionToggleRowView::PermissionChanged() {
  UpdateUiOnPermissionChanged();

  for (PermissionToggleRowViewObserver& observer : observer_list_) {
    observer.OnPermissionChanged(permission_);
  }
}

void PermissionToggleRowView::OnToggleButtonPressed() {
  PageInfoUI::ToggleBetweenAllowAndBlock(permission_);
  PermissionChanged();
}

void PermissionToggleRowView::InitForUserSource(bool should_show_spacer_view) {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);

  auto toggle_button = std::make_unique<views::ToggleButton>(
      base::BindRepeating(&PermissionToggleRowView::OnToggleButtonPressed,
                          base::Unretained(this)));
  toggle_button->SetID(
      PageInfoViewFactory::VIEW_ID_PERMISSION_TOGGLE_ROW_TOGGLE_BUTTON);
  toggle_button->SetPreferredSize(
      gfx::Size(toggle_button->GetPreferredSize().width(),
                row_view_->GetFirstLineHeight()));
  toggle_button->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(0, icon_label_spacing));
  toggle_button->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_PAGE_INFO_SELECTOR_TOOLTIP,
      PageInfoUI::PermissionTypeToUIString(permission_.type)));

  toggle_button_ = row_view_->AddControl(std::move(toggle_button));

  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);
  if (permissions::PermissionUtil::CanPermissionBeAllowedOnce(
          permission_.type)) {
    auto subpage_button = views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(
            [=](PermissionToggleRowView* row) {
              row->navigation_handler_->OpenPermissionPage(
                  row->permission_.type);
            },
            base::Unretained(this)),
        vector_icons::kSubmenuArrowIcon);
    subpage_button->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_BUTTON_TOOLTIP));
    views::InstallCircleHighlightPathGenerator(subpage_button.get());
    subpage_button->SetMinimumImageSize({icon_size, icon_size});
    subpage_button->SetFlipCanvasOnPaintForRTLUI(false);
    row_view_->AddControl(std::move(subpage_button));
  } else {
    // If there is a permission that supports one time grants, offset all other
    // permissions to align toggles.
    if (should_show_spacer_view) {
      auto spacer_view = std::make_unique<views::View>();
      spacer_view->SetPreferredSize(gfx::Size(icon_size, icon_size));
      spacer_view_ = row_view_->AddControl(std::move(spacer_view));
    } else {
      toggle_button_->SetProperty(
          views::kMarginsKey, gfx::Insets::TLBR(0, icon_label_spacing, 0, 0));
    }
  }
}

void PermissionToggleRowView::InitForManagedSource(
    ChromePageInfoUiDelegate* delegate) {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto state_label = std::make_unique<views::Label>(
      PageInfoUI::PermissionStateToUIString(delegate, permission_),
      views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY);
  state_label->SetProperty(views::kMarginsKey,
                           gfx::Insets::VH(0, icon_label_spacing));
  row_view_->AddControl(std::move(state_label));

  auto managed_icon = std::make_unique<NonAccessibleImageView>();
  managed_icon->SetImage(
      PageInfoViewFactory::GetManagedPermissionIcon(permission_));
  std::u16string managed_tooltip =
      PageInfoUI::PermissionManagedTooltipToUIString(delegate, permission_);
  managed_icon->SetTooltipText(managed_tooltip);
  row_view_->AddControl(std::move(managed_icon));
}

void PermissionToggleRowView::UpdateUiOnPermissionChanged() {
  // Change the permission icon to reflect the selected setting.
  row_view_->SetIcon(PageInfoViewFactory::GetPermissionIcon(permission_));

  // Update toggle state if it is used.
  if (toggle_button_) {
    toggle_button_->SetIsOn(PageInfoUI::IsToggleOn(permission_));
  }

  // Reset |state_label_|, readd it after if needed.
  if (state_label_) {
    delete state_label_;
    state_label_ = nullptr;
  }

  // Add explanation for the user-managed permission state if needed. This would
  // be shown if permission is in allowed once or default states or if it is
  // automatically blocked.
  if (permission_.source == content_settings::SETTING_SOURCE_USER &&
      (delegate_->ShouldShowAllow(permission_.type) ||
       delegate_->ShouldShowAsk(permission_.type))) {
    std::u16string state_text =
        PageInfoUI::PermissionMainPageStateToUIString(delegate_, permission_);
    if (!state_text.empty()) {
      state_label_ = row_view_->AddSecondaryLabel(state_text);
    }
  }
}

void PermissionToggleRowView::ResetPermission() {
  permission_.setting = CONTENT_SETTING_DEFAULT;
  permission_.is_one_time = false;
  PermissionChanged();
}
