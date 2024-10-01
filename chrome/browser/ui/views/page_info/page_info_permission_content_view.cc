// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_permission_content_view.h"

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/accessibility/non_accessible_image_view.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/file_system_access/file_system_access_scroll_panel.h"
#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "components/page_info/page_info.h"
#include "components/permissions/permission_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/media_preview/media_preview_feature.h"
#endif

PageInfoPermissionContentView::PageInfoPermissionContentView(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate,
    ContentSettingsType type,
    content::WebContents* web_contents)
    : presenter_(presenter), type_(type), ui_delegate_(ui_delegate) {
  CHECK(web_contents);
  web_contents_ = web_contents->GetWeakPtr();

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
  title_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  title_->SetEnabledColorId(kColorPageInfoForeground);

  state_label_ = label_wrapper->AddChildView(std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_BODY_4));
  state_label_->SetEnabledColorId(kColorPageInfoSubtitleForeground);
  state_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Add extra details as sublabel.
  std::u16string detail = ui_delegate_->GetPermissionDetail(type);
  if (!detail.empty()) {
    auto detail_label = std::make_unique<views::Label>(
        detail, views::style::CONTEXT_LABEL, views::style::STYLE_BODY_4);
    detail_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    detail_label->SetEnabledColorId(kColorPageInfoSubtitleForeground);
    label_wrapper->AddChildView(std::move(detail_label));
  }

  if (type == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD &&
      base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    std::vector<base::FilePath> granted_file_paths;
    auto* context =
        FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
            web_contents->GetBrowserContext());
    if (context) {
      granted_file_paths = context->GetGrantedPaths(
          url::Origin::Create(web_contents->GetLastCommittedURL()));
    }
    if (!granted_file_paths.empty()) {
      std::unique_ptr<views::ScrollView> scroll_panel =
          FileSystemAccessScrollPanel::Create(granted_file_paths);
      scroll_panel->SetID(
          PageInfoViewFactory::
              VIEW_ID_PAGE_INFO_PERMISSION_SUBPAGE_FILE_SYSTEM_SCROLL_PANEL);
      label_wrapper->AddChildView(std::move(scroll_panel));
    }
  }
  remember_setting_ =
      label_wrapper->AddChildView(std::make_unique<views::Checkbox>(
          l10n_util::GetStringUTF16(
              IDS_PAGE_INFO_PERMISSIONS_SUBPAGE_REMEMBER_THIS_SETTING),
          base::BindRepeating(
              &PageInfoPermissionContentView::OnRememberSettingPressed,
              base::Unretained(this)),
          views::style::CONTEXT_DIALOG_BODY_TEXT));
  remember_setting_->SetID(
      PageInfoViewFactory::
          VIEW_ID_PAGE_INFO_PERMISSION_SUBPAGE_REMEMBER_CHECKBOX);
  remember_setting_->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(controls_spacing, 0, 0, 0));

  const int title_height =
      title_->GetPreferredSize(views::SizeBounds(title_->width(), {})).height();
  toggle_button_ = permission_info_container->AddChildView(
      std::make_unique<views::ToggleButton>(base::BindRepeating(
          &PageInfoPermissionContentView::OnToggleButtonPressed,
          base::Unretained(this))));
  toggle_button_->GetViewAccessibility().SetName(
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

  auto* separator = AddChildView(PageInfoViewFactory::CreateSeparator(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW)));

  MaybeAddMediaPreview(web_contents, *separator);

  // TODO(crbug.com/40775890): Consider to use permission specific text.
  auto* subpage_manage_button = AddChildView(std::make_unique<RichHoverButton>(
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
  subpage_manage_button->SetID(
      PageInfoViewFactory::VIEW_ID_PAGE_INFO_PERMISSION_SUBPAGE_MANAGE_BUTTON);
  subpage_manage_button->title()->SetTextStyle(
      views::style::STYLE_BODY_3_MEDIUM);
  subpage_manage_button->title()->SetEnabledColorId(kColorPageInfoForeground);
  presenter_->InitializeUiState(this, base::DoNothing());
}

PageInfoPermissionContentView::~PageInfoPermissionContentView() {
#if !BUILDFLAG(IS_CHROMEOS)
  if (previews_coordinator_) {
    previews_coordinator_->UpdateDevicePreferenceRanking();
  }
#endif
}

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
    // TODO(crbug.com/325020452): Update label for File System Access to be
    // displayed on this view to meet UX requirements for the Persistent
    // Permissions feature.
    if (type_ != ContentSettingsType::FILE_SYSTEM_WRITE_GUARD ||
        !base::FeatureList::IsEnabled(
            features::kFileSystemAccessPersistentPermissions)) {
      state_label_->SetText(
          PageInfoUI::PermissionStateToUIString(ui_delegate_, permission_));
    }
  }

  bool is_toggle_on = PageInfoUI::IsToggleOn(permission_);
  toggle_button_->SetIsOn(is_toggle_on);

#if !BUILDFLAG(IS_CHROMEOS)
  if (previews_coordinator_) {
    previews_coordinator_->OnPermissionChange(is_toggle_on);
  }
#endif

  if (type_ == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD &&
      base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions)) {
    if (web_contents_.MaybeValid()) {
      auto* context =
          FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
              web_contents_->GetBrowserContext());
      remember_setting_->SetVisible(context && permission_.setting !=
                                                   CONTENT_SETTING_BLOCK);
      remember_setting_->SetChecked(
          context && context->OriginHasExtendedPermission(url::Origin::Create(
                         web_contents_->GetLastCommittedURL())));
    }
  } else {
    remember_setting_->SetChecked(!permission_.is_one_time &&
                                  permission_.setting !=
                                      CONTENT_SETTING_DEFAULT);
    remember_setting_->SetVisible(
        (permissions::PermissionUtil::IsPermission(type_) &&
         permissions::PermissionUtil::DoesSupportTemporaryGrants(
             permission_.type)) &&
        (permission_.setting != CONTENT_SETTING_BLOCK));
  }
  PreferredSizeChanged();
}

void PageInfoPermissionContentView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void PageInfoPermissionContentView::OnToggleButtonPressed() {
  PageInfoUI::ToggleBetweenAllowAndBlock(permission_);

  // One time permissible permissions show a remember me checkbox only for the
  // non-deny state.
  if (permissions::PermissionUtil::DoesSupportTemporaryGrants(
          permission_.type)) {
    PreferredSizeChanged();
  }

  PermissionChanged();
}

void PageInfoPermissionContentView::OnRememberSettingPressed() {
  if (type_ == ContentSettingsType::FILE_SYSTEM_WRITE_GUARD) {
    ToggleFileSystemExtendedPermissions();
  } else {
    PageInfoUI::ToggleBetweenRememberAndForget(permission_);
  }
  PermissionChanged();
}

void PageInfoPermissionContentView::PermissionChanged() {
  presenter_->OnSitePermissionChanged(permission_.type, permission_.setting,
                                      permission_.requesting_origin,
                                      permission_.is_one_time);
}

#if !BUILDFLAG(IS_CHROMEOS)
void PageInfoPermissionContentView::OnAudioDevicesChanged(
    const std::optional<std::vector<media::AudioDeviceDescription>>&
        device_infos) {
  if (type_ == ContentSettingsType::MEDIASTREAM_MIC && device_infos) {
    SetTitleTextAndTooltip(
        IDS_SITE_SETTINGS_TYPE_MIC_WITH_COUNT,
        media_effects::GetRealAudioDeviceNames(device_infos.value()));
  }
}

void PageInfoPermissionContentView::OnVideoDevicesChanged(
    const std::optional<std::vector<media::VideoCaptureDeviceInfo>>&
        device_infos) {
  if (type_ == ContentSettingsType::MEDIASTREAM_CAMERA && device_infos) {
    SetTitleTextAndTooltip(
        IDS_SITE_SETTINGS_TYPE_CAMERA_WITH_COUNT,
        media_effects::GetRealVideoDeviceNames(device_infos.value()));
  } else if (type_ == ContentSettingsType::CAMERA_PAN_TILT_ZOOM &&
             device_infos) {
    SetTitleTextAndTooltip(
        IDS_SITE_SETTINGS_TYPE_CAMERA_PAN_TILT_ZOOM_WITH_COUNT,
        media_effects::GetRealVideoDeviceNames(device_infos.value()));
  }
}

void PageInfoPermissionContentView::SetTitleTextAndTooltip(
    int message_id,
    const std::vector<std::string>& device_names) {
  title_->SetText(l10n_util::GetStringFUTF16(
      message_id, base::NumberToString16(device_names.size())));
  title_->SetTooltipText(
      base::UTF8ToUTF16(base::JoinString(device_names, "\n")));
}
#endif

void PageInfoPermissionContentView::ToggleFileSystemExtendedPermissions() {
  if (!web_contents_.MaybeValid()) {
    return;
  }
  auto* context =
      FileSystemAccessPermissionContextFactory::GetForProfileIfExists(
          web_contents_->GetBrowserContext());
  if (!context) {
    return;
  }
  bool checkbox_enabled = remember_setting_->GetChecked();
  const url::Origin site_origin =
      url::Origin::Create(web_contents_->GetLastCommittedURL());
  bool origin_has_extended_permission =
      context->OriginHasExtendedPermission(site_origin);

  // After pressing the checkbox, the current extended permissions state should
  // not match the checkbox state. In the unlikely scenario that they are the
  // same, do not update permission grants in order to align with the
  // user-visible checkbox state.
  if (origin_has_extended_permission == checkbox_enabled) {
    base::debug::DumpWithoutCrashing();
    return;
  }
  if (checkbox_enabled) {
    context->SetOriginExtendedPermissionByUser(site_origin);
  } else {
    context->RemoveOriginExtendedPermissionByUser(site_origin);
  }
  base::UmaHistogramBoolean(
      "Storage.FileSystemAccess.ToggleExtendedPermissionOutcome",
      checkbox_enabled);
}

void PageInfoPermissionContentView::MaybeAddMediaPreview(
    content::WebContents* web_contents,
    views::View& preceding_separator) {
#if !BUILDFLAG(IS_CHROMEOS)
  if (type_ != ContentSettingsType::MEDIASTREAM_CAMERA &&
      type_ != ContentSettingsType::MEDIASTREAM_MIC &&
      type_ != ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    return;
  }

  const GURL& site_url = web_contents->GetLastCommittedURL();
  if (!media_preview_feature::ShouldShowMediaPreview(
          *web_contents->GetBrowserContext(), site_url, site_url,
          media_preview_metrics::UiLocation::kPageInfo)) {
    return;
  }

  auto* cached_device_info = media_effects::MediaDeviceInfo::GetInstance();
  devices_observer_.Observe(cached_device_info);
  if (type_ == ContentSettingsType::MEDIASTREAM_CAMERA ||
      type_ == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    // Initialize `title_` with the current number of cached video devices.
    OnVideoDevicesChanged(cached_device_info->GetVideoDeviceInfos());
  } else {
    // Initialize `title_` with the current number of cached audio devices.
    OnAudioDevicesChanged(cached_device_info->GetAudioDeviceInfos());
  }

  preceding_separator.GetProperty(views::kMarginsKey)->set_bottom(0);

  previews_coordinator_.emplace(web_contents, type_,
                                /*parent_view=*/this);

  AddChildView(PageInfoViewFactory::CreateSeparator(
                   ChromeLayoutProvider::Get()->GetDistanceMetric(
                       DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW)))
      ->GetProperty(views::kMarginsKey)
      ->set_top(0);
#endif
}

BEGIN_METADATA(PageInfoPermissionContentView)
END_METADATA
