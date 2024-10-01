// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/permission_toggle_row_view.h"

#include <string>
#include <string_view>

#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/page_info/page_info.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionToggleRowView,
                                      kRowSubTitleCameraElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PermissionToggleRowView,
                                      kRowSubTitleMicrophoneElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(
    PermissionToggleRowView,
    kPermissionDisabledAtSystemLevelElementId);

using content_settings::SettingSource;

PermissionToggleRowView::PermissionToggleRowView(
    ChromePageInfoUiDelegate* delegate,
    PageInfoNavigationHandler* navigation_handler,
    const PageInfo::PermissionInfo& permission,
    bool should_show_spacer_view)
    : permission_(permission),
      delegate_(delegate),
      navigation_handler_(navigation_handler) {
  // TODO(crbug.com/40064612): Directly subclass `RichControlsContainerView`
  // instead of adding it as the only child.
  SetUseDefaultFillLayout(true);
  row_view_ = AddChildView(std::make_unique<RichControlsContainerView>());

  std::u16string toggle_accessible_name =
      PageInfoUI::PermissionTypeToUIString(permission.type);
  row_view_->SetTitle(toggle_accessible_name);

  // Add extra details as sublabel.
  std::u16string detail = delegate->GetPermissionDetail(permission.type);
  if (!detail.empty())
    row_view_->AddSecondaryLabel(detail);

  if (permission.requesting_origin.has_value()) {
    std::u16string requesting_origin_string;
    switch (permission.type) {
      case ContentSettingsType::STORAGE_ACCESS:
        requesting_origin_string =
            url_formatter::FormatOriginForSecurityDisplay(
                *permission.requesting_origin,
                url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    row_view_->AddSecondaryLabel(requesting_origin_string);
    toggle_accessible_name = l10n_util::GetStringFUTF16(
        IDS_CONCAT_TWO_STRINGS_WITH_COMMA, toggle_accessible_name,
        requesting_origin_string);
  }

  int settings_text_id = 0, settings_link_id = 0;
  if (delegate->ShouldShowSettingsLinkForPermission(
          permission.type, &settings_text_id, &settings_link_id)) {
    permission_blocked_on_system_level_ = true;
    std::u16string settings_text_for_link =
        l10n_util::GetStringUTF16(settings_link_id);
    size_t offset;
    blocked_on_system_level_label_ =
        row_view_->AddSecondaryStyledLabel(l10n_util::GetStringFUTF16(
            settings_text_id, settings_text_for_link, &offset));
    base::RepeatingClosure clicked = base::BindRepeating(
        [](PermissionToggleRowView* row, ContentSettingsType type) {
          row->delegate_->SettingsLinkClicked(type);
        },
        base::Unretained(this), permission.type);
    blocked_on_system_level_label_->AddStyleRange(
        gfx::Range(offset, offset + settings_text_for_link.length()),
        views::StyledLabel::RangeStyleInfo::CreateForLink(clicked));

    // Setting the element identifier key to easily get a handle to the label in
    // tests.
    blocked_on_system_level_label_->SetProperty(
        views::kElementIdentifierKey,
        kPermissionDisabledAtSystemLevelElementId);

    // When permission is blocked on the system level, all control elements are
    // disabled. The permission row's title should match color with disabled
    // control elements.
    row_view_->title()->SetEnabledColorId(
        kColorPageInfoPermissionBlockedOnSystemLevelDisabled);
  }

  if (permission.source == SettingSource::kUser) {
    // If permission is not allowed because of security reasons, show a label
    // with explanations instead of the controls.
    std::u16string reason =
        delegate->GetAutomaticallyBlockedReason(permission_.type);
    if (!reason.empty()) {
      views::Label* label =
          row_view_->AddControl(std::make_unique<views::Label>(
              delegate->GetAutomaticallyBlockedReason(permission_.type),
              views::style::CONTEXT_LABEL, views::style::STYLE_BODY_4));
      label->SetEnabledColorId(kColorPageInfoSubtitleForeground);
    } else {
      InitForUserSource(should_show_spacer_view, toggle_accessible_name);
    }
  } else {
    InitForManagedSource(delegate);
  }
  // Set flex rule, defined in `RichControlsContainerView`, to wrap the subtitle
  // text but size the parent view to match the content.
  SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(base::BindRepeating(
          &RichControlsContainerView::FlexRule, base::Unretained(row_view_))));
  UpdateUiOnPermissionChanged();
}

PermissionToggleRowView::~PermissionToggleRowView() = default;

void PermissionToggleRowView::AddObserver(
    PermissionToggleRowViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PermissionToggleRowView::PermissionChanged() {
  UpdateUiOnPermissionChanged();

  observer_list_.Notify(&PermissionToggleRowViewObserver::OnPermissionChanged,
                        permission_);
}

void PermissionToggleRowView::UpdatePermission(
    const PageInfo::PermissionInfo& permission) {
  permission_ = permission;
  UpdateUiOnPermissionChanged();
}

void PermissionToggleRowView::OnToggleButtonPressed() {
  PageInfoUI::ToggleBetweenAllowAndBlock(permission_);
  PermissionChanged();
}

void PermissionToggleRowView::InitForUserSource(
    bool should_show_spacer_view,
    const std::u16string& toggle_accessible_name) {
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
  toggle_button->SetTooltipText(PageInfoUI::PermissionTooltipUiString(
      permission_.type, permission_.requesting_origin));
  toggle_button->GetViewAccessibility().SetName(toggle_accessible_name);

  toggle_button_ = row_view_->AddControl(std::move(toggle_button));

  const int icon_size = GetLayoutConstant(PAGE_INFO_ICON_SIZE);

  if (permission_.is_one_time ||
      permissions::PermissionUtil::DoesSupportTemporaryGrants(
          permission_.type) ||
      (permission_.type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD &&
       base::FeatureList::IsEnabled(
           features::kFileSystemAccessPersistentPermissions))) {
    auto subpage_button = views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(
            [=](PermissionToggleRowView* row) {
              row->navigation_handler_->OpenPermissionPage(
                  row->permission_.type);
            },
            base::Unretained(this)),
        vector_icons::kSubmenuArrowChromeRefreshIcon, icon_size);
    subpage_button->SetTooltipText(
        PageInfoUI::PermissionSubpageButtonTooltipString(permission_.type));
    views::InstallCircleHighlightPathGenerator(subpage_button.get());
    subpage_button->SetMinimumImageSize({icon_size, icon_size});
    subpage_button->SetFlipCanvasOnPaintForRTLUI(false);
    if (permission_blocked_on_system_level_) {
      subpage_button->SetEnabled(false);
    }
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
      views::style::CONTEXT_LABEL, views::style::STYLE_BODY_4);
  state_label->SetEnabledColorId(kColorPageInfoSubtitleForeground);
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
  if (blocked_on_system_level_label_) {
    if (permission_.setting == CONTENT_SETTING_DEFAULT) {
      permission_blocked_on_system_level_ = false;
      blocked_on_system_level_label_->SetVisible(false);
    } else {
      permission_blocked_on_system_level_ = true;
      blocked_on_system_level_label_->SetVisible(true);
    }
  }

  // Change the permission icon to reflect the selected setting.
  row_view_->SetIcon(PageInfoViewFactory::GetPermissionIcon(
      permission_, permission_blocked_on_system_level_));

  // Update toggle state if it is used.
  if (toggle_button_) {
    toggle_button_->SetEnabled(!permission_blocked_on_system_level_);
    toggle_button_->AnimateIsOn(PageInfoUI::IsToggleOn(permission_));
  }

  // Reset |state_label_|, readd it after if needed.
  if (state_label_) {
    delete state_label_;
    state_label_ = nullptr;
  }

  // Add explanation for the user-managed permission state if needed. This would
  // be shown if permission is in allowed once or default states or if it is
  // automatically blocked.
  if (permission_.source == SettingSource::kUser &&
      (delegate_->ShouldShowAllow(permission_.type) ||
       delegate_->ShouldShowAsk(permission_.type))) {
    std::u16string state_text =
        PageInfoUI::PermissionMainPageStateToUIString(delegate_, permission_);
    if (!state_text.empty()) {
      state_label_ = row_view_->AddSecondaryLabel(state_text);
      if (permission_.type == ContentSettingsType::MEDIASTREAM_CAMERA) {
        state_label_->SetProperty(views::kElementIdentifierKey,
                                  kRowSubTitleCameraElementId);
      } else if (permission_.type == ContentSettingsType::MEDIASTREAM_MIC) {
        state_label_->SetProperty(views::kElementIdentifierKey,
                                  kRowSubTitleMicrophoneElementId);
      }
    }
  }
}

void PermissionToggleRowView::ResetPermission() {
  permission_.setting = CONTENT_SETTING_DEFAULT;
  permission_.is_one_time = false;
  permission_.is_in_use = false;
  PermissionChanged();
}

bool PermissionToggleRowView::GetToggleButtonStateForTesting() const {
  return toggle_button_->GetIsOn();
}

BEGIN_METADATA(PermissionToggleRowView)
END_METADATA
