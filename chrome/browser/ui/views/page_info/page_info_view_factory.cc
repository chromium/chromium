// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_ad_personalization_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/page_info_permission_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/page_info/page_info.h"
#include "components/permissions/permission_util.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/vector_icons.h"

constexpr int PageInfoViewFactory::kMinBubbleWidth;
constexpr int PageInfoViewFactory::kMaxBubbleWidth;

namespace {

class PageInfoSubpageView : public views::View {
 public:
  PageInfoSubpageView(std::unique_ptr<views::View> header,
                      std::unique_ptr<views::View> content) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    AddChildView(std::move(header));
    content_ = AddChildView(std::move(content));
  }

  gfx::Size CalculatePreferredSize() const override {
    // Only the with of |content_| is taken into account, because the header
    // view contains site origin in the subtitle which can be very long.
    const int width = content_->GetPreferredSize().width();
    return gfx::Size(width, GetHeightForWidth(width));
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  raw_ptr<views::View> content_ = nullptr;
};

int GetIconSize() {
  return GetLayoutConstant(PAGE_INFO_ICON_SIZE);
}

}  // namespace

// static
std::unique_ptr<views::View> PageInfoViewFactory::CreateSeparator(
    int horizontal_inset) {
  int separator_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  if (!features::IsChromeRefresh2023()) {
    // Distance for multi content list is used, but split in half, since there
    // is a separator in the middle of it. For ChromeRefresh2023, the separator
    // spacing is larger hence no need to split in half.
    separator_spacing /= 2;
  }
  auto separator = std::make_unique<views::Separator>();
  separator->SetProperty(views::kMarginsKey,
                         gfx::Insets::VH(separator_spacing, horizontal_inset));
  return separator;
}

// static
std::unique_ptr<views::View> PageInfoViewFactory::CreateLabelWrapper() {
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  auto label_wrapper = std::make_unique<views::View>();
  label_wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
  label_wrapper->SetProperty(views::kMarginsKey,
                             gfx::Insets::VH(0, icon_label_spacing));
  label_wrapper->SetProperty(views::kCrossAxisAlignmentKey,
                             views::LayoutAlignment::kStretch);
  label_wrapper->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1));
  return label_wrapper;
}

PageInfoViewFactory::PageInfoViewFactory(
    PageInfo* presenter,
    ChromePageInfoUiDelegate* ui_delegate,
    PageInfoNavigationHandler* navigation_handler,
    PageInfoHistoryController* history_controller)
    : presenter_(presenter),
      ui_delegate_(ui_delegate),
      navigation_handler_(navigation_handler),
      history_controller_(history_controller) {}

std::unique_ptr<views::View> PageInfoViewFactory::CreateMainPageView(
    base::OnceClosure initialized_callback) {
  return std::make_unique<PageInfoMainView>(
      presenter_, ui_delegate_, navigation_handler_, history_controller_,
      std::move(initialized_callback));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateSecurityPageView() {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_SUBPAGE_HEADER),
          presenter_->GetSubjectNameForDisplay()),
      std::make_unique<PageInfoSecurityContentView>(
          presenter_, /*is_standalone_page=*/true));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreatePermissionPageView(
    ContentSettingsType type) {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(PageInfoUI::PermissionTypeToUIString(type),
                          presenter_->GetSubjectNameForDisplay()),
      std::make_unique<PageInfoPermissionContentView>(presenter_, ui_delegate_,
                                                      type));
}

std::unique_ptr<views::View>
PageInfoViewFactory::CreateAdPersonalizationPageView() {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_AD_PRIVACY_HEADER),
          presenter_->GetSubjectNameForDisplay()),
      std::make_unique<PageInfoAdPersonalizationContentView>(presenter_,
                                                             ui_delegate_));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateCookiesPageView() {
  const std::u16string title_label =
      ui_delegate_->IsTrackingProtection3pcdEnabled()
          ? l10n_util::GetStringUTF16(
                IDS_PAGE_INFO_SUB_PAGE_VIEW_TRACKING_PROTECTION_HEADER)
          : l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_HEADER);
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(title_label, presenter_->GetSubjectNameForDisplay()),
      std::make_unique<PageInfoCookiesContentView>(presenter_));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateSubpageHeader(
    std::u16string title,
    std::u16string subtitle) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  views::FlexSpecification stretch_specification =
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width =*/true)
          .WithWeight(1);
  auto wrapper = std::make_unique<views::View>();
  wrapper->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  const int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  const int bottom_margin =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);

  auto* header = wrapper->AddChildView(std::make_unique<views::View>());
  header->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetInteriorMargin(
          gfx::Insets::TLBR(0, side_margin, bottom_margin, side_margin));
  header->SetProperty(views::kFlexBehaviorKey, stretch_specification);
  wrapper->AddChildView(CreateSeparator());

  auto back_button = views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&PageInfoNavigationHandler::OpenMainPage,
                          base::Unretained(navigation_handler_),
                          base::DoNothing()),
      features::IsChromeRefresh2023()
          ? vector_icons::kArrowBackChromeRefreshIcon
          : vector_icons::kArrowBackIcon,
      GetIconSize());
  views::InstallCircleHighlightPathGenerator(back_button.get());
  back_button->SetID(VIEW_ID_PAGE_INFO_BACK_BUTTON);
  back_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_button->SetProperty(views::kInternalPaddingKey,
                           back_button->GetInsets());
  header->AddChildView(std::move(back_button));
  auto* label_wrapper = header->AddChildView(CreateLabelWrapper());
  auto* title_label = label_wrapper->AddChildView(
      std::make_unique<views::Label>(title, views::style::CONTEXT_DIALOG_TITLE,
                                     views::style::STYLE_SECONDARY));
  if (features::IsChromeRefresh2023()) {
    title_label->SetTextStyle(views::style::STYLE_HEADLINE_4);
  }
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetID(VIEW_ID_PAGE_INFO_SUBPAGE_TITLE);

  if (!subtitle.empty()) {
    auto* subtitle_label =
        label_wrapper->AddChildView(std::make_unique<views::Label>(
            subtitle, views::style::CONTEXT_LABEL,
            views::style::STYLE_SECONDARY,
            gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
    subtitle_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    subtitle_label->SetAllowCharacterBreak(true);
    subtitle_label->SetMultiLine(true);
    subtitle_label->SetProperty(views::kFlexBehaviorKey, stretch_specification);
  }

  auto close_button = views::BubbleFrameView::CreateCloseButton(
      base::BindRepeating(&PageInfoNavigationHandler::CloseBubble,
                          base::Unretained(navigation_handler_)));
  close_button->SetID(VIEW_ID_PAGE_INFO_CLOSE_BUTTON);
  close_button->SetVisible(true);
  close_button->SetProperty(views::kInternalPaddingKey,
                            close_button->GetInsets());
  header->AddChildView(std::move(close_button));

  return wrapper;
}

// static
const ui::ImageModel PageInfoViewFactory::GetPermissionIcon(
    const PageInfo::PermissionInfo& info) {
  ContentSetting setting = info.setting == CONTENT_SETTING_DEFAULT
                               ? info.default_setting
                               : info.setting;

  // For guard content settings and Automatic Picture-in-Picture, ASK is treated
  // as an "on" state.
  const bool show_blocked_badge =
      (!permissions::PermissionUtil::IsGuardContentSetting(info.type) &&
       info.type != ContentSettingsType::AUTO_PICTURE_IN_PICTURE)
          ? setting == CONTENT_SETTING_BLOCK || setting == CONTENT_SETTING_ASK
          : setting == CONTENT_SETTING_BLOCK;

  if (features::IsChromeRefresh2023()) {
    // Cr2023 does not add an additional blocked badge for block states,
    // instead it uses a completely different icon. This icon usually has the
    // word `Off` in the icon name.
    const gfx::VectorIcon* icon = nullptr;
    switch (info.type) {
      case ContentSettingsType::COOKIES:
        icon = show_blocked_badge ? &vector_icons::kDatabaseOffIcon
                                  : &vector_icons::kDatabaseIcon;
        break;
      case ContentSettingsType::FEDERATED_IDENTITY_API:
        icon = show_blocked_badge
                   ? &vector_icons::kAccountCircleOffChromeRefreshIcon
                   : &vector_icons::kAccountCircleChromeRefreshIcon;
        break;
      case ContentSettingsType::IMAGES:
        icon = show_blocked_badge ? &vector_icons::kPhotoOffChromeRefreshIcon
                                  : &vector_icons::kPhotoChromeRefreshIcon;
        break;
      case ContentSettingsType::JAVASCRIPT:
        icon = show_blocked_badge ? &vector_icons::kCodeOffChromeRefreshIcon
                                  : &vector_icons::kCodeChromeRefreshIcon;
        break;
      case ContentSettingsType::POPUPS:
        icon = show_blocked_badge ? &vector_icons::kLaunchOffChromeRefreshIcon
                                  : &vector_icons::kLaunchChromeRefreshIcon;
        break;
      case ContentSettingsType::GEOLOCATION:
        icon = show_blocked_badge ? &vector_icons::kLocationOffChromeRefreshIcon
                                  : &vector_icons::kLocationOnChromeRefreshIcon;
        break;
      case ContentSettingsType::NOTIFICATIONS:
        icon = show_blocked_badge
                   ? &vector_icons::kNotificationsOffChromeRefreshIcon
                   : &vector_icons::kNotificationsChromeRefreshIcon;
        break;
      case ContentSettingsType::MEDIASTREAM_MIC:
        icon = show_blocked_badge ? &vector_icons::kMicOffChromeRefreshIcon
                                  : &vector_icons::kMicChromeRefreshIcon;
        break;
      case ContentSettingsType::MEDIASTREAM_CAMERA:
      case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
        icon = show_blocked_badge ? &vector_icons::kVideocamOffChromeRefreshIcon
                                  : &vector_icons::kVideocamChromeRefreshIcon;
        break;
      case ContentSettingsType::AUTOMATIC_DOWNLOADS:
        icon = show_blocked_badge
                   ? &vector_icons::kFileDownloadOffChromeRefreshIcon
                   : &vector_icons::kFileDownloadChromeRefreshIcon;
        break;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
      case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
        icon = show_blocked_badge
                   ? &vector_icons::kCertificateOffChromeRefreshIcon
                   : &vector_icons::kCertificateChromeRefreshIcon;
        break;
#endif
      case ContentSettingsType::MIDI:
      case ContentSettingsType::MIDI_SYSEX:
        icon = show_blocked_badge ? &vector_icons::kMidiOffChromeRefreshIcon
                                  : &vector_icons::kMidiChromeRefreshIcon;
        break;
      case ContentSettingsType::BACKGROUND_SYNC:
        icon = show_blocked_badge ? &vector_icons::kSyncOffChromeRefreshIcon
                                  : &vector_icons::kSyncChromeRefreshIcon;
        break;
      case ContentSettingsType::ADS:
        icon = show_blocked_badge ? &vector_icons::kAdsOffChromeRefreshIcon
                                  : &vector_icons::kAdsChromeRefreshIcon;
        break;
      case ContentSettingsType::SOUND:
        icon = show_blocked_badge ? &vector_icons::kVolumeOffChromeRefreshIcon
                                  : &vector_icons::kVolumeUpChromeRefreshIcon;
        break;
      case ContentSettingsType::CLIPBOARD_READ_WRITE:
        icon = show_blocked_badge
                   ? &vector_icons::kPageInfoContentPasteOffChromeRefreshIcon
                   : &vector_icons::kPageInfoContentPasteChromeRefreshIcon;
        break;
      case ContentSettingsType::SENSORS:
        icon = show_blocked_badge ? &vector_icons::kSensorsOffChromeRefreshIcon
                                  : &vector_icons::kSensorsChromeRefreshIcon;
        break;
      case ContentSettingsType::USB_GUARD:
        icon = show_blocked_badge ? &vector_icons::kUsbOffChromeRefreshIcon
                                  : &vector_icons::kUsbChromeRefreshIcon;
        break;
      case ContentSettingsType::SERIAL_GUARD:
        icon = show_blocked_badge
                   ? &vector_icons::kSerialPortOffChromeRefreshIcon
                   : &vector_icons::kSerialPortChromeRefreshIcon;
        break;
      case ContentSettingsType::BLUETOOTH_GUARD:
        icon = show_blocked_badge
                   ? &vector_icons::kBluetoothOffChromeRefreshIcon
                   : &vector_icons::kBluetoothChromeRefreshIcon;
        break;
      case ContentSettingsType::BLUETOOTH_SCANNING:
        icon = show_blocked_badge
                   ? &vector_icons::kBluetoothOffChromeRefreshIcon
                   : &vector_icons::kBluetoothScanningChromeRefreshIcon;
        break;
      case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
        icon = show_blocked_badge ? &kFileSaveOffChromeRefreshIcon
                                  : &kFileSaveChromeRefreshIcon;
        break;
      case ContentSettingsType::VR:
        icon = show_blocked_badge
                   ? &vector_icons::kVrHeadsetOffChromeRefreshIcon
                   : &vector_icons::kVrHeadsetChromeRefreshIcon;
        break;
      case ContentSettingsType::AR:
        icon = show_blocked_badge ? &vector_icons::kViewInArOffChromeRefreshIcon
                                  : &vector_icons::kViewInArChromeRefreshIcon;
        break;
      case ContentSettingsType::WINDOW_MANAGEMENT:
        icon = show_blocked_badge
                   ? &vector_icons::kSelectWindowOffChromeRefreshIcon
                   : &vector_icons::kSelectWindowChromeRefreshIcon;
        break;
      case ContentSettingsType::LOCAL_FONTS:
        icon = show_blocked_badge
                   ? &vector_icons::kFontDownloadOffChromeRefreshIcon
                   : &vector_icons::kFontDownloadChromeRefreshIcon;
        break;
      case ContentSettingsType::HID_GUARD:
        icon = show_blocked_badge
                   ? &vector_icons::kVideogameAssetOffChromeRefreshIcon
                   : &vector_icons::kVideogameAssetChromeRefreshIcon;
        break;
      case ContentSettingsType::IDLE_DETECTION:
        icon = show_blocked_badge ? &vector_icons::kDevicesOffChromeRefreshIcon
                                  : &vector_icons::kDevicesChromeRefreshIcon;
        break;
      case ContentSettingsType::STORAGE_ACCESS:
        icon = show_blocked_badge ? &vector_icons::kStorageAccessOffIcon
                                  : &vector_icons::kStorageAccessIcon;
        break;
      default:
        break;
    }

    // If there is no ChromeRefreshIcon currently defined, continue to the rest
    // of the function.
    if (icon != nullptr) {
      return ui::ImageModel::FromVectorIcon(*icon, ui::kColorIcon,
                                            GetIconSize());
    }
  }

  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  switch (info.type) {
    case ContentSettingsType::COOKIES:
      icon = &vector_icons::kDatabaseIcon;
      break;
    case ContentSettingsType::FEDERATED_IDENTITY_API:
      icon = &vector_icons::kAccountCircleIcon;
      break;
    case ContentSettingsType::IMAGES:
      icon = &vector_icons::kPhotoIcon;
      break;
    case ContentSettingsType::JAVASCRIPT:
      icon = &vector_icons::kCodeIcon;
      break;
    case ContentSettingsType::POPUPS:
      icon = &vector_icons::kLaunchIcon;
      break;
    case ContentSettingsType::GEOLOCATION:
      icon = &vector_icons::kLocationOnIcon;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      icon = &vector_icons::kNotificationsIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      icon = &vector_icons::kMicIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      icon = &vector_icons::kVideocamIcon;
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      icon = &vector_icons::kFileDownloadIcon;
      break;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      icon = &vector_icons::kProtectedContentIcon;
      break;
#endif
    case ContentSettingsType::MIDI:
    case ContentSettingsType::MIDI_SYSEX:
      icon = &vector_icons::kMidiIcon;
      break;
    case ContentSettingsType::BACKGROUND_SYNC:
      icon = &vector_icons::kSyncIcon;
      break;
    case ContentSettingsType::ADS:
      icon = &vector_icons::kAdsIcon;
      break;
    case ContentSettingsType::SOUND:
      icon = &vector_icons::kVolumeUpIcon;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      icon = &vector_icons::kPageInfoContentPasteIcon;
      break;
    case ContentSettingsType::SENSORS:
      icon = &vector_icons::kSensorsIcon;
      break;
    case ContentSettingsType::USB_GUARD:
      icon = &vector_icons::kUsbIcon;
      break;
    case ContentSettingsType::SERIAL_GUARD:
      icon = &vector_icons::kSerialPortIcon;
      break;
    case ContentSettingsType::BLUETOOTH_GUARD:
      icon = &vector_icons::kBluetoothIcon;
      break;
    case ContentSettingsType::BLUETOOTH_SCANNING:
      icon = &vector_icons::kBluetoothScanningIcon;
      break;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      icon = &kFileSaveIcon;
      break;
    case ContentSettingsType::VR:
    case ContentSettingsType::AR:
      icon = &vector_icons::kVrHeadsetIcon;
      break;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      icon = &vector_icons::kSelectWindowIcon;
      break;
    case ContentSettingsType::LOCAL_FONTS:
      icon = &vector_icons::kFontDownloadIcon;
      break;
    case ContentSettingsType::HID_GUARD:
      icon = &vector_icons::kVideogameAssetIcon;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      icon = &vector_icons::kDevicesIcon;
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      icon = &vector_icons::kStorageAccessIcon;
      break;
    case ContentSettingsType::AUTO_PICTURE_IN_PICTURE:
      icon = &vector_icons::kPictureInPictureIcon;
      break;
    default:
      // All other |ContentSettingsType|s do not have icons on desktop or are
      // not shown in the Page Info bubble.
      NOTREACHED_NORETURN();
  }

  return ui::ImageModel::FromVectorIcon(
      *icon, ui::kColorIcon, GetIconSize(),
      show_blocked_badge ? &vector_icons::kBlockedBadgeIcon : nullptr);
}

// static
const ui::ImageModel PageInfoViewFactory::GetChosenObjectIcon(
    const PageInfoUI::ChosenObjectInfo& object,
    bool deleted) {
  // The permissions data for device APIs will always appear even if the device
  // is not currently conncted to the system.
  // TODO(https://crbug.com/1048860): Check the connected status of devices and
  // change the icon to one that reflects that status.
  const gfx::VectorIcon* icon = &gfx::kNoneIcon;
  switch (object.ui_info->content_settings_type) {
    case ContentSettingsType::USB_CHOOSER_DATA:
      icon = &vector_icons::kUsbIcon;
      break;
    case ContentSettingsType::SERIAL_CHOOSER_DATA:
      icon = &vector_icons::kSerialPortIcon;
      break;
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      icon = &vector_icons::kBluetoothIcon;
      break;
    case ContentSettingsType::HID_CHOOSER_DATA:
      icon = &vector_icons::kVideogameAssetIcon;
      break;
    default:
      // All other content settings types do not represent chosen object
      // permissions.
      NOTREACHED_NORETURN();
  }

  return ui::ImageModel::FromVectorIcon(
      *icon, ui::kColorIcon, GetIconSize(),
      deleted ? &vector_icons::kBlockedBadgeIcon : nullptr);
}

// static
const ui::ImageModel PageInfoViewFactory::GetValidCertificateIcon() {
  return GetImageModel(features::IsChromeRefresh2023()
                           ? vector_icons::kCertificateChromeRefreshIcon
                           : vector_icons::kCertificateIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetInvalidCertificateIcon() {
  if (features::IsChromeRefresh2023()) {
    return ui::ImageModel::FromVectorIcon(
        vector_icons::kCertificateOffChromeRefreshIcon, ui::kColorIcon,
        GetIconSize());
  }
  return ui::ImageModel::FromVectorIcon(vector_icons::kCertificateIcon,
                                        ui::kColorIcon, GetIconSize(),
                                        &vector_icons::kBlockedBadgeIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetSiteSettingsIcon() {
  return GetImageModel(features::IsChromeRefresh2023()
                           ? vector_icons::kSettingsChromeRefreshIcon
                           : vector_icons::kSettingsIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetVrSettingsIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kVrHeadsetIcon,
                                        ui::kColorIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetLaunchIcon() {
  return ui::ImageModel::FromVectorIcon(
      features::IsChromeRefresh2023() ? vector_icons::kLaunchChromeRefreshIcon
                                      : vector_icons::kLaunchIcon,
      features::IsChromeRefresh2023() ? ui::kColorIcon
                                      : ui::kColorIconSecondary,
      GetIconSize());
}

// static
const ui::ImageModel PageInfoViewFactory::GetConnectionNotSecureIcon() {
  return ui::ImageModel::FromVectorIcon(vector_icons::kNotSecureWarningIcon,
                                        ui::kColorAlertHighSeverity,
                                        GetIconSize());
}

// static
const ui::ImageModel PageInfoViewFactory::GetConnectionDangerousIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kDangerousIcon, ui::kColorAlertHighSeverity, GetIconSize());
}

// static
const ui::ImageModel PageInfoViewFactory::GetConnectionSecureIcon() {
  return GetImageModel(features::IsChromeRefresh2023()
                           ? vector_icons::kHttpsValidChromeRefreshIcon
                           : vector_icons::kHttpsValidIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetOpenSubpageIcon() {
  // GetIconSize() does not work for subpage icons because the default size of
  // kSubmenuArrowIcon is 8 rather than 16.
  const int icon_size = features::IsChromeRefresh2023() ? 20 : 8;
  return ui::ImageModel::FromVectorIcon(
      features::IsChromeRefresh2023()
          ? vector_icons::kSubmenuArrowChromeRefreshIcon
          : vector_icons::kSubmenuArrowIcon,
      ui::kColorIcon, icon_size);
}

// static
const ui::ImageModel PageInfoViewFactory::GetAboutThisSiteIcon() {
  return GetImageModel(GetAboutThisSiteVectorIcon());
}

// static
const gfx::VectorIcon& PageInfoViewFactory::GetAboutThisSiteColorVectorIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kPageInsightsColorIcon;
#else
  return features::IsChromeRefresh2023() ? views::kInfoChromeRefreshIcon
                                         : views::kInfoIcon;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
const gfx::VectorIcon& PageInfoViewFactory::GetAboutThisSiteVectorIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kPageInsightsIcon;
#else
  return features::IsChromeRefresh2023() ? views::kInfoChromeRefreshIcon
                                         : views::kInfoIcon;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
// static
const ui::ImageModel PageInfoViewFactory::GetHistoryIcon() {
  return GetImageModel(vector_icons::kHistoryIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetAdPersonalizationIcon() {
  return GetImageModel(vector_icons::kAdsClickIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetManagedPermissionIcon(
    const PageInfo::PermissionInfo& info) {
  const gfx::VectorIcon& managed_vector_icon =
      info.source == content_settings::SETTING_SOURCE_EXTENSION
          ? vector_icons::kExtensionIcon
          : vector_icons::kBusinessIcon;
  return GetImageModel(managed_vector_icon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetThirdPartyCookiesIcon(
    bool third_party_cookies_enabled) {
  if (third_party_cookies_enabled) {
    return GetImageModel(features::IsChromeRefresh2023()
                             ? views::kEyeRefreshIcon
                             : views::kEyeIcon);
  } else {
    return GetBlockingThirdPartyCookiesIcon();
  }
}

// static
const ui::ImageModel PageInfoViewFactory::GetBlockingThirdPartyCookiesIcon() {
  return GetImageModel(features::IsChromeRefresh2023()
                           ? views::kEyeCrossedRefreshIcon
                           : views::kEyeCrossedIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetCookiesAndSiteDataIcon() {
  return GetImageModel(features::IsChromeRefresh2023()
                           ? vector_icons::kCookieChromeRefreshIcon
                           : vector_icons::kCookieIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetFpsIcon() {
  return GetImageModel(vector_icons::kTenancyIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetEnforcedByPolicyIcon() {
  return GetImageModel(vector_icons::kBusinessIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetImageModel(
    const gfx::VectorIcon& icon) {
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, GetIconSize());
}
