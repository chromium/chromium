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
#include "ui/base/ui_base_features.h"
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
                          features::IsRoundedIconsEnabled()
                              ? vector_icons::kArrowBackIcon
                              : vector_icons::kArrowBackChromeRefreshOldIcon,
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
      icon = show_blocked_badge ? &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kDatabaseOffIcon
                                        : vector_icons::kDatabaseOffOldIcon)
                                : &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kDatabaseIcon
                                        : vector_icons::kDatabaseOldIcon);
      break;
    case ContentSettingsType::FEDERATED_IDENTITY_API:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kAccountCircleOffIcon
                         : vector_icons::kAccountCircleOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled() ? kAccountCircleIcon
                     : features::IsRoundedIconsEnabled()
                         ? vector_icons::kAccountCircleIcon
                         : vector_icons::kAccountCircleChromeRefreshOldIcon);
      break;
    case ContentSettingsType::IMAGES:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kHideImageIcon
                         : vector_icons::kPhotoOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kPhotoIcon
                         : vector_icons::kPhotoChromeRefreshOldIcon);
      break;
    case ContentSettingsType::JAVASCRIPT:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kCodeOffIcon
                         : vector_icons::kCodeOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kCodeIcon
                         : vector_icons::kCodeChromeRefreshOldIcon);
      break;
    case ContentSettingsType::POPUPS:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kOpenInNewOffIcon
                         : vector_icons::kLaunchOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kOpenInNewFlippableIcon
                         : vector_icons::kLaunchChromeRefreshOldIcon);
      break;
    case ContentSettingsType::GEOLOCATION:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kLocationOffIcon
                         : vector_icons::kLocationOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kLocationOnIcon
                         : vector_icons::kLocationOnChromeRefreshOldIcon);
      break;
    case ContentSettingsType::NOTIFICATIONS:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kNotificationsOffIcon
                         : vector_icons::kNotificationsOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kNotificationsIcon
                         : vector_icons::kNotificationsChromeRefreshOldIcon);
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kMicOffIcon
                         : vector_icons::kMicOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kMicIcon
                         : vector_icons::kMicChromeRefreshOldIcon);
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kVideocamOffIcon
                         : vector_icons::kVideocamOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kVideocamIcon
                         : vector_icons::kVideocamChromeRefreshOldIcon);
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kFileDownloadOffIcon
                         : vector_icons::kFileDownloadOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kDownloadIcon
                         : vector_icons::kFileDownloadChromeRefreshOldIcon);
      break;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSyncSavedLocallyOffIcon
                         : vector_icons::kSyncSavedLocallyOffOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSyncSavedLocallyIcon
                         : vector_icons::kSyncSavedLocallyOldIcon);
      break;
#endif
    case ContentSettingsType::MIDI_SYSEX:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kPianoOffIcon
                         : vector_icons::kMidiOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kPianoIcon
                         : vector_icons::kMidiChromeRefreshOldIcon);
      break;
    case ContentSettingsType::BACKGROUND_SYNC:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSyncDisabledIcon
                         : vector_icons::kSyncOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled() ? kSyncIcon
                     : features::IsRoundedIconsEnabled()
                         ? vector_icons::kSyncIcon
                         : vector_icons::kSyncChromeRefreshOldIcon);
      break;
    case ContentSettingsType::ADS:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kAdOffIcon
                         : vector_icons::kAdsOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kAdIcon
                         : vector_icons::kAdsChromeRefreshOldIcon);
      break;
    case ContentSettingsType::SOUND:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kVolumeOffIcon
                         : vector_icons::kVolumeOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kVolumeUpIcon
                         : vector_icons::kVolumeUpChromeRefreshOldIcon);
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      icon = show_blocked_badge ? &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kContentPasteOffIcon
                                        : vector_icons::kContentPasteOffOldIcon)
                                : &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kContentPasteIcon
                                        : vector_icons::kContentPasteOldIcon);
      break;
    case ContentSettingsType::SENSORS:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSensorsOffIcon
                         : vector_icons::kSensorsOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSensorsIcon
                         : vector_icons::kSensorsChromeRefreshOldIcon);
      break;
    case ContentSettingsType::USB_GUARD:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kUsbOffIcon
                         : vector_icons::kUsbOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kUsbIcon
                         : vector_icons::kUsbChromeRefreshOldIcon);
      break;
    case ContentSettingsType::SERIAL_GUARD:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kDeveloperBoardOffIcon
                         : vector_icons::kSerialPortOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kDeveloperBoardIcon
                         : vector_icons::kSerialPortChromeRefreshOldIcon);
      break;
    case ContentSettingsType::BLUETOOTH_GUARD:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kBluetoothDisabledIcon
                         : vector_icons::kBluetoothOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kBluetoothIcon
                         : vector_icons::kBluetoothChromeRefreshOldIcon);
      break;
    case ContentSettingsType::BLUETOOTH_SCANNING:
      icon =
          show_blocked_badge
              ? &(features::IsRoundedIconsEnabled()
                      ? vector_icons::kBluetoothDisabledIcon
                      : vector_icons::kBluetoothOffChromeRefreshOldIcon)
              : &(features::IsRoundedIconsEnabled()
                      ? vector_icons::kBluetoothSearchingIcon
                      : vector_icons::kBluetoothScanningChromeRefreshOldIcon);
      break;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      icon = show_blocked_badge ? &(features::IsRoundedIconsEnabled()
                                        ? kFileSaveOffIcon
                                        : kFileSaveOffChromeRefreshOldIcon)
                                : &(features::IsRoundedIconsEnabled()
                                        ? kFileSaveIcon
                                        : kFileSaveChromeRefreshOldIcon);
      break;
    case ContentSettingsType::VR:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kCardboardOffIcon
                         : vector_icons::kVrHeadsetOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kCardboardIcon
                         : vector_icons::kVrHeadsetChromeRefreshOldIcon);
      break;
    case ContentSettingsType::HAND_TRACKING:
      icon = show_blocked_badge ? &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kHandGestureOffIcon
                                        : vector_icons::kHandGestureOffOldIcon)
                                : &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kHandGestureIcon
                                        : vector_icons::kHandGestureOldIcon);
      break;
    case ContentSettingsType::AR:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kViewInArOffIcon
                         : vector_icons::kViewInArOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kViewInArIcon
                         : vector_icons::kViewInArChromeRefreshOldIcon);
      break;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSelectWindowOffIcon
                         : vector_icons::kSelectWindowOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kSelectWindowIcon
                         : vector_icons::kSelectWindowChromeRefreshOldIcon);
      break;
    case ContentSettingsType::LOCAL_FONTS:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kFontDownloadOffIcon
                         : vector_icons::kFontDownloadOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kFontDownloadIcon
                         : vector_icons::kFontDownloadChromeRefreshOldIcon);
      break;
    case ContentSettingsType::HID_GUARD:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kVideogameAssetOffIcon
                         : vector_icons::kVideogameAssetOffChromeRefreshOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kVideogameAssetIcon
                         : vector_icons::kVideogameAssetChromeRefreshOldIcon);
      break;
    case ContentSettingsType::IDLE_DETECTION:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kDevicesOffIcon
                         : vector_icons::kDevicesOffOldIcon)
                 : &(features::IsRoundedIconsEnabled() ? kDevicesIcon
                     : features::IsRoundedIconsEnabled()
                         ? vector_icons::kDevicesIcon
                         : vector_icons::kDevicesOldIcon);
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kVr180Create2dOffIcon
                         : vector_icons::kStorageAccessOffOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kVr180Create2dIcon
                         : vector_icons::kStorageAccessOldIcon);
      break;
    case ContentSettingsType::KEYBOARD_LOCK:
      icon = show_blocked_badge ? &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kKeyboardLockOffIcon
                                        : vector_icons::kKeyboardLockOffOldIcon)
                                : &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kKeyboardLockIcon
                                        : vector_icons::kKeyboardLockOldIcon);
      break;
    case ContentSettingsType::POINTER_LOCK:
      icon = show_blocked_badge ? &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kMouseLockOffIcon
                                        : vector_icons::kPointerLockOffOldIcon)
                                : &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kMouseLockIcon
                                        : vector_icons::kPointerLockOldIcon);
      break;
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kTouchpadMouseOffIcon
                         : vector_icons::kTouchpadMouseOffOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kTouchpadMouseIcon
                         : vector_icons::kTouchpadMouseOldIcon);
      break;
    case ContentSettingsType::WEB_APP_INSTALLATION:
      icon = show_blocked_badge ? &vector_icons::kInstallDesktopOffCustomIcon
                                : &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kInstallDesktopIcon
                                        : vector_icons::kInstallDesktopOldIcon);
      break;
    case ContentSettingsType::LOCAL_NETWORK:
      icon = show_blocked_badge ? &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kRouterOffIcon
                                        : vector_icons::kRouterOffOldIcon)
                                : &(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kRouterIcon
                                        : vector_icons::kRouterOldIcon);
      break;
    case ContentSettingsType::LOOPBACK_NETWORK:
      icon = show_blocked_badge
                 ? &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kDesktopAccessDisabledIcon
                         : vector_icons::kDesktopAccessDisabledOldIcon)
                 : &(features::IsRoundedIconsEnabled()
                         ? vector_icons::kDesktopWindowsIcon
                         : vector_icons::kDesktopWindowsOldIcon);
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
      icon =
          &(features::IsRoundedIconsEnabled() ? vector_icons::kDatabaseIcon
                                              : vector_icons::kDatabaseOldIcon);
      break;
    case ContentSettingsType::FEDERATED_IDENTITY_API:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kAccountCircleIcon
                   : vector_icons::kAccountCircleOldIcon);
      break;
    case ContentSettingsType::IMAGES:
      icon =
          &(features::IsRoundedIconsEnabled() ? vector_icons::kPhotoFilledIcon
                                              : vector_icons::kPhotoOldIcon);
      break;
    case ContentSettingsType::JAVASCRIPT:
      icon = &(features::IsRoundedIconsEnabled() ? vector_icons::kCodeIcon
                                                 : vector_icons::kCodeOldIcon);
      break;
    case ContentSettingsType::POPUPS:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kOpenInNewFlippableIcon
                   : vector_icons::kLaunchOldIcon);
      break;
    case ContentSettingsType::GEOLOCATION:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kLocationOnIcon
                   : vector_icons::kLocationOnOldIcon);
      break;
    case ContentSettingsType::NOTIFICATIONS:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kNotificationsFilledIcon
                   : vector_icons::kNotificationsOldIcon);
      break;
    case ContentSettingsType::MEDIASTREAM_MIC:
      icon = &(features::IsRoundedIconsEnabled() ? vector_icons::kMicFilledIcon
                                                 : vector_icons::kMicOldIcon);
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
    case ContentSettingsType::CAMERA_PAN_TILT_ZOOM:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kVideocamFilledIcon
                   : vector_icons::kVideocamOldIcon);
      break;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kDownload2FilledIcon
                   : vector_icons::kFileDownloadOldIcon);
      break;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER:
      icon = &vector_icons::kProtectedContentCustomIcon;
      break;
#endif
    case ContentSettingsType::MIDI_SYSEX:
      icon = &(features::IsRoundedIconsEnabled() ? vector_icons::kPianoIcon
                                                 : vector_icons::kMidiOldIcon);
      break;
    case ContentSettingsType::BACKGROUND_SYNC:
      icon = &(features::IsRoundedIconsEnabled() ? vector_icons::kSyncIcon
                                                 : vector_icons::kSyncOldIcon);
      break;
    case ContentSettingsType::ADS:
      icon = &(features::IsRoundedIconsEnabled() ? vector_icons::kAdIcon
                                                 : vector_icons::kAdsOldIcon);
      break;
    case ContentSettingsType::SOUND:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kVolumeUpFilledIcon
                   : vector_icons::kVolumeUpOldIcon);
      break;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kContentPasteIcon
                   : vector_icons::kPageInfoContentPasteOldIcon);
      break;
    case ContentSettingsType::SENSORS:
      icon =
          &(features::IsRoundedIconsEnabled() ? vector_icons::kSensorsIcon
                                              : vector_icons::kSensorsOldIcon);
      break;
    case ContentSettingsType::USB_GUARD:
      icon = &(features::IsRoundedIconsEnabled() ? vector_icons::kUsbIcon
                                                 : vector_icons::kUsbOldIcon);
      break;
    case ContentSettingsType::SERIAL_GUARD:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kDeveloperBoardIcon
                   : vector_icons::kSerialPortOldIcon);
      break;
    case ContentSettingsType::BLUETOOTH_GUARD:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kBluetoothIcon
                   : vector_icons::kBluetoothOldIcon);
      break;
    case ContentSettingsType::BLUETOOTH_SCANNING:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kBluetoothSearchingIcon
                   : vector_icons::kBluetoothScanningOldIcon);
      break;
    case ContentSettingsType::FILE_SYSTEM_WRITE_GUARD:
      icon = &(features::IsRoundedIconsEnabled() ? kFileSaveIcon
                                                 : kFileSaveOldIcon);
      break;
    case ContentSettingsType::VR:
    case ContentSettingsType::AR:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kCardboardFilledIcon
                   : vector_icons::kVrHeadsetOldIcon);
      break;
    case ContentSettingsType::HAND_TRACKING:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kHandGestureIcon
                   : vector_icons::kHandGestureOldIcon);
      break;
    case ContentSettingsType::WINDOW_MANAGEMENT:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kSelectWindowIcon
                   : vector_icons::kSelectWindowOldIcon);
      break;
    case ContentSettingsType::LOCAL_FONTS:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kFontDownloadIcon
                   : vector_icons::kFontDownloadOldIcon);
      break;
    case ContentSettingsType::HID_GUARD:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kVideogameAssetFilledIcon
                   : vector_icons::kVideogameAssetOldIcon);
      break;
    case ContentSettingsType::IDLE_DETECTION:
      icon = &(features::IsRoundedIconsEnabled() ? kDevicesIcon
               : features::IsRoundedIconsEnabled()
                   ? vector_icons::kDevicesIcon
                   : vector_icons::kDevicesOldIcon);
      break;
    case ContentSettingsType::STORAGE_ACCESS:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kVr180Create2dIcon
                   : vector_icons::kStorageAccessOldIcon);
      break;
    case ContentSettingsType::AUTO_PICTURE_IN_PICTURE:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kPictureInPictureIcon
                   : vector_icons::kPictureInPictureOldIcon);
      break;
    case ContentSettingsType::AUTOMATIC_FULLSCREEN:
      icon = &(features::IsRoundedIconsEnabled() ? kFullscreenIcon
                                                 : kFullscreenOldIcon);
      break;
    case ContentSettingsType::CAPTURED_SURFACE_CONTROL:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kTouchpadMouseIcon
                   : vector_icons::kTouchpadMouseOldIcon);
      break;
    case ContentSettingsType::KEYBOARD_LOCK:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kKeyboardLockIcon
                   : vector_icons::kKeyboardLockOldIcon);
      break;
    case ContentSettingsType::POINTER_LOCK:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kMouseLockIcon
                   : vector_icons::kPointerLockOldIcon);
      break;
    case ContentSettingsType::WEB_PRINTING:
      icon =
          &(features::IsRoundedIconsEnabled() ? vector_icons::kPrintIcon
                                              : vector_icons::kPrinterOldIcon);
      break;
    default:
      // All other |ContentSettingsType|s do not have icons on desktop or are
      // not shown in the Page Info bubble.
      NOTREACHED();
  }

  return ui::ImageModel::FromVectorIcon(
      *icon, ui::kColorIcon, GetIconSize(),
      show_blocked_badge ? &vector_icons::kBlockedBadgeCustomIcon : nullptr);
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
      icon = &(features::IsRoundedIconsEnabled() ? vector_icons::kUsbIcon
                                                 : vector_icons::kUsbOldIcon);
      break;
    case ContentSettingsType::SERIAL_CHOOSER_DATA:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kDeveloperBoardIcon
                   : vector_icons::kSerialPortOldIcon);
      break;
    case ContentSettingsType::BLUETOOTH_CHOOSER_DATA:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kBluetoothIcon
                   : vector_icons::kBluetoothOldIcon);
      break;
    case ContentSettingsType::HID_CHOOSER_DATA:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kVideogameAssetFilledIcon
                   : vector_icons::kVideogameAssetOldIcon);
      break;
    case ContentSettingsType::SMART_CARD_DATA:
      icon = &(features::IsRoundedIconsEnabled()
                   ? vector_icons::kSmartCardReaderIcon
                   : vector_icons::kSmartCardReaderOldIcon);
      break;
    default:
      // All other content settings types do not represent chosen object
      // permissions.
      NOTREACHED();
  }

  return ui::ImageModel::FromVectorIcon(
      *icon, ui::kColorIcon, GetIconSize(),
      deleted ? &vector_icons::kBlockedBadgeCustomIcon : nullptr);
}

// static
const ui::ImageModel PageInfoViewFactory::GetSiteSettingsIcon() {
  return GetImageModel(features::IsRoundedIconsEnabled()
                           ? vector_icons::kSettingsIcon
                           : vector_icons::kSettingsChromeRefreshOldIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetLaunchIcon() {
  return ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled()
          ? vector_icons::kOpenInNewFlippableIcon
          : vector_icons::kLaunchChromeRefreshOldIcon,
      ui::kColorIcon, GetIconSize());
}

// static
const ui::ImageModel PageInfoViewFactory::GetConnectionSecureIcon() {
  return GetImageModel(features::IsRoundedIconsEnabled()
                           ? vector_icons::kLockIcon
                           : vector_icons::kHttpsValidOldIcon);
}

// static
const ui::ImageModel PageInfoViewFactory::GetOpenSubpageIcon() {
  // GetIconSize() does not work for subpage icons because the default size of
  // features::IsRoundedIconsEnabled() ? vector_icons::kArrowRightFlippableIcon
  // : kSubmenuArrowOldIcon is 8 rather than 16.
  constexpr int kIconSize = 20;
  return ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled()
          ? vector_icons::kKeyboardArrowRightFlippableIcon
          : vector_icons::kSubmenuArrowChromeRefreshOldIcon,
      ui::kColorIcon, kIconSize);
}

// static
const gfx::VectorIcon& PageInfoViewFactory::GetAboutThisSiteVectorIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kPageInsightsIcon;
#else
  return features::IsRoundedIconsEnabled() ? views::kInfoIcon
                                           : views::kInfoChromeRefreshOldIcon;
#endif  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
const ui::ImageModel PageInfoViewFactory::GetImageModel(
    const gfx::VectorIcon& icon) {
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, GetIconSize());
}
