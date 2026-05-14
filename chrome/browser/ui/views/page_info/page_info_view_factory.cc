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
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/page_info/page_info_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/chrome_page_info_ui_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/page_info/page_info_ad_personalization_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_cookies_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/page_info/page_info_merchant_trust_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_navigation_handler.h"
#include "chrome/browser/ui/views/page_info/page_info_permission_content_view.h"
#include "chrome/browser/ui/views/page_info/page_info_security_content_view.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/cookie_controls_state.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/page_info/page_info.h"
#include "components/permissions/permission_util.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"

constexpr int PageInfoViewFactory::kMinBubbleWidth;
constexpr int PageInfoViewFactory::kMaxBubbleWidth;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PageInfoViewFactory,
                                      kBackButtonElementId);

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

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Only the with of |content_| is taken into account, because the header
    // view contains site origin in the subtitle which can be very long.
    const int width = content_->GetPreferredSize(available_size).width();
    return gfx::Size(
        width, GetLayoutManager()->GetPreferredHeightForWidth(this, width));
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  raw_ptr<views::View> content_ = nullptr;
};

int GetIconSize() {
  return GetLayoutConstant(LayoutConstant::kPageInfoIconSize);
}

}  // namespace

// static
std::unique_ptr<views::View> PageInfoViewFactory::CreateSeparator(
    int horizontal_inset) {
  int separator_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_CONTENT_LIST_VERTICAL_MULTI);
  auto separator = std::make_unique<views::Separator>();
  separator->SetProperty(views::kMarginsKey,
                         gfx::Insets::VH(separator_spacing, horizontal_inset));
  return separator;
}

// static
std::unique_ptr<views::View> PageInfoViewFactory::CreateLabelWrapper() {
  // Using the same constant as RichHoverButton so the labels are aligned.
  const int icon_label_spacing = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RICH_HOVER_BUTTON_ICON_HORIZONTAL);
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
    bool allow_extended_site_info)
    : presenter_(presenter),
      ui_delegate_(ui_delegate),
      navigation_handler_(navigation_handler),
      allow_extended_site_info_(allow_extended_site_info) {}

std::unique_ptr<views::View> PageInfoViewFactory::CreatePageView(
    std::u16string title,
    std::unique_ptr<views::View> content_view) {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(title, presenter_->GetSubjectNameForDisplay()),
      std::move(content_view));
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateMainPageView(
    base::OnceClosure initialized_callback) {
  return std::make_unique<PageInfoMainView>(
      presenter_, ui_delegate_, navigation_handler_,
      std::move(initialized_callback), allow_extended_site_info_);
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
    ContentSettingsType type,
    content::WebContents* web_contents) {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(PageInfoUI::PermissionTypeToUIString(type),
                          presenter_->GetSubjectNameForDisplay()),
      std::make_unique<PageInfoPermissionContentView>(presenter_, ui_delegate_,
                                                      type, web_contents));
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
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_HEADER),
          presenter_->GetSubjectNameForDisplay()),
      std::make_unique<PageInfoCookiesContentView>(presenter_));
}

std::unique_ptr<views::View>
PageInfoViewFactory::CreateMerchantTrustPageView() {
  return std::make_unique<PageInfoSubpageView>(
      CreateSubpageHeader(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_MERCHANT_TRUST_HEADER),
          presenter_->GetSubjectNameForDisplay()),
      std::make_unique<PageInfoMerchantTrustContentView>());
}

std::unique_ptr<views::View> PageInfoViewFactory::CreateSubpageHeader(
    std::u16string title,
    std::u16string subtitle) {
  views::Builder<views::FlexLayoutView> label_wrapper;
  label_wrapper.AddChild(views::Builder<views::Label>(
                             std::make_unique<views::Label>(
                                 title, views::style::CONTEXT_DIALOG_TITLE,
                                 views::style::STYLE_HEADLINE_4))
                             .SetEnabledColor(kColorPageInfoForeground)
                             .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                             .SetID(VIEW_ID_PAGE_INFO_SUBPAGE_TITLE));

  if (!subtitle.empty()) {
    label_wrapper.AddChild(
        views::Builder<views::Label>(
            std::make_unique<views::Label>(
                subtitle, views::style::CONTEXT_LABEL,
                views::style::STYLE_BODY_4,
                gfx::DirectionalityMode::DIRECTIONALITY_AS_URL))
            .SetEnabledColor(kColorPageInfoSubtitleForeground)
            .SetHorizontalAlignment(gfx::ALIGN_LEFT)
            .SetAllowCharacterBreak(true)
            .SetMultiLine(true));
  }

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int icon_label_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_LABEL_HORIZONTAL);
  const int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  const int bottom_margin =
      layout_provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_MULTI);

  return views::Builder<views::FlexLayoutView>()
      .SetOrientation(views::LayoutOrientation::kVertical)
      .AddChildren(
          views::Builder<views::FlexLayoutView>()
              .SetCrossAxisAlignment(views::LayoutAlignment::kStart)
              .SetInteriorMargin(
                  gfx::Insets::TLBR(0, side_margin, bottom_margin, side_margin))
              .AddChildren(
                  views::Builder<views::ImageButton>(
                      views::CreateVectorImageButtonWithNativeTheme(
                          base::BindRepeating(
                              &PageInfoNavigationHandler::OpenMainPage,
                              base::Unretained(navigation_handler_),
                              base::DoNothing()),
                          vector_icons::kArrowBackChromeRefreshOldIcon,
                          GetIconSize()))
                      .SetID(VIEW_ID_PAGE_INFO_BACK_BUTTON)
                      .SetProperty(views::kElementIdentifierKey,
                                   kBackButtonElementId)
                      .SetTooltipText(
                          l10n_util::GetStringUTF16(IDS_ACCNAME_BACK))
                      .CustomConfigure(
                          base::BindOnce([](views::ImageButton* button) {
                            views::InstallCircleHighlightPathGenerator(button);
                            button->SetProperty(views::kInternalPaddingKey,
                                                button->GetInsets());
                          })),
                  std::move(label_wrapper)
                      .SetOrientation(views::LayoutOrientation::kVertical)
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::VH(0, icon_label_spacing))
                      .SetProperty(views::kFlexBehaviorKey,
                                   views::FlexSpecification(
                                       views::LayoutOrientation::kHorizontal,
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded)
                                       .WithWeight(1)),
                  views::Builder<views::View>(
                      views::BubbleFrameView::CreateCloseButton(
                          base::BindRepeating(
                              &PageInfoNavigationHandler::CloseBubble,
                              base::Unretained(navigation_handler_))))
                      .SetID(VIEW_ID_PAGE_INFO_CLOSE_BUTTON)
                      .SetVisible(true)
                      .CustomConfigure(base::BindOnce([](views::View* view) {
                        view->SetProperty(views::kInternalPaddingKey,
                                          view->GetInsets());
                      }))),
          views::Builder<views::View>(CreateSeparator()))
      .Build();
}

// static
const ui::ImageModel PageInfoViewFactory::GetPermissionIcon(
    const PageInfo::PermissionInfo& permission,
    bool blocked_on_system_level) {
  PermissionSetting setting =
      permission.setting.value_or(permission.default_setting);

  auto* info = content_settings::PermissionSettingsRegistry::GetInstance()->Get(
      permission.type);
  // For guard content settings and Automatic Picture-in-Picture, ASK is treated
  // as an "on" state.
  const bool show_blocked_badge =
      (!permissions::PermissionUtil::IsGuardContentSetting(permission.type) &&
       permission.type != ContentSettingsType::AUTO_PICTURE_IN_PICTURE)
          ? std::get<ContentSetting>(setting) == CONTENT_SETTING_BLOCK ||
                std::get<ContentSetting>(setting) == CONTENT_SETTING_ASK
          : info->delegate().IsBlocked(setting);

  // TODO(crbug.com/335848275): Migrate the icons in 2 steps.
  // 1 - Copy contents of refresh icons into current non-refresh icons.
  // 2 - In a separate change, remove the refresh icons.

  // Cr2023 does not add an additional blocked badge for block states,
  // instead it uses a completely different icon. This icon usually has the
  // word `Off` in the icon name.
  const gfx::VectorIcon* icon = nullptr;
  switch (permission.type) {
    case ContentSettingsType::COOKIES:
      icon = show_blocked_badge ? &vector_icons::kDatabaseOffOldIcon
                                : &vector_icons::kDatabaseOldIcon;
      break;
    case ContentSettingsType::FEDERATED_IDENTITY_API:
      icon = show_blocked_badge
                 ? &vector_icons::kAccountCircleOffChromeRefreshOldIcon
                 : &vector_icons::kAccountCircleChromeRefreshOldIcon;
      break;
    case ContentSettingsType::IMAGES:
      icon = show_blocked_badge ? &vector_icons::kPhotoOffChromeRefreshOldIcon
                                : &vector_icons::kPhotoChromeRefreshOldIcon;
      break;
    case ContentSettingsType::JAVASCRIPT:
      icon = show_blocked_badge ? &vector_icons::kCodeOffChromeRefreshOldIcon
                                : &vector_icons::kCodeChromeRefreshOldIcon;
      break;
    case ContentSettingsType::POPUPS:
      icon = show_blocked_badge ? &vector_icons::kLaunchOffChromeRefreshOldIcon
                                : &vector_icons::kLaunchChromeRefreshOldIcon;
      break;
    case ContentSettingsType::GEOLOCATION:
      icon = show_blocked_badge
                 ? &vector_icons::kLocationOffChromeRefreshOldIcon
                 : &vector_icons::kLocationOnChromeRefreshOldIcon;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      icon = show_blocked_badge
                 ? &vector_icons::kNotificationsOffChromeRefreshOldIcon
                 : &vector_icons::kNotificationsChromeRefreshOldIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      icon = show_blocked_badge ? &vector_icons::kMicOffChromeRefreshOldIcon
                                : &vector_icons::kMicChromeRefreshOldIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      icon = show_blocked_badge
                 ? &vector_icons::kVideocamOffChromeRefreshOldIcon
                 : &vector_icons::kVideocamChromeRefreshOldIcon;
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      icon = show_blocked_badge
                 ? &vector_icons::kFileDownloadOffChromeRefreshOldIcon
                 : &vector_icons::kFileDownloadChromeRefreshOldIcon;
      break;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      icon = show_blocked_badge ? &vector_icons::kSyncSavedLocallyOffOldIcon
                                : &vector_icons::kSyncSavedLocallyOldIcon;
      break;
#endif
    case ContentSettingsType::MIDI_SYSEX:
      icon = show_blocked_badge ? &vector_icons::kMidiOffChromeRefreshOldIcon
                                : &vector_icons::kMidiChromeRefreshOldIcon;
      break;
    case ContentSettingsType::BACKGROUND_SYNC:
      icon = show_blocked_badge ? &vector_icons::kSyncOffChromeRefreshOldIcon
                                : &vector_icons::kSyncChromeRefreshOldIcon;
      break;
    case ContentSettingsType::ADS:
      icon = show_blocked_badge ? &vector_icons::kAdsOffChromeRefreshOldIcon
                                : &vector_icons::kAdsChromeRefreshOldIcon;
      break;
    case ContentSettingsType::SOUND:
      icon = show_blocked_badge ? &vector_icons::kVolumeOffChromeRefreshOldIcon
                                : &vector_icons::kVolumeUpChromeRefreshOldIcon;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      icon = show_blocked_badge ? &vector_icons::kContentPasteOffOldIcon
                                : &vector_icons::kContentPasteOldIcon;
      break;
    case ContentSettingsType::SENSORS:
      icon = show_blocked_badge ? &vector_icons::kSensorsOffChromeRefreshOldIcon
                                : &vector_icons::kSensorsChromeRefreshOldIcon;
      break;
    case ContentSettingsType::USB_GUARD:
      icon = show_blocked_badge ? &vector_icons::kUsbOffChromeRefreshOldIcon
                                : &vector_icons::kUsbChromeRefreshOldIcon;
      break;
    case ContentSettingsType::SERIAL_GUARD:
      icon = show_blocked_badge
                 ? &vector_icons::kSerialPortOffChromeRefreshOldIcon
                 : &vector_icons::kSerialPortChromeRefreshOldIcon;
      break;
    case ContentSettingsType::BLUETOOTH_GUARD:
      icon = show_blocked_badge
                 ? &vector_icons::kBluetoothOffChromeRefreshOldIcon
                 : &vector_icons::kBluetoothChromeRefreshOldIcon;
      break;
    case ContentSettingsType::BLUETOOTH_SCANNING:
      icon = show_blocked_badge
                 ? &vector_icons::kBluetoothOffChromeRefreshOldIcon
                 : &vector_icons::kBluetoothScanningChromeRefreshOldIcon;
      break;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      icon = show_blocked_badge ? &kFileSaveOffChromeRefreshOldIcon
                                : &kFileSaveChromeRefreshOldIcon;
      break;
    case ContentSettingsType::VR:
      icon = show_blocked_badge
                 ? &vector_icons::kVrHeadsetOffChromeRefreshOldIcon
                 : &vector_icons::kVrHeadsetChromeRefreshOldIcon;
      break;
    case ContentSettingsType::HAND_TRACKING:
      icon = show_blocked_badge ? &vector_icons::kHandGestureOffOldIcon
                                : &vector_icons::kHandGestureOldIcon;
      break;
    case ContentSettingsType::AR:
      icon = show_blocked_badge
                 ? &vector_icons::kViewInArOffChromeRefreshOldIcon
                 : &vector_icons::kViewInArChromeRefreshOldIcon;
      break;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      icon = show_blocked_badge
                 ? &vector_icons::kSelectWindowOffChromeRefreshOldIcon
                 : &vector_icons::kSelectWindowChromeRefreshOldIcon;
      break;
    case ContentSettingsType::LOCAL_FONTS:
      icon = show_blocked_badge
                 ? &vector_icons::kFontDownloadOffChromeRefreshOldIcon
                 : &vector_icons::kFontDownloadChromeRefreshOldIcon;
      break;
    case ContentSettingsType::HID_GUARD:
      icon = show_blocked_badge
                 ? &vector_icons::kVideogameAssetOffChromeRefreshOldIcon
                 : &vector_icons::kVideogameAssetChromeRefreshOldIcon;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      icon = show_blocked_badge ? &vector_icons::kDevicesOffOldIcon
                                : &vector_icons::kDevicesOldIcon;
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      icon = show_blocked_badge ? &vector_icons::kStorageAccessOffOldIcon
                                : &vector_icons::kStorageAccessOldIcon;
      break;
    case ContentSettingsType::KEYBOARD_LOCK:
      icon = show_blocked_badge ? &vector_icons::kKeyboardLockOffOldIcon
                                : &vector_icons::kKeyboardLockOldIcon;
      break;
    case ContentSettingsType::POINTER_LOCK:
      icon = show_blocked_badge ? &vector_icons::kPointerLockOffOldIcon
                                : &vector_icons::kPointerLockOldIcon;
      break;
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      icon = show_blocked_badge ? &vector_icons::kTouchpadMouseOffOldIcon
                                : &vector_icons::kTouchpadMouseOldIcon;
      break;
    case ContentSettingsType::WEB_APP_INSTALLATION:
      icon = show_blocked_badge ? &vector_icons::kInstallDesktopOffIcon
                                : &vector_icons::kInstallDesktopOldIcon;
      break;
    case ContentSettingsType::LOCAL_NETWORK:
      icon = show_blocked_badge ? &vector_icons::kRouterOffOldIcon
                                : &vector_icons::kRouterOldIcon;
      break;
    case ContentSettingsType::LOOPBACK_NETWORK:
      icon = show_blocked_badge ? &vector_icons::kDesktopAccessDisabledOldIcon
                                : &vector_icons::kDesktopWindowsOldIcon;
      break;
    default:
      break;
  }

  // If there is no ChromeRefreshIcon currently defined, continue to the rest
  // of the function.
  if (icon != nullptr) {
    if (blocked_on_system_level) {
      return ui::ImageModel::FromVectorIcon(
          *icon, kColorPageInfoPermissionBlockedOnSystemLevelDisabled,
          GetIconSize());
    }

    if (permission.is_in_use && !show_blocked_badge) {
      return ui::ImageModel::FromVectorIcon(
          *icon, kColorPageInfoPermissionUsedIcon, GetIconSize());
    }
    return ui::ImageModel::FromVectorIcon(*icon, ui::kColorIcon, GetIconSize());
  }

  icon = &gfx::VectorIcon::EmptyIcon();
  switch (permission.type) {
    case ContentSettingsType::COOKIES:
      icon = &vector_icons::kDatabaseOldIcon;
      break;
    case ContentSettingsType::FEDERATED_IDENTITY_API:
      icon = &vector_icons::kAccountCircleOldIcon;
      break;
    case ContentSettingsType::IMAGES:
      icon = &vector_icons::kPhotoOldIcon;
      break;
    case ContentSettingsType::JAVASCRIPT:
      icon = &vector_icons::kCodeOldIcon;
      break;
    case ContentSettingsType::POPUPS:
      icon = &vector_icons::kLaunchOldIcon;
      break;
    case ContentSettingsType::GEOLOCATION:
      icon = &vector_icons::kLocationOnOldIcon;
      break;
    case ContentSettingsType::NOTIFICATIONS:
      icon = &vector_icons::kNotificationsOldIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      icon = &vector_icons::kMicOldIcon;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      icon = &vector_icons::kVideocamOldIcon;
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      icon = &vector_icons::kFileDownloadOldIcon;
      break;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      icon = &vector_icons::kProtectedContentIcon;
      break;
#endif
    case ContentSettingsType::MIDI_SYSEX:
      icon = &vector_icons::kMidiOldIcon;
      break;
    case ContentSettingsType::BACKGROUND_SYNC:
      icon = &vector_icons::kSyncOldIcon;
      break;
    case ContentSettingsType::ADS:
      icon = &vector_icons::kAdsOldIcon;
      break;
    case ContentSettingsType::SOUND:
      icon = &vector_icons::kVolumeUpOldIcon;
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      icon = &vector_icons::kPageInfoContentPasteOldIcon;
      break;
    case ContentSettingsType::SENSORS:
      icon = &vector_icons::kSensorsOldIcon;
      break;
    case ContentSettingsType::USB_GUARD:
      icon = &vector_icons::kUsbOldIcon;
      break;
    case ContentSettingsType::SERIAL_GUARD:
      icon = &vector_icons::kSerialPortOldIcon;
      break;
    case ContentSettingsType::BLUETOOTH_GUARD:
      icon = &vector_icons::kBluetoothOldIcon;
      break;
    case ContentSettingsType::BLUETOOTH_SCANNING:
      icon = &vector_icons::kBluetoothScanningOldIcon;
      break;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      icon = &kFileSaveOldIcon;
      break;
    case ContentSettingsType::VR:
    case ContentSettingsType::AR:
      icon = &vector_icons::kVrHeadsetOldIcon;
      break;
    case ContentSettingsType::HAND_TRACKING:
      icon = &vector_icons::kHandGestureOldIcon;
      break;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      icon = &vector_icons::kSelectWindowOldIcon;
      break;
    case ContentSettingsType::LOCAL_FONTS:
      icon = &vector_icons::kFontDownloadOldIcon;
      break;
    case ContentSettingsType::HID_GUARD:
      icon = &vector_icons::kVideogameAssetOldIcon;
      break;
    case ContentSettingsType::IDLE_DETECTION:
      icon = &vector_icons::kDevicesOldIcon;
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      icon = &vector_icons::kStorageAccessOldIcon;
      break;
    case ContentSettingsType::AUTO_PICTURE_IN_PICTURE:
      icon = &vector_icons::kPictureInPictureOldIcon;
      break;
    case ContentSettingsType::AUTOMATIC_FULLSCREEN:
      icon = &kFullscreenOldIcon;
      break;
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      icon = &vector_icons::kTouchpadMouseOldIcon;
      break;
    case ContentSettingsType::KEYBOARD_LOCK:
      icon = &vector_icons::kKeyboardLockOldIcon;
      break;
    case ContentSettingsType::POINTER_LOCK:
      icon = &vector_icons::kPointerLockOldIcon;
      break;
    case ContentSettingsType::WEB_PRINTING:
      icon = &vector_icons::kPrinterOldIcon;
      break;
    default:
      // All other |ContentSettingsType|s do not have icons on desktop or are
      // not shown in the Page Info bubble.
      NOTREACHED();
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
  // TODO(crbug.com/40672237): Check the connected status of devices and
  // change the icon to one that reflects that status.
  const gfx::VectorIcon* icon = &gfx::VectorIcon::EmptyIcon();
  switch (object.ui_info->content_settings_type) {
    case ContentSettingsType::USB_CHOOSER_DATA:
      icon = &vector_icons::kUsbOldIcon;
      break;
    case ContentSettingsType::SERIAL_CHOOSER_DATA:
      icon = &vector_icons::kSerialPortOldIcon;
      break;
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      icon = &vector_icons::kBluetoothOldIcon;
      break;
    case ContentSettingsType::HID_CHOOSER_DATA:
      icon = &vector_icons::kVideogameAssetOldIcon;
      break;
    case ContentSettingsType::SMART_CARD_DATA:
      icon = &vector_icons::kSmartCardReaderOldIcon;
      break;
    default:
      // All other content settings types do not represent chosen object
      // permissions.
      NOTREACHED();
  }

  return ui::ImageModel::FromVectorIcon(
      *icon, ui::kColorIcon, GetIconSize(),
      deleted ? &vector_icons::kBlockedBadgeIcon : nullptr);
}

// static
const ui::ImageModel PageInfoViewFactory::GetSiteSettingsIcon() {
  return GetImageModel(vector_icons::kSettingsChromeRefreshOldIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetLaunchIcon() {
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kLaunchChromeRefreshOldIcon, ui::kColorIcon, GetIconSize());
}

// static
const ui::ImageModel PageInfoViewFactory::GetConnectionSecureIcon() {
  return GetImageModel(vector_icons::kHttpsValidOldIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetOpenSubpageIcon() {
  // GetIconSize() does not work for subpage icons because the default size of
  // kSubmenuArrowOldIcon is 8 rather than 16.
  constexpr int kIconSize = 20;
  return ui::ImageModel::FromVectorIcon(
      vector_icons::kSubmenuArrowChromeRefreshOldIcon, ui::kColorIcon,
      kIconSize);
}

// static
const gfx::VectorIcon& PageInfoViewFactory::GetAboutThisSiteVectorIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kPageInsightsIcon;
#else
  return views::kInfoChromeRefreshOldIcon;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
const ui::ImageModel PageInfoViewFactory::GetImageModel(
    const gfx::VectorIcon& icon) {
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, GetIconSize());
}
