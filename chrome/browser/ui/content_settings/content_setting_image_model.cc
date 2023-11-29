// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model_states.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/base/schemeful_site.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#include "services/device/public/cpp/geolocation/location_system_permission_status.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/media/webrtc/system_media_capture_permissions_mac.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#include "services/device/public/cpp/geolocation/geolocation_manager.h"
#endif

using content::WebContents;
using content_settings::PageSpecificContentSettings;

// The image models hierarchy:
//
// ContentSettingImageModel                   - base class
//   ContentSettingSimpleImageModel             - single content setting
//     ContentSettingBlockedImageModel            - generic blocked setting
//     ContentSettingGeolocationImageModel        - geolocation
//     ContentSettingRPHImageModel                - protocol handlers
//     ContentSettingMIDISysExImageModel          - midi sysex
//     ContentSettingDownloadsImageModel          - automatic downloads
//     ContentSettingClipboardReadWriteImageModel - clipboard read and write
//     ContentSettingSensorsImageModel            - sensors
//     ContentSettingNotificationsImageModel      - notifications
//   ContentSettingMediaImageModel              - media
//   ContentSettingFramebustBlockImageModel     - blocked framebust

constexpr bool kNotifyAccessibility = true;

class ContentSettingBlockedImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingBlockedImageModel(ImageType image_type,
                                  ContentSettingsType content_type);

  ContentSettingBlockedImageModel(const ContentSettingBlockedImageModel&) =
      delete;
  ContentSettingBlockedImageModel& operator=(
      const ContentSettingBlockedImageModel&) = delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

class ContentSettingGeolocationImageModel : public ContentSettingImageModel {
 public:
  ContentSettingGeolocationImageModel();

  ContentSettingGeolocationImageModel(
      const ContentSettingGeolocationImageModel&) = delete;
  ContentSettingGeolocationImageModel& operator=(
      const ContentSettingGeolocationImageModel&) = delete;

  ~ContentSettingGeolocationImageModel() override;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;

  bool IsGeolocationAccessed();
  bool IsGeolocationAllowedOnASystemLevel();
  bool IsGeolocationPermissionDetermined();

  std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      WebContents* web_contents) override;
};

class ContentSettingRPHImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingRPHImageModel();

  ContentSettingRPHImageModel(const ContentSettingRPHImageModel&) = delete;
  ContentSettingRPHImageModel& operator=(const ContentSettingRPHImageModel&) =
      delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

class ContentSettingMIDIImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingMIDIImageModel();

  ContentSettingMIDIImageModel(const ContentSettingMIDIImageModel&) = delete;
  ContentSettingMIDIImageModel& operator=(const ContentSettingMIDIImageModel&) =
      delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

class ContentSettingMIDISysExImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingMIDISysExImageModel();

  ContentSettingMIDISysExImageModel(const ContentSettingMIDISysExImageModel&) =
      delete;
  ContentSettingMIDISysExImageModel& operator=(
      const ContentSettingMIDISysExImageModel&) = delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

class ContentSettingDownloadsImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingDownloadsImageModel();

  ContentSettingDownloadsImageModel(const ContentSettingDownloadsImageModel&) =
      delete;
  ContentSettingDownloadsImageModel& operator=(
      const ContentSettingDownloadsImageModel&) = delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

class ContentSettingClipboardReadWriteImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingClipboardReadWriteImageModel();

  ContentSettingClipboardReadWriteImageModel(
      const ContentSettingClipboardReadWriteImageModel&) = delete;
  ContentSettingClipboardReadWriteImageModel& operator=(
      const ContentSettingClipboardReadWriteImageModel&) = delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

// Image model for displaying media icons in the location bar.
class ContentSettingMediaImageModel : public ContentSettingImageModel {
 public:
  ContentSettingMediaImageModel();

  ContentSettingMediaImageModel(const ContentSettingMediaImageModel&) = delete;
  ContentSettingMediaImageModel& operator=(
      const ContentSettingMediaImageModel&) = delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
  bool IsMicAccessed();
  bool IsCamAccessed();
  bool IsMicBlockedOnSiteLevel();
  bool IsCameraBlockedOnSiteLevel();
#if BUILDFLAG(IS_MAC)
  bool DidCameraAccessFailBecauseOfSystemLevelBlock();
  bool DidMicAccessFailBecauseOfSystemLevelBlock();
  bool IsCameraAccessPendingOnSystemLevelPrompt();
  bool IsMicAccessPendingOnSystemLevelPrompt();
#endif  // BUILDFLAG(IS_MAC)

  std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      WebContents* web_contents) override;

 private:
  PageSpecificContentSettings::MicrophoneCameraState state_;
};

class ContentSettingSensorsImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingSensorsImageModel();

  ContentSettingSensorsImageModel(const ContentSettingSensorsImageModel&) =
      delete;
  ContentSettingSensorsImageModel& operator=(
      const ContentSettingSensorsImageModel&) = delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

// The image model for an icon that acts as a quiet permission request prompt
// for notifications. In contrast to other icons -- which are either
// permission-in-use indicators or permission-blocked indicators -- this is
// shown before the user makes the first permission decision, and in fact,
// allows the user to make that decision.
class ContentSettingNotificationsImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingNotificationsImageModel();

  ContentSettingNotificationsImageModel(
      const ContentSettingNotificationsImageModel&) = delete;
  ContentSettingNotificationsImageModel& operator=(
      const ContentSettingNotificationsImageModel&) = delete;

  // ContentSettingSimpleImageModel:
  bool UpdateAndGetVisibility(WebContents* web_contents) override;
  void SetPromoWasShown(content::WebContents* contents) override;
  std::unique_ptr<ContentSettingBubbleModel> CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      WebContents* web_contents) override;
};

class ContentSettingPopupImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingPopupImageModel();

  ContentSettingPopupImageModel(const ContentSettingPopupImageModel&) = delete;
  ContentSettingPopupImageModel& operator=(
      const ContentSettingPopupImageModel&) = delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

class ContentSettingStorageAccessImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingStorageAccessImageModel();

  ContentSettingStorageAccessImageModel(
      const ContentSettingStorageAccessImageModel&) = delete;
  ContentSettingStorageAccessImageModel& operator=(
      const ContentSettingStorageAccessImageModel&) = delete;

  bool UpdateAndGetVisibility(WebContents* web_contents) override;
};

namespace {

struct ContentSettingsImageDetails {
  ContentSettingsType content_type;
  // This field is not a raw_ref<> because it was filtered by the rewriter for:
  // #global-scope
  RAW_PTR_EXCLUSION const gfx::VectorIcon& icon;
  int blocked_tooltip_id;
  int blocked_explanatory_text_id;
  int accessed_tooltip_id;
};

const ContentSettingsImageDetails kImageDetails[] = {
    {ContentSettingsType::COOKIES, vector_icons::kCookieIcon,
     IDS_BLOCKED_ON_DEVICE_SITE_DATA_MESSAGE, 0,
     IDS_ACCESSED_ON_DEVICE_SITE_DATA_MESSAGE},
    {ContentSettingsType::IMAGES, vector_icons::kPhotoIcon,
     IDS_BLOCKED_IMAGES_MESSAGE, 0, 0},
    {ContentSettingsType::JAVASCRIPT, vector_icons::kCodeIcon,
     IDS_BLOCKED_JAVASCRIPT_MESSAGE, 0, 0},
    {ContentSettingsType::MIXEDSCRIPT, kMixedContentIcon,
     IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT, 0, 0},
    {ContentSettingsType::SOUND, kTabAudioIcon, IDS_BLOCKED_SOUND_TITLE, 0, 0},
    {ContentSettingsType::ADS, vector_icons::kAdsIcon,
     IDS_BLOCKED_ADS_PROMPT_TOOLTIP, IDS_BLOCKED_ADS_PROMPT_TITLE, 0},
};

const ContentSettingsImageDetails* GetImageDetails(ContentSettingsType type) {
  for (const ContentSettingsImageDetails& image_details : kImageDetails) {
    if (image_details.content_type == type)
      return &image_details;
  }
  return nullptr;
}

void GetIconChromeRefresh(ContentSettingsType type,
                          bool blocked,
                          raw_ptr<const gfx::VectorIcon>* icon) {
  switch (type) {
    case ContentSettingsType::COOKIES:
      *icon = blocked ? &vector_icons::kDatabaseOffIcon
                      : &vector_icons::kDatabaseIcon;
      return;
    case ContentSettingsType::IMAGES:
      *icon = blocked ? &vector_icons::kPhotoOffChromeRefreshIcon
                      : &vector_icons::kPhotoChromeRefreshIcon;
      return;
    case ContentSettingsType::JAVASCRIPT:
      *icon = blocked ? &vector_icons::kCodeOffChromeRefreshIcon
                      : &vector_icons::kCodeChromeRefreshIcon;
      return;
    case ContentSettingsType::MIXEDSCRIPT:
      *icon = blocked ? &vector_icons::kNotSecureWarningOffChromeRefreshIcon
                      : &vector_icons::kNotSecureWarningChromeRefreshIcon;
      return;
    case ContentSettingsType::SOUND:
      *icon = blocked ? &vector_icons::kVolumeOffChromeRefreshIcon
                      : &vector_icons::kVolumeUpChromeRefreshIcon;
      return;
    case ContentSettingsType::ADS:
      *icon = blocked ? &vector_icons::kAdsOffChromeRefreshIcon
                      : &vector_icons::kAdsChromeRefreshIcon;
      return;
    case ContentSettingsType::GEOLOCATION:
      *icon = blocked ? &vector_icons::kLocationOffChromeRefreshIcon
                      : &vector_icons::kLocationOnChromeRefreshIcon;
      return;
    case ContentSettingsType::PROTOCOL_HANDLERS:
      *icon = blocked ? &vector_icons::kProtocolHandlerOffChromeRefreshIcon
                      : &vector_icons::kProtocolHandlerChromeRefreshIcon;
      return;
    case ContentSettingsType::MIDI:
    case ContentSettingsType::MIDI_SYSEX:
      *icon = blocked ? &vector_icons::kMidiOffChromeRefreshIcon
                      : &vector_icons::kMidiChromeRefreshIcon;
      return;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      *icon = blocked ? &vector_icons::kFileDownloadOffChromeRefreshIcon
                      : &vector_icons::kFileDownloadChromeRefreshIcon;
      return;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      *icon = blocked ? &vector_icons::kContentPasteOffChromeRefreshIcon
                      : &vector_icons::kContentPasteChromeRefreshIcon;
      return;
    case ContentSettingsType::MEDIASTREAM_MIC:
      *icon = blocked ? &vector_icons::kMicOffChromeRefreshIcon
                      : &vector_icons::kMicChromeRefreshIcon;
      return;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      *icon = blocked ? &vector_icons::kVideocamOffChromeRefreshIcon
                      : &vector_icons::kVideocamChromeRefreshIcon;
      return;
    case ContentSettingsType::NOTIFICATIONS:
      *icon = blocked ? &vector_icons::kNotificationsOffChromeRefreshIcon
                      : &vector_icons::kNotificationsChromeRefreshIcon;
      return;
    case ContentSettingsType::SENSORS:
      *icon = blocked ? &vector_icons::kSensorsOffChromeRefreshIcon
                      : &vector_icons::kSensorsChromeRefreshIcon;
      return;
    case ContentSettingsType::STORAGE_ACCESS:
      *icon = blocked ? &vector_icons::kStorageAccessOffIcon
                      : &vector_icons::kStorageAccessIcon;
      return;
    case ContentSettingsType::POPUPS:
      *icon =
          blocked ? &vector_icons::kIframeOffIcon : &vector_icons::kIframeIcon;
      return;
    default:
      NOTREACHED();
      return;
  }
}

// A wrapper function that allows returning both post-chrome-refresh and
// pre-chrome-refresh icons. To minimize code churn, this method returns two
// icons: a base icon and a badge. The badge is painted on top of the base icon,
// which is only needed for pre-chrome-refresh disabled icons.
// |icon| and |badge| are output parameters.
void GetIconFromType(ContentSettingsType type,
                     bool blocked,
                     raw_ptr<const gfx::VectorIcon>* icon,
                     raw_ptr<const gfx::VectorIcon>* badge) {
  if (features::IsChromeRefresh2023()) {
    *badge = &gfx::kNoneIcon;
    GetIconChromeRefresh(type, blocked, icon);
    return;
  }

  *badge = (blocked ? &vector_icons::kBlockedBadgeIcon : &gfx::kNoneIcon);
  switch (type) {
    case ContentSettingsType::COOKIES:
      *icon = &vector_icons::kDatabaseIcon;
      return;
    case ContentSettingsType::IMAGES:
      *icon = &vector_icons::kPhotoIcon;
      return;
    case ContentSettingsType::JAVASCRIPT:
      *icon = &vector_icons::kCodeIcon;
      return;
    case ContentSettingsType::MIXEDSCRIPT:
      *icon = &kMixedContentIcon;
      return;
    case ContentSettingsType::SOUND: {
      bool touch_ui = ui::TouchUiController::Get()->touch_ui();
      *icon = (touch_ui ? &kTabAudioRoundedIcon : &kTabAudioIcon);
      return;
    }
    case ContentSettingsType::ADS:
      *icon = &vector_icons::kAdsIcon;
      return;
    case ContentSettingsType::GEOLOCATION:
      *icon = &vector_icons::kLocationOnIcon;
      return;
    case ContentSettingsType::PROTOCOL_HANDLERS:
      *icon = &vector_icons::kProtocolHandlerIcon;
      return;
    case ContentSettingsType::MIDI:
    case ContentSettingsType::MIDI_SYSEX:
      *icon = &vector_icons::kMidiIcon;
      return;
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      *icon = &vector_icons::kFileDownloadIcon;
      return;
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      *icon = &vector_icons::kContentPasteIcon;
      return;
    case ContentSettingsType::MEDIASTREAM_MIC:
      *icon = &vector_icons::kMicIcon;
      return;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      *icon = &vector_icons::kVideocamIcon;
      return;
    case ContentSettingsType::NOTIFICATIONS:
      *icon = &vector_icons::kNotificationsOffIcon;
      return;
    case ContentSettingsType::SENSORS:
      *icon = &vector_icons::kSensorsIcon;
      return;
    case ContentSettingsType::STORAGE_ACCESS:
      *icon = &vector_icons::kStorageAccessIcon;
      return;
    case ContentSettingsType::POPUPS:
      *icon = &kWebIcon;
      return;
    default:
      NOTREACHED();
      return;
  }
}

std::optional<ContentSettingsType> WhichMidiShouldDisplay(
    PageSpecificContentSettings* content_settings) {
  // MIDI and MIDI-SysEx should be mutually exclusive in their display.
  // The precedence logic:
  // * Always show the allowed permission over the blocked one.
  // * Always show the more dangerous permission (MIDI-SysEx) over the less
  //   dangerous permission (MIDI).
  const bool is_midi_allowed =
      content_settings->IsContentAllowed(ContentSettingsType::MIDI);
  const bool is_midi_blocked =
      content_settings->IsContentBlocked(ContentSettingsType::MIDI);
  const bool is_sysex_allowed =
      content_settings->IsContentAllowed(ContentSettingsType::MIDI_SYSEX);
  const bool is_sysex_blocked =
      content_settings->IsContentBlocked(ContentSettingsType::MIDI_SYSEX);

  if (is_sysex_allowed) {
    return ContentSettingsType::MIDI_SYSEX;
  }

  if (is_midi_allowed) {
    return ContentSettingsType::MIDI;
  }

  if (is_midi_blocked) {
    return ContentSettingsType::MIDI;
  }

  if (is_sysex_blocked) {
    return ContentSettingsType::MIDI_SYSEX;
  }

  return std::nullopt;
}

}  // namespace

// Single content setting ------------------------------------------------------

ContentSettingSimpleImageModel::ContentSettingSimpleImageModel(
    ImageType image_type,
    ContentSettingsType content_type,
    bool image_type_should_notify_accessibility)
    : ContentSettingImageModel(image_type,
                               image_type_should_notify_accessibility),
      content_type_(content_type) {}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingSimpleImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return ContentSettingBubbleModel::CreateContentSettingBubbleModel(
      delegate, web_contents, content_type());
}

// static
std::unique_ptr<ContentSettingImageModel>
ContentSettingImageModel::CreateForContentType(ImageType image_type) {
  switch (image_type) {
    case ImageType::COOKIES:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::COOKIES, ContentSettingsType::COOKIES);
    case ImageType::IMAGES:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::IMAGES, ContentSettingsType::IMAGES);
    case ImageType::JAVASCRIPT:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::JAVASCRIPT, ContentSettingsType::JAVASCRIPT);
    case ImageType::POPUPS:
      return std::make_unique<ContentSettingPopupImageModel>();
    case ImageType::GEOLOCATION:
      return std::make_unique<ContentSettingGeolocationImageModel>();
    case ImageType::MIXEDSCRIPT:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::MIXEDSCRIPT, ContentSettingsType::MIXEDSCRIPT);
    case ImageType::PROTOCOL_HANDLERS:
      return std::make_unique<ContentSettingRPHImageModel>();
    case ImageType::MEDIASTREAM:
      return std::make_unique<ContentSettingMediaImageModel>();
    case ImageType::ADS:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::ADS, ContentSettingsType::ADS);
    case ImageType::AUTOMATIC_DOWNLOADS:
      return std::make_unique<ContentSettingDownloadsImageModel>();
    case ImageType::MIDI_SYSEX:
      return std::make_unique<ContentSettingMIDISysExImageModel>();
    case ImageType::SOUND:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::SOUND, ContentSettingsType::SOUND);
    case ImageType::FRAMEBUST:
      return std::make_unique<ContentSettingFramebustBlockImageModel>();
    case ImageType::CLIPBOARD_READ_WRITE:
      return std::make_unique<ContentSettingClipboardReadWriteImageModel>();
    case ImageType::SENSORS:
      return std::make_unique<ContentSettingSensorsImageModel>();
    case ImageType::NOTIFICATIONS_QUIET_PROMPT:
      return std::make_unique<ContentSettingNotificationsImageModel>();
    case ImageType::STORAGE_ACCESS:
      return std::make_unique<ContentSettingStorageAccessImageModel>();
    case ImageType::MIDI:
      return std::make_unique<ContentSettingMIDIImageModel>();

    case ImageType::NUM_IMAGE_TYPES:
      break;
  }
  NOTREACHED();
  return nullptr;
}

void ContentSettingImageModel::Update(content::WebContents* contents) {
  bool new_visibility = contents ? UpdateAndGetVisibility(contents) : false;
  is_visible_ = new_visibility;
  if (contents && !is_visible_) {
    ContentSettingImageModelStates::Get(contents)->SetAnimationHasRun(
        image_type(), false);
    if (image_type_should_notify_accessibility_) {
      ContentSettingImageModelStates::Get(contents)->SetAccessibilityNotified(
          image_type(), false);
    }
    if (should_auto_open_bubble_) {
      ContentSettingImageModelStates::Get(contents)->SetBubbleWasAutoOpened(
          image_type(), false);
    }
    if (should_show_promo_) {
      ContentSettingImageModelStates::Get(contents)->SetPromoWasShown(
          image_type(), false);
    }
  }
}

bool ContentSettingImageModel::ShouldRunAnimation(
    content::WebContents* contents) {
  DCHECK(contents);
  return !ContentSettingImageModelStates::Get(contents)->AnimationHasRun(
      image_type());
}

void ContentSettingImageModel::SetAnimationHasRun(
    content::WebContents* contents) {
  DCHECK(contents);
  ContentSettingImageModelStates::Get(contents)->SetAnimationHasRun(
      image_type(), true);
}

bool ContentSettingImageModel::ShouldNotifyAccessibility(
    content::WebContents* contents) const {
  return image_type_should_notify_accessibility_ &&
         AccessibilityAnnouncementStringId() &&
         !ContentSettingImageModelStates::Get(contents)
              ->GetAccessibilityNotified(image_type());
}

void ContentSettingImageModel::AccessibilityWasNotified(
    content::WebContents* contents) {
  ContentSettingImageModelStates::Get(contents)->SetAccessibilityNotified(
      image_type(), true);
}

bool ContentSettingImageModel::ShouldShowPromo(content::WebContents* contents) {
  DCHECK(contents);
  return should_show_promo_ &&
         !ContentSettingImageModelStates::Get(contents)->PromoWasShown(
             image_type());
}

void ContentSettingImageModel::SetPromoWasShown(
    content::WebContents* contents) {
  DCHECK(contents);
  ContentSettingImageModelStates::Get(contents)->SetPromoWasShown(image_type(),
                                                                  true);
}

bool ContentSettingImageModel::
    IsMacRestoreLocationPermissionExperimentActive() {
#if BUILDFLAG(IS_MAC)
  return base::FeatureList::IsEnabled(
             features::kLocationPermissionsExperiment) &&
         g_browser_process->local_state()->GetInteger(
             prefs::kMacRestoreLocationPermissionsExperimentCount) <
             (features::GetLocationPermissionsExperimentBubblePromptLimit() +
              features::GetLocationPermissionsExperimentLabelPromptLimit()) &&
         explanatory_string_id() == IDS_GEOLOCATION_TURNED_OFF;
#else
  return false;
#endif
}

bool ContentSettingImageModel::ShouldAutoOpenBubble(
    content::WebContents* contents) {
  return should_auto_open_bubble_ &&
         !ContentSettingImageModelStates::Get(contents)->BubbleWasAutoOpened(
             image_type());
}

void ContentSettingImageModel::SetBubbleWasAutoOpened(
    content::WebContents* contents) {
  // Do nothing if this is part of the Mac restore location permission
  // experiment. In that case we do not want to restrict showing the bubble
  // again.
  if (image_type() == ImageType::GEOLOCATION &&
      IsMacRestoreLocationPermissionExperimentActive()) {
    return;
  }
  ContentSettingImageModelStates::Get(contents)->SetBubbleWasAutoOpened(
      image_type(), true);
}

void ContentSettingImageModel::SetIcon(ContentSettingsType type, bool blocked) {
  GetIconFromType(type, blocked, &icon_, &icon_badge_);
}

void ContentSettingImageModel::SetFramebustBlockedIcon() {
  if (features::IsChromeRefresh2023()) {
    icon_ = &kOpenInNewOffChromeRefreshIcon;
    icon_badge_ = &gfx::kNoneIcon;
  } else {
    icon_ = &kBlockedRedirectIcon;
    icon_badge_ = &vector_icons::kBlockedBadgeIcon;
  }
}

// Generic blocked content settings --------------------------------------------

ContentSettingBlockedImageModel::ContentSettingBlockedImageModel(
    ImageType image_type,
    ContentSettingsType content_type)
    : ContentSettingSimpleImageModel(image_type, content_type) {}

bool ContentSettingBlockedImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  const ContentSettingsType type = content_type();
  const ContentSettingsImageDetails* image_details = GetImageDetails(type);
  DCHECK(image_details) << "No entry for " << static_cast<int32_t>(type)
                        << " in kImageDetails[].";

  int tooltip_id = image_details->blocked_tooltip_id;
  int explanation_id = image_details->blocked_explanatory_text_id;

  // If a content type is blocked by default and was accessed, display the
  // content blocked page action.
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  if (!content_settings)
    return false;

  bool is_blocked = content_settings->IsContentBlocked(type);
  bool is_allowed = content_settings->IsContentAllowed(type);
  if (!is_blocked && !is_allowed)
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);

  // For allowed cookies, don't show the cookie page action unless cookies are
  // blocked by default.
  if (!is_blocked && type == ContentSettingsType::COOKIES &&
      map->GetDefaultContentSetting(type, nullptr) != CONTENT_SETTING_BLOCK) {
    return false;
  }

  // TODO(crbug.com/1054460): Handle first-party blocking with new ui.
  if (type == ContentSettingsType::COOKIES &&
      CookieSettingsFactory::GetForProfile(profile)
          ->ShouldBlockThirdPartyCookies()) {
    return false;
  }

  if (!is_blocked) {
    tooltip_id = image_details->accessed_tooltip_id;
    explanation_id = 0;
  }

  SetIcon(type, content_settings->IsContentBlocked(type));
  set_explanatory_string_id(explanation_id);
  DCHECK(tooltip_id);
  set_tooltip(l10n_util::GetStringUTF16(tooltip_id));
  return true;
}

// Geolocation -----------------------------------------------------------------

ContentSettingGeolocationImageModel::ContentSettingGeolocationImageModel()
    : ContentSettingImageModel(ImageType::GEOLOCATION, kNotifyAccessibility) {}

ContentSettingGeolocationImageModel::~ContentSettingGeolocationImageModel() {}

bool ContentSettingGeolocationImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  set_should_auto_open_bubble(false);
  if (!content_settings) {
    return false;
  }

  bool is_allowed =
      content_settings->IsContentAllowed(ContentSettingsType::GEOLOCATION);
  bool is_blocked =
      content_settings->IsContentBlocked(ContentSettingsType::GEOLOCATION);

  if (!is_allowed && !is_blocked) {
    return false;
  }

  if (is_allowed) {
    if (!IsGeolocationAllowedOnASystemLevel()) {
      set_explanatory_string_id(0);
      SetIcon(ContentSettingsType::GEOLOCATION, /*blocked=*/true);
      base::RecordAction(base::UserMetricsAction(
          "ContentSettings.Geolocation.BlockedIconShown"));
      set_tooltip(l10n_util::GetStringUTF16(IDS_BLOCKED_GEOLOCATION_MESSAGE));
      if (content_settings->geolocation_was_just_granted_on_site_level()) {
#if BUILDFLAG(IS_MAC)
        if (IsGeolocationPermissionDetermined()) {
          // If the system permission is already denied then requesting the
          // system permission will not show a prompt. Show the bubble instead.
          set_should_auto_open_bubble(true);
        } else {
          // Ask the system to display a permission prompt for location access.
          device::GeolocationManager::GetInstance()->RequestSystemPermission();
        }
#else
        set_should_auto_open_bubble(true);
#endif  // BUILDFLAG(IS_MAC)
      }
      // At this point macOS may not have told us whether location permission
      // has been allowed or blocked. Wait until the permission state is
      // determined before displaying this message since it triggers an
      // animation that cannot be cancelled
      if (IsGeolocationPermissionDetermined()) {
#if BUILDFLAG(IS_MAC)
        if (base::FeatureList::IsEnabled(
                features::kLocationPermissionsExperiment)) {
          PrefService* prefs = g_browser_process->local_state();
          int count = prefs->GetInteger(
              prefs::kMacRestoreLocationPermissionsExperimentCount);
          if (count <
              features::GetLocationPermissionsExperimentBubblePromptLimit()) {
            // Show the bubble when the location is denied.
            set_should_auto_open_bubble(true);
            prefs->SetInteger(
                prefs::kMacRestoreLocationPermissionsExperimentCount, ++count);
            prefs->CommitPendingWrite();
          } else if (
              count <
              (features::GetLocationPermissionsExperimentBubblePromptLimit() +
               features::GetLocationPermissionsExperimentLabelPromptLimit())) {
            // Show a persistent label without a bubble when the location is
            // denied.
            set_explanatory_string_id(IDS_GEOLOCATION_TURNED_OFF);
            prefs->SetInteger(
                prefs::kMacRestoreLocationPermissionsExperimentCount, ++count);
            prefs->CommitPendingWrite();
          } else {
            // Return to normal behavior.
            set_explanatory_string_id(IDS_GEOLOCATION_TURNED_OFF);
          }
        } else {
          set_explanatory_string_id(IDS_GEOLOCATION_TURNED_OFF);
        }
#else
        set_explanatory_string_id(IDS_GEOLOCATION_TURNED_OFF);
#endif  // BUILDFLAG(IS_MAC)
      }
      return true;
    }
  }

  SetIcon(ContentSettingsType::GEOLOCATION, /*blocked=*/!is_allowed);
  auto message_id = is_allowed ? IDS_ALLOWED_GEOLOCATION_MESSAGE
                               : IDS_BLOCKED_GEOLOCATION_MESSAGE;
  set_tooltip(l10n_util::GetStringUTF16(message_id));
  set_accessibility_string_id(message_id);

  return true;
}

bool ContentSettingGeolocationImageModel::IsGeolocationAllowedOnASystemLevel() {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  device::GeolocationManager* geolocation_manager =
      device::GeolocationManager::GetInstance();
  CHECK(geolocation_manager);
  device::LocationSystemPermissionStatus permission =
      geolocation_manager->GetSystemPermission();

  return permission == device::LocationSystemPermissionStatus::kAllowed;
#endif
}

bool ContentSettingGeolocationImageModel::IsGeolocationPermissionDetermined() {
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_CHROMEOS)
  return true;
#else

  device::GeolocationManager* geolocation_manager =
      device::GeolocationManager::GetInstance();
  CHECK(geolocation_manager);
  device::LocationSystemPermissionStatus permission =
      geolocation_manager->GetSystemPermission();

  return permission != device::LocationSystemPermissionStatus::kNotDetermined;
#endif
}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingGeolocationImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return std::make_unique<ContentSettingGeolocationBubbleModel>(delegate,
                                                                web_contents);
}

// Protocol handlers -----------------------------------------------------------

ContentSettingRPHImageModel::ContentSettingRPHImageModel()
    : ContentSettingSimpleImageModel(ImageType::PROTOCOL_HANDLERS,
                                     ContentSettingsType::PROTOCOL_HANDLERS) {
  SetIcon(ContentSettingsType::PROTOCOL_HANDLERS, /*blocked=*/false);
  set_tooltip(l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_TOOLTIP));
}

bool ContentSettingRPHImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  auto* content_settings_delegate =
      chrome::PageSpecificContentSettingsDelegate::FromWebContents(
          web_contents);
  if (!content_settings_delegate)
    return false;
  if (content_settings_delegate->pending_protocol_handler().IsEmpty())
    return false;

  return true;
}

// MIDI ------------------------------------------------------------------------

ContentSettingMIDIImageModel::ContentSettingMIDIImageModel()
    : ContentSettingSimpleImageModel(ImageType::MIDI,
                                     ContentSettingsType::MIDI) {}

bool ContentSettingMIDIImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(features::kBlockMidiByDefault)) {
    return false;
  }

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  if (!content_settings) {
    return false;
  }

  if (WhichMidiShouldDisplay(content_settings) != ContentSettingsType::MIDI) {
    return false;
  }

  const bool is_allowed =
      content_settings->IsContentAllowed(ContentSettingsType::MIDI);
  SetIcon(ContentSettingsType::MIDI, /*blocked=*/!is_allowed);
  set_tooltip(l10n_util::GetStringUTF16(is_allowed ? IDS_ALLOWED_MIDI_MESSAGE
                                                   : IDS_BLOCKED_MIDI_MESSAGE));
  return true;
}

// MIDI SysEx ------------------------------------------------------------------

ContentSettingMIDISysExImageModel::ContentSettingMIDISysExImageModel()
    : ContentSettingSimpleImageModel(ImageType::MIDI_SYSEX,
                                     ContentSettingsType::MIDI_SYSEX) {}

bool ContentSettingMIDISysExImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  if (!content_settings)
    return false;

  const bool is_allowed =
      content_settings->IsContentAllowed(ContentSettingsType::MIDI_SYSEX);
  const bool is_blocked =
      content_settings->IsContentBlocked(ContentSettingsType::MIDI_SYSEX);

  if (base::FeatureList::IsEnabled(features::kBlockMidiByDefault)) {
    if (WhichMidiShouldDisplay(content_settings) !=
        ContentSettingsType::MIDI_SYSEX) {
      return false;
    }
  } else {
    if (!is_allowed && !is_blocked) {
      return false;
    }
  }

  SetIcon(ContentSettingsType::MIDI_SYSEX, /*blocked=*/!is_allowed);
  set_tooltip(l10n_util::GetStringUTF16(is_allowed
                                            ? IDS_ALLOWED_MIDI_SYSEX_MESSAGE
                                            : IDS_BLOCKED_MIDI_SYSEX_MESSAGE));
  return true;
}

// Automatic downloads ---------------------------------------------------------

ContentSettingDownloadsImageModel::ContentSettingDownloadsImageModel()
    : ContentSettingSimpleImageModel(ImageType::AUTOMATIC_DOWNLOADS,
                                     ContentSettingsType::AUTOMATIC_DOWNLOADS) {
}

bool ContentSettingDownloadsImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  DownloadRequestLimiter* download_request_limiter =
      g_browser_process->download_request_limiter();

  // DownloadRequestLimiter can be absent in unit_tests.
  if (!download_request_limiter)
    return false;

  switch (download_request_limiter->GetDownloadUiStatus(web_contents)) {
    case DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED:
      SetIcon(ContentSettingsType::AUTOMATIC_DOWNLOADS, /*blocked=*/false);
      set_explanatory_string_id(0);
      set_tooltip(l10n_util::GetStringUTF16(IDS_ALLOWED_DOWNLOAD_TITLE));
      return true;
    case DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED:
      SetIcon(ContentSettingsType::AUTOMATIC_DOWNLOADS, /*blocked=*/true);
      set_explanatory_string_id(IDS_BLOCKED_DOWNLOADS_EXPLANATION);
      set_tooltip(l10n_util::GetStringUTF16(IDS_BLOCKED_DOWNLOAD_TITLE));
      return true;
    case DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT:
      // No need to show icon otherwise.
      return false;
  }
}

// Clipboard -------------------------------------------------------------------

ContentSettingClipboardReadWriteImageModel::
    ContentSettingClipboardReadWriteImageModel()
    : ContentSettingSimpleImageModel(
          ImageType::CLIPBOARD_READ_WRITE,
          ContentSettingsType::CLIPBOARD_READ_WRITE) {}

bool ContentSettingClipboardReadWriteImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  if (!content_settings)
    return false;
  ContentSettingsType content_type = ContentSettingsType::CLIPBOARD_READ_WRITE;
  bool blocked = content_settings->IsContentBlocked(content_type);
  bool allowed = content_settings->IsContentAllowed(content_type);
  if (!blocked && !allowed)
    return false;

  SetIcon(ContentSettingsType::CLIPBOARD_READ_WRITE, /*blocked=*/!allowed);
  set_tooltip(l10n_util::GetStringUTF16(
      allowed ? IDS_ALLOWED_CLIPBOARD_MESSAGE : IDS_BLOCKED_CLIPBOARD_MESSAGE));
  return true;
}

// Media -----------------------------------------------------------------------

ContentSettingMediaImageModel::ContentSettingMediaImageModel()
    : ContentSettingImageModel(ImageType::MEDIASTREAM, kNotifyAccessibility) {}

bool ContentSettingMediaImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  set_should_auto_open_bubble(false);
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  if (!content_settings)
    return false;
  state_ = content_settings->GetMicrophoneCameraState();

  // If neither the microphone nor the camera stream was accessed then no icon
  // is displayed in the omnibox.
  if (state_.Empty()) {
    return false;
  }

#if BUILDFLAG(IS_MAC)
  // Don't show an icon when the user has not made a decision yet for
  // the site level media permissions.
  if (IsCameraAccessPendingOnSystemLevelPrompt() ||
      IsMicAccessPendingOnSystemLevelPrompt()) {
    return false;
  }

  set_explanatory_string_id(0);

  if (IsCamAccessed() && IsMicAccessed()) {
    if (IsCameraBlockedOnSiteLevel() || IsMicBlockedOnSiteLevel()) {
      SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/true);
      set_tooltip(l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_BLOCKED));
      set_accessibility_string_id(IDS_MICROPHONE_CAMERA_BLOCKED);
    } else if (DidCameraAccessFailBecauseOfSystemLevelBlock() ||
               DidMicAccessFailBecauseOfSystemLevelBlock()) {
      SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/true);
      set_tooltip(l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_BLOCKED));
      set_accessibility_string_id(IDS_MICROPHONE_CAMERA_BLOCKED);
      if (content_settings->camera_was_just_granted_on_site_level() ||
          content_settings->mic_was_just_granted_on_site_level()) {
        // Automatically trigger the new bubble, if the camera
        // and/or mic was just granted on a site level, but blocked on a
        // system level.
        set_should_auto_open_bubble(true);
      } else {
        set_explanatory_string_id(IDS_CAMERA_TURNED_OFF);
      }
    } else {
      SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/false);
      set_tooltip(l10n_util::GetStringUTF16(IDS_MICROPHONE_CAMERA_ALLOWED));
    }
    return true;
  }

  if (IsCamAccessed()) {
    if (IsCameraBlockedOnSiteLevel()) {
      SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/true);
      set_tooltip(l10n_util::GetStringUTF16(IDS_CAMERA_BLOCKED));
      set_accessibility_string_id(IDS_CAMERA_BLOCKED);
    } else if (DidCameraAccessFailBecauseOfSystemLevelBlock()) {
      SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/true);
      set_tooltip(l10n_util::GetStringUTF16(IDS_CAMERA_BLOCKED));
      set_accessibility_string_id(IDS_CAMERA_BLOCKED);
      if (content_settings->camera_was_just_granted_on_site_level()) {
        set_should_auto_open_bubble(true);
      } else {
        set_explanatory_string_id(IDS_CAMERA_TURNED_OFF);
      }
    } else {
      SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/false);
      set_tooltip(l10n_util::GetStringUTF16(IDS_CAMERA_ACCESSED));
      set_accessibility_string_id(IDS_CAMERA_ACCESSED);
    }
    return true;
  }

  if (IsMicAccessed()) {
    if (IsMicBlockedOnSiteLevel()) {
      SetIcon(ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/true);
      set_tooltip(l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED));
      set_accessibility_string_id(IDS_MICROPHONE_BLOCKED);
    } else if (DidMicAccessFailBecauseOfSystemLevelBlock()) {
      SetIcon(ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/true);
      set_tooltip(l10n_util::GetStringUTF16(IDS_MICROPHONE_BLOCKED));
      set_accessibility_string_id(IDS_MICROPHONE_BLOCKED);
      if (content_settings->mic_was_just_granted_on_site_level()) {
        set_should_auto_open_bubble(true);
      } else {
        set_explanatory_string_id(IDS_MIC_TURNED_OFF);
      }
    } else {
      SetIcon(ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/false);
      set_tooltip(l10n_util::GetStringUTF16(IDS_MICROPHONE_ACCESSED));
      set_accessibility_string_id(IDS_MICROPHONE_ACCESSED);
    }
    return true;
  }
#endif  // BUILDFLAG(IS_MAC)

  DCHECK(IsMicAccessed() || IsCamAccessed());

  int id = IDS_CAMERA_BLOCKED;
  if (IsMicBlockedOnSiteLevel() || IsCameraBlockedOnSiteLevel()) {
    if (IsMicAccessed())
      id = IsCamAccessed() ? IDS_MICROPHONE_CAMERA_BLOCKED
                           : IDS_MICROPHONE_BLOCKED;

    if (IsCamAccessed()) {
      SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/true);
    } else {
      SetIcon(ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/true);
    }

  } else {
    SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/false);
    id = IDS_CAMERA_ACCESSED;
    if (IsMicAccessed())
      id = IsCamAccessed() ? IDS_MICROPHONE_CAMERA_ALLOWED
                           : IDS_MICROPHONE_ACCESSED;

    if (IsCamAccessed()) {
      SetIcon(ContentSettingsType::MEDIASTREAM_CAMERA, /*blocked=*/false);
    } else {
      SetIcon(ContentSettingsType::MEDIASTREAM_MIC, /*blocked=*/false);
    }
  }
  set_tooltip(l10n_util::GetStringUTF16(id));
  set_accessibility_string_id(id);

  return true;
}

bool ContentSettingMediaImageModel::IsMicAccessed() {
  return state_.Has(PageSpecificContentSettings::kMicrophoneAccessed);
}

bool ContentSettingMediaImageModel::IsCamAccessed() {
  return state_.Has(PageSpecificContentSettings::kCameraAccessed);
}

bool ContentSettingMediaImageModel::IsMicBlockedOnSiteLevel() {
  return state_.Has(PageSpecificContentSettings::kMicrophoneBlocked);
}

bool ContentSettingMediaImageModel::IsCameraBlockedOnSiteLevel() {
  return state_.Has(PageSpecificContentSettings::kCameraBlocked);
}

#if BUILDFLAG(IS_MAC)
bool ContentSettingMediaImageModel::
    DidCameraAccessFailBecauseOfSystemLevelBlock() {
  return (IsCamAccessed() && !IsCameraBlockedOnSiteLevel() &&
          system_media_permissions::CheckSystemVideoCapturePermission() ==
              system_media_permissions::SystemPermission::kDenied);
}

bool ContentSettingMediaImageModel::
    DidMicAccessFailBecauseOfSystemLevelBlock() {
  return (IsMicAccessed() && !IsMicBlockedOnSiteLevel() &&
          system_media_permissions::CheckSystemAudioCapturePermission() ==
              system_media_permissions::SystemPermission::kDenied);
}

bool ContentSettingMediaImageModel::IsCameraAccessPendingOnSystemLevelPrompt() {
  return (system_media_permissions::CheckSystemVideoCapturePermission() ==
              system_media_permissions::SystemPermission::kNotDetermined &&
          IsCamAccessed() && !IsCameraBlockedOnSiteLevel());
}

bool ContentSettingMediaImageModel::IsMicAccessPendingOnSystemLevelPrompt() {
  return (system_media_permissions::CheckSystemAudioCapturePermission() ==
              system_media_permissions::SystemPermission::kNotDetermined &&
          IsMicAccessed() && !IsMicBlockedOnSiteLevel());
}

#endif  // BUILDFLAG(IS_MAC)

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingMediaImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return std::make_unique<ContentSettingMediaStreamBubbleModel>(delegate,
                                                                web_contents);
}

// Blocked Framebust -----------------------------------------------------------
ContentSettingFramebustBlockImageModel::ContentSettingFramebustBlockImageModel()
    : ContentSettingImageModel(ImageType::FRAMEBUST) {}

bool ContentSettingFramebustBlockImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  // Early exit if no blocked Framebust.
  if (!FramebustBlockTabHelper::FromWebContents(web_contents)->HasBlockedUrls())
    return false;

  SetFramebustBlockedIcon();
  set_explanatory_string_id(IDS_REDIRECT_BLOCKED_TITLE);
  set_tooltip(l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_TOOLTIP));
  return true;
}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingFramebustBlockImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return std::make_unique<ContentSettingFramebustBlockBubbleModel>(
      delegate, web_contents);
}

// Sensors ---------------------------------------------------------------------

ContentSettingSensorsImageModel::ContentSettingSensorsImageModel()
    : ContentSettingSimpleImageModel(ImageType::SENSORS,
                                     ContentSettingsType::SENSORS) {}

bool ContentSettingSensorsImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  auto* content_settings = PageSpecificContentSettings::GetForFrame(
      web_contents->GetPrimaryMainFrame());
  if (!content_settings)
    return false;

  bool blocked = content_settings->IsContentBlocked(content_type());
  bool allowed = content_settings->IsContentAllowed(content_type());

  if (!blocked && !allowed)
    return false;

  HostContentSettingsMap* map = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  // Do not show any indicator if sensors are allowed by default and they were
  // not blocked in this page.
  if (!blocked && map->GetDefaultContentSetting(content_type(), nullptr) ==
                      CONTENT_SETTING_ALLOW) {
    return false;
  }

  SetIcon(ContentSettingsType::SENSORS, /*blocked=*/blocked);
  if (base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)) {
    set_tooltip(l10n_util::GetStringUTF16(
        !blocked ? IDS_SENSORS_ALLOWED_TOOLTIP : IDS_SENSORS_BLOCKED_TOOLTIP));
  } else {
    set_tooltip(l10n_util::GetStringUTF16(
        !blocked ? IDS_MOTION_SENSORS_ALLOWED_TOOLTIP
                 : IDS_MOTION_SENSORS_BLOCKED_TOOLTIP));
  }
  return true;
}

// Popups ---------------------------------------------------------------------

ContentSettingPopupImageModel::ContentSettingPopupImageModel()
    : ContentSettingSimpleImageModel(ImageType::POPUPS,
                                     ContentSettingsType::POPUPS) {}

bool ContentSettingPopupImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  if (!content_settings || !content_settings->IsContentBlocked(content_type()))
    return false;
  SetIcon(ContentSettingsType::POPUPS, /*blocked=*/true);
  set_explanatory_string_id(IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT);
  set_tooltip(l10n_util::GetStringUTF16(IDS_BLOCKED_POPUPS_TOOLTIP));
  return true;
}

// Storage Access
// ---------------------------------------------------------------------

ContentSettingStorageAccessImageModel::ContentSettingStorageAccessImageModel()
    : ContentSettingSimpleImageModel(ImageType::STORAGE_ACCESS,
                                     ContentSettingsType::STORAGE_ACCESS) {}

bool ContentSettingStorageAccessImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents->GetPrimaryMainFrame());
  if (!content_settings) {
    return false;
  }
  std::map<net::SchemefulSite, bool> entries =
      content_settings->GetTwoSiteRequests(content_type());
  if (entries.empty()) {
    return false;
  }
  bool has_blocked_requests =
      base::ranges::any_of(entries, [](auto& entry) { return !entry.second; });

  SetIcon(ContentSettingsType::STORAGE_ACCESS,
          /*blocked=*/has_blocked_requests);
  // set_explanatory_string_id(IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT);
  set_tooltip(l10n_util::GetStringUTF16(
      has_blocked_requests ? IDS_STORAGE_ACCESS_PERMISSION_BLOCKED_TOOLTIP
                           : IDS_STORAGE_ACCESS_PERMISSION_ALLOWED_TOOLTIP));
  return true;
}

// Notifications --------------------------------------------------------------

ContentSettingNotificationsImageModel::ContentSettingNotificationsImageModel()
    : ContentSettingSimpleImageModel(
          ImageType::NOTIFICATIONS_QUIET_PROMPT,
          ContentSettingsType::NOTIFICATIONS,
          true /* image_type_should_notify_accessibility */) {
  SetIcon(ContentSettingsType::NOTIFICATIONS, /*blocked=*/false);
  set_tooltip(
      l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT));
}

bool ContentSettingNotificationsImageModel::UpdateAndGetVisibility(
    WebContents* web_contents) {
  auto* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // We shouldn't show the icon unless we're a PWA.
  // TODO(crbug.com/1221189): Allow PermissionRequestManager to identify the
  // correct UI style of a permission prompt.
  const bool quiet_icon_allowed = web_app::AppBrowserController::IsWebApp(
      chrome::FindBrowserWithTab(web_contents));

  if (!quiet_icon_allowed || !manager ||
      !manager->ShouldCurrentRequestUseQuietUI()) {
    return false;
  }

  // |manager| may be null in tests.
  // Show promo the first time a quiet prompt is shown to the user.
  set_should_show_promo(
      QuietNotificationPermissionUiState::ShouldShowPromo(profile));
  if (permissions::PermissionUiSelector::ShouldSuppressAnimation(
          manager->ReasonForUsingQuietUi())) {
    set_accessibility_string_id(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT);
    set_explanatory_string_id(0);
  } else {
    set_explanatory_string_id(IDS_NOTIFICATIONS_OFF_EXPLANATORY_TEXT);
  }
  return true;
}

void ContentSettingNotificationsImageModel::SetPromoWasShown(
    content::WebContents* contents) {
  DCHECK(contents);
  auto* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  QuietNotificationPermissionUiState::PromoWasShown(profile);

  ContentSettingImageModel::SetPromoWasShown(contents);
}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingNotificationsImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents) {
  return std::make_unique<ContentSettingQuietRequestBubbleModel>(delegate,
                                                                 web_contents);
}

// Base class ------------------------------------------------------------------

gfx::Image ContentSettingImageModel::GetIcon(SkColor icon_color) const {
  int icon_size =
      icon_size_.value_or(GetLayoutConstant(LOCATION_BAR_TRAILING_ICON_SIZE));
  return gfx::Image(gfx::CreateVectorIconWithBadge(*icon_, icon_size,
                                                   icon_color, *icon_badge_));
}

void ContentSettingImageModel::SetIconSize(int icon_size) {
  icon_size_ = icon_size;
}

int ContentSettingImageModel::AccessibilityAnnouncementStringId() const {
  return explanatory_string_id_ ? explanatory_string_id_
                                : accessibility_string_id_;
}

ContentSettingImageModel::ContentSettingImageModel(
    ImageType image_type,
    bool image_type_should_notify_accessibility)
    : icon_(&gfx::kNoneIcon),
      icon_badge_(&gfx::kNoneIcon),
      image_type_(image_type),
      image_type_should_notify_accessibility_(
          image_type_should_notify_accessibility) {}

std::unique_ptr<ContentSettingBubbleModel>
ContentSettingImageModel::CreateBubbleModel(
    ContentSettingBubbleModel::Delegate* delegate,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  return CreateBubbleModelImpl(delegate, web_contents);
}

// static
std::vector<std::unique_ptr<ContentSettingImageModel>>
ContentSettingImageModel::GenerateContentSettingImageModels() {
  // The ordering of the models here influences the order in which icons are
  // shown in the omnibox.
  constexpr ImageType kContentSettingImageOrder[] = {
      ImageType::COOKIES,
      ImageType::IMAGES,
      ImageType::JAVASCRIPT,
      ImageType::POPUPS,
      ImageType::GEOLOCATION,
      ImageType::MIXEDSCRIPT,
      ImageType::PROTOCOL_HANDLERS,
      ImageType::MEDIASTREAM,
      ImageType::SENSORS,
      ImageType::ADS,
      ImageType::AUTOMATIC_DOWNLOADS,
      ImageType::MIDI,
      ImageType::MIDI_SYSEX,
      ImageType::SOUND,
      ImageType::FRAMEBUST,
      ImageType::CLIPBOARD_READ_WRITE,
      ImageType::NOTIFICATIONS_QUIET_PROMPT,
      ImageType::STORAGE_ACCESS,
  };

  std::vector<std::unique_ptr<ContentSettingImageModel>> result;
  for (auto type : kContentSettingImageOrder) {
    if (!base::FeatureList::IsEnabled(features::kBlockMidiByDefault) &&
        type == ImageType::MIDI) {
      continue;
    }
    result.push_back(CreateForContentType(type));
  }

  return result;
}

// static
size_t ContentSettingImageModel::GetContentSettingImageModelIndexForTesting(
    ImageType image_type) {
  std::vector<std::unique_ptr<ContentSettingImageModel>> models =
      GenerateContentSettingImageModels();
  for (size_t i = 0; i < models.size(); ++i) {
    if (image_type == models[i]->image_type())
      return i;
  }
  NOTREACHED();
  return models.size();
}
