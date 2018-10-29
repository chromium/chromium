// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

using content::WebContents;

// The image models hierarchy:
//
// ContentSettingImageModel                  - base class
//   ContentSettingSimpleImageModel            - single content setting
//     ContentSettingBlockedImageModel           - generic blocked setting
//     ContentSettingGeolocationImageModel       - geolocation
//     ContentSettingRPHImageModel               - protocol handlers
//     ContentSettingMIDISysExImageModel         - midi sysex
//     ContentSettingDownloadsImageModel         - automatic downloads
//     ContentSettingClipboardReadImageModel     - clipboard read
//     ContentSettingSensorsImageModel           - sensors
//   ContentSettingMediaImageModel             - media
//   ContentSettingFramebustBlockImageModel    - blocked framebust

class ContentSettingBlockedImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingBlockedImageModel(ImageType image_type,
                                  ContentSettingsType content_type);

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingBlockedImageModel);
};

class ContentSettingGeolocationImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingGeolocationImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingGeolocationImageModel);
};

class ContentSettingRPHImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingRPHImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingRPHImageModel);
};

class ContentSettingMIDISysExImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingMIDISysExImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingMIDISysExImageModel);
};

class ContentSettingDownloadsImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingDownloadsImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingDownloadsImageModel);
};

class ContentSettingClipboardReadImageModel
    : public ContentSettingSimpleImageModel {
 public:
  ContentSettingClipboardReadImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingClipboardReadImageModel);
};

// Image model for displaying media icons in the location bar.
class ContentSettingMediaImageModel : public ContentSettingImageModel {
 public:
  ContentSettingMediaImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

  ContentSettingBubbleModel* CreateBubbleModelImpl(
      ContentSettingBubbleModel::Delegate* delegate,
      WebContents* web_contents,
      Profile* profile) override;

  bool ShouldRunAnimation(WebContents* web_contents) override;
  void SetAnimationHasRun(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingMediaImageModel);
};

class ContentSettingSensorsImageModel : public ContentSettingSimpleImageModel {
 public:
  ContentSettingSensorsImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentSettingSensorsImageModel);
};

namespace {

struct ContentSettingsImageDetails {
  ContentSettingsType content_type;
  const gfx::VectorIcon& icon;
  int blocked_tooltip_id;
  int blocked_explanatory_text_id;
  int accessed_tooltip_id;
};

const ContentSettingsImageDetails kImageDetails[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, kCookieIcon, IDS_BLOCKED_COOKIES_MESSAGE, 0,
     IDS_ACCESSED_COOKIES_MESSAGE},
    {CONTENT_SETTINGS_TYPE_IMAGES, kPhotoIcon, IDS_BLOCKED_IMAGES_MESSAGE, 0,
     0},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, kCodeIcon,
     IDS_BLOCKED_JAVASCRIPT_MESSAGE, 0, 0},
    {CONTENT_SETTINGS_TYPE_PLUGINS, kExtensionIcon, IDS_BLOCKED_PLUGINS_MESSAGE,
     IDS_BLOCKED_PLUGIN_EXPLANATORY_TEXT, 0},
    {CONTENT_SETTINGS_TYPE_POPUPS, kWebIcon, IDS_BLOCKED_POPUPS_TOOLTIP,
     IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT, 0},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, kMixedContentIcon,
     IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT, 0, 0},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, kExtensionIcon,
     IDS_BLOCKED_PPAPI_BROKER_MESSAGE, 0, IDS_ALLOWED_PPAPI_BROKER_MESSAGE},
    {CONTENT_SETTINGS_TYPE_SOUND, kTabAudioIcon, IDS_BLOCKED_SOUND_TITLE, 0, 0},
    {CONTENT_SETTINGS_TYPE_ADS, kAdsIcon, IDS_BLOCKED_ADS_PROMPT_TOOLTIP,
     IDS_BLOCKED_ADS_PROMPT_TITLE, 0},
};

const ContentSettingsImageDetails* GetImageDetails(ContentSettingsType type) {
  for (const ContentSettingsImageDetails& image_details : kImageDetails) {
    if (image_details.content_type == type)
      return &image_details;
  }
  return nullptr;
}

}  // namespace

// Single content setting ------------------------------------------------------

ContentSettingSimpleImageModel::ContentSettingSimpleImageModel(
    ImageType image_type,
    ContentSettingsType content_type)
    : ContentSettingImageModel(image_type), content_type_(content_type) {}

ContentSettingBubbleModel*
ContentSettingSimpleImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents,
    Profile* profile) {
  return ContentSettingBubbleModel::CreateContentSettingBubbleModel(
      delegate,
      web_contents,
      profile,
      content_type());
}

bool ContentSettingSimpleImageModel::ShouldRunAnimation(
    WebContents* web_contents) {
  if (!web_contents)
    return false;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;

  return !content_settings->IsBlockageIndicated(content_type());
}

void ContentSettingSimpleImageModel::SetAnimationHasRun(
    WebContents* web_contents) {
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (content_settings)
    content_settings->SetBlockageHasBeenIndicated(content_type());
}

// static
std::unique_ptr<ContentSettingImageModel>
ContentSettingImageModel::CreateForContentType(ImageType image_type) {
  switch (image_type) {
    case ImageType::COOKIES:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::COOKIES, CONTENT_SETTINGS_TYPE_COOKIES);
    case ImageType::IMAGES:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::IMAGES, CONTENT_SETTINGS_TYPE_IMAGES);
    case ImageType::JAVASCRIPT:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::JAVASCRIPT, CONTENT_SETTINGS_TYPE_JAVASCRIPT);
    case ImageType::PPAPI_BROKER:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::PPAPI_BROKER, CONTENT_SETTINGS_TYPE_PPAPI_BROKER);
    case ImageType::PLUGINS:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::PLUGINS, CONTENT_SETTINGS_TYPE_PLUGINS);
    case ImageType::POPUPS:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::POPUPS, CONTENT_SETTINGS_TYPE_POPUPS);
    case ImageType::GEOLOCATION:
      return std::make_unique<ContentSettingGeolocationImageModel>();
    case ImageType::MIXEDSCRIPT:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::MIXEDSCRIPT, CONTENT_SETTINGS_TYPE_MIXEDSCRIPT);
    case ImageType::PROTOCOL_HANDLERS:
      return std::make_unique<ContentSettingRPHImageModel>();
    case ImageType::MEDIASTREAM:
      return std::make_unique<ContentSettingMediaImageModel>();
    case ImageType::ADS:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::ADS, CONTENT_SETTINGS_TYPE_ADS);
    case ImageType::AUTOMATIC_DOWNLOADS:
      return std::make_unique<ContentSettingDownloadsImageModel>();
    case ImageType::MIDI_SYSEX:
      return std::make_unique<ContentSettingMIDISysExImageModel>();
    case ImageType::SOUND:
      return std::make_unique<ContentSettingBlockedImageModel>(
          ImageType::SOUND, CONTENT_SETTINGS_TYPE_SOUND);
    case ImageType::FRAMEBUST:
      return std::make_unique<ContentSettingFramebustBlockImageModel>();
    case ImageType::CLIPBOARD_READ:
      return std::make_unique<ContentSettingClipboardReadImageModel>();
    case ImageType::SENSORS:
      return std::make_unique<ContentSettingSensorsImageModel>();
    case ImageType::NUM_IMAGE_TYPES:
      break;
  }
  NOTREACHED();
  return nullptr;
}

// Generic blocked content settings --------------------------------------------

ContentSettingBlockedImageModel::ContentSettingBlockedImageModel(
    ImageType image_type,
    ContentSettingsType content_type)
    : ContentSettingSimpleImageModel(image_type, content_type) {}

void ContentSettingBlockedImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;

  const ContentSettingsType type = content_type();
  const ContentSettingsImageDetails* image_details = GetImageDetails(type);
  DCHECK(image_details) << "No entry for " << type << " in kImageDetails[].";

  int tooltip_id = image_details->blocked_tooltip_id;
  int explanation_id = image_details->blocked_explanatory_text_id;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    GURL url = web_contents->GetURL();
    ContentSetting setting =
        map->GetContentSetting(url, url, type, std::string());

    // For plugins, show the animated explanation in these cases:
    //  - The plugin is blocked despite the user having content setting ALLOW.
    //  - The user has disabled Flash using BLOCK and HTML5 By Default feature.
    bool show_explanation = setting == CONTENT_SETTING_ALLOW ||
                            (setting == CONTENT_SETTING_BLOCK &&
                             PluginUtils::ShouldPreferHtmlOverPlugins(map));
    if (!show_explanation)
      explanation_id = 0;
  }

  // If a content type is blocked by default and was accessed, display the
  // content blocked page action.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  if (!content_settings->IsContentBlocked(type)) {
    if (!content_settings->IsContentAllowed(type))
      return;

    // For cookies, only show the cookie blocked page action if cookies are
    // blocked by default.
    if (type == CONTENT_SETTINGS_TYPE_COOKIES &&
        (map->GetDefaultContentSetting(type, nullptr) != CONTENT_SETTING_BLOCK))
      return;

    tooltip_id = image_details->accessed_tooltip_id;
    explanation_id = 0;
  }
  set_visible(true);
  const gfx::VectorIcon* badge_id = &gfx::kNoneIcon;
  if (type == CONTENT_SETTINGS_TYPE_PPAPI_BROKER)
    badge_id = &kWarningBadgeIcon;
  else if (content_settings->IsContentBlocked(type))
    badge_id = &kBlockedBadgeIcon;

  const gfx::VectorIcon* icon = &image_details->icon;
  // Touch mode uses a different tab audio icon.
  if (image_details->content_type == CONTENT_SETTINGS_TYPE_SOUND &&
      ui::MaterialDesignController::touch_ui()) {
    icon = &kTabAudioRoundedIcon;
  }
  set_icon(*icon, *badge_id);
  set_explanatory_string_id(explanation_id);
  DCHECK(tooltip_id);
  set_tooltip(l10n_util::GetStringUTF16(tooltip_id));
}

// Geolocation -----------------------------------------------------------------

ContentSettingGeolocationImageModel::ContentSettingGeolocationImageModel()
    : ContentSettingSimpleImageModel(ImageType::GEOLOCATION,
                                     CONTENT_SETTINGS_TYPE_GEOLOCATION) {}

void ContentSettingGeolocationImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  const ContentSettingsUsagesState& usages_state = content_settings->
      geolocation_usages_state();
  if (usages_state.state_map().empty())
    return;
  set_visible(true);

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int state_flags = 0;
  usages_state.GetDetailedInfo(nullptr, &state_flags);
  bool allowed =
      !!(state_flags & ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED);
  set_icon(kMyLocationIcon, allowed ? gfx::kNoneIcon : kBlockedBadgeIcon);
  set_tooltip(l10n_util::GetStringUTF16(allowed
                                            ? IDS_GEOLOCATION_ALLOWED_TOOLTIP
                                            : IDS_GEOLOCATION_BLOCKED_TOOLTIP));
}

// Protocol handlers -----------------------------------------------------------

ContentSettingRPHImageModel::ContentSettingRPHImageModel()
    : ContentSettingSimpleImageModel(ImageType::PROTOCOL_HANDLERS,
                                     CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
  set_icon(vector_icons::kProtocolHandlerIcon, gfx::kNoneIcon);
  set_tooltip(l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_TOOLTIP));
}

void ContentSettingRPHImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  if (content_settings->pending_protocol_handler().IsEmpty())
    return;

  set_visible(true);
}

// MIDI SysEx ------------------------------------------------------------------

ContentSettingMIDISysExImageModel::ContentSettingMIDISysExImageModel()
    : ContentSettingSimpleImageModel(ImageType::MIDI_SYSEX,
                                     CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {}

void ContentSettingMIDISysExImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  const ContentSettingsUsagesState& usages_state =
      content_settings->midi_usages_state();
  if (usages_state.state_map().empty())
    return;
  set_visible(true);

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int state_flags = 0;
  usages_state.GetDetailedInfo(nullptr, &state_flags);
  bool allowed =
      !!(state_flags & ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED);
  set_icon(vector_icons::kMidiIcon,
           allowed ? gfx::kNoneIcon : kBlockedBadgeIcon);
  set_tooltip(l10n_util::GetStringUTF16(allowed
                                            ? IDS_MIDI_SYSEX_ALLOWED_TOOLTIP
                                            : IDS_MIDI_SYSEX_BLOCKED_TOOLTIP));
}

// Automatic downloads ---------------------------------------------------------

ContentSettingDownloadsImageModel::ContentSettingDownloadsImageModel()
    : ContentSettingSimpleImageModel(
          ImageType::AUTOMATIC_DOWNLOADS,
          CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS) {}

void ContentSettingDownloadsImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;

  DownloadRequestLimiter* download_request_limiter =
      g_browser_process->download_request_limiter();

  // DownloadRequestLimiter can be absent in unit_tests.
  if (!download_request_limiter)
    return;

  switch (download_request_limiter->GetDownloadUiStatus(web_contents)) {
    case DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED:
      set_visible(true);
      set_icon(kFileDownloadIcon, gfx::kNoneIcon);
      set_explanatory_string_id(0);
      set_tooltip(l10n_util::GetStringUTF16(IDS_ALLOWED_DOWNLOAD_TITLE));
      return;
    case DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED:
      set_visible(true);
      set_icon(kFileDownloadIcon, kBlockedBadgeIcon);
      set_explanatory_string_id(IDS_BLOCKED_DOWNLOADS_EXPLANATION);
      set_tooltip(l10n_util::GetStringUTF16(IDS_BLOCKED_DOWNLOAD_TITLE));
      return;
    case DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT:
      // No need to show icon otherwise.
      return;
  }
}

// Clipboard -------------------------------------------------------------------

ContentSettingClipboardReadImageModel::ContentSettingClipboardReadImageModel()
    : ContentSettingSimpleImageModel(ImageType::CLIPBOARD_READ,
                                     CONTENT_SETTINGS_TYPE_CLIPBOARD_READ) {}

void ContentSettingClipboardReadImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  ContentSettingsType content_type = CONTENT_SETTINGS_TYPE_CLIPBOARD_READ;
  bool blocked = content_settings->IsContentBlocked(content_type);
  bool allowed = content_settings->IsContentAllowed(content_type);
  if (!blocked && !allowed)
    return;
  set_visible(true);

  set_icon(kContentPasteIcon, allowed ? gfx::kNoneIcon : kBlockedBadgeIcon);
  set_tooltip(l10n_util::GetStringUTF16(
      allowed ? IDS_ALLOWED_CLIPBOARD_MESSAGE : IDS_BLOCKED_CLIPBOARD_MESSAGE));
}

// Media -----------------------------------------------------------------------

ContentSettingMediaImageModel::ContentSettingMediaImageModel()
    : ContentSettingImageModel(ImageType::MEDIASTREAM) {}

void ContentSettingMediaImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);

  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  TabSpecificContentSettings::MicrophoneCameraState state =
      content_settings->GetMicrophoneCameraState();

  // If neither the microphone nor the camera stream was accessed then no icon
  // is displayed in the omnibox.
  if (state == TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED)
    return;

  bool is_mic = (state & TabSpecificContentSettings::MICROPHONE_ACCESSED) != 0;
  bool is_cam = (state & TabSpecificContentSettings::CAMERA_ACCESSED) != 0;
  DCHECK(is_mic || is_cam);

  int id = IDS_CAMERA_BLOCKED;
  if (state & (TabSpecificContentSettings::MICROPHONE_BLOCKED |
               TabSpecificContentSettings::CAMERA_BLOCKED)) {
    set_icon(vector_icons::kVideocamIcon, kBlockedBadgeIcon);
    if (is_mic)
      id = is_cam ? IDS_MICROPHONE_CAMERA_BLOCKED : IDS_MICROPHONE_BLOCKED;
  } else {
    set_icon(vector_icons::kVideocamIcon, gfx::kNoneIcon);
    id = IDS_CAMERA_ACCESSED;
    if (is_mic)
      id = is_cam ? IDS_MICROPHONE_CAMERA_ALLOWED : IDS_MICROPHONE_ACCESSED;
  }
  set_tooltip(l10n_util::GetStringUTF16(id));
  set_visible(true);
}

ContentSettingBubbleModel* ContentSettingMediaImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents,
    Profile* profile) {
  return new ContentSettingMediaStreamBubbleModel(delegate,
                                                  web_contents,
                                                  profile);
}

bool ContentSettingMediaImageModel::ShouldRunAnimation(
    WebContents* web_contents) {
  if (!web_contents)
    return false;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return false;
  return (!content_settings->IsBlockageIndicated(
              CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC) &&
          !content_settings->IsBlockageIndicated(
              CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));
}

void ContentSettingMediaImageModel::SetAnimationHasRun(
    WebContents* web_contents) {
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (content_settings) {
    content_settings->SetBlockageHasBeenIndicated(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
    content_settings->SetBlockageHasBeenIndicated(
        CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  }
}


// Blocked Framebust -----------------------------------------------------------
ContentSettingFramebustBlockImageModel::ContentSettingFramebustBlockImageModel()
    : ContentSettingImageModel(ImageType::FRAMEBUST) {}

void ContentSettingFramebustBlockImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);

  if (!web_contents)
    return;

  // Early exit if no blocked Framebust.
  if (!FramebustBlockTabHelper::FromWebContents(web_contents)->HasBlockedUrls())
    return;

  set_icon(kBlockedRedirectIcon, kBlockedBadgeIcon);
  set_explanatory_string_id(IDS_REDIRECT_BLOCKED_TITLE);
  set_tooltip(l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_TOOLTIP));
  set_visible(true);
}

ContentSettingBubbleModel*
ContentSettingFramebustBlockImageModel::CreateBubbleModelImpl(
    ContentSettingBubbleModel::Delegate* delegate,
    WebContents* web_contents,
    Profile* profile) {
  return new ContentSettingFramebustBlockBubbleModel(delegate, web_contents,
                                                     profile);
}

bool ContentSettingFramebustBlockImageModel::ShouldRunAnimation(
    WebContents* web_contents) {
  return web_contents && !FramebustBlockTabHelper::FromWebContents(web_contents)
                              ->animation_has_run();
}

void ContentSettingFramebustBlockImageModel::SetAnimationHasRun(
    WebContents* web_contents) {
  if (!web_contents)
    return;
  FramebustBlockTabHelper::FromWebContents(web_contents)
      ->set_animation_has_run();
}

// Sensors ---------------------------------------------------------------------

ContentSettingSensorsImageModel::ContentSettingSensorsImageModel()
    : ContentSettingSimpleImageModel(ImageType::SENSORS,
                                     CONTENT_SETTINGS_TYPE_SENSORS) {}

void ContentSettingSensorsImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;
  auto* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;

  bool blocked = content_settings->IsContentBlocked(content_type());
  bool allowed = content_settings->IsContentAllowed(content_type());
  if (!blocked && !allowed)
    return;

  set_visible(true);
  set_icon(kSensorsIcon, allowed ? gfx::kNoneIcon : kBlockedBadgeIcon);
  set_tooltip(l10n_util::GetStringUTF16(allowed ? IDS_SENSORS_ALLOWED_TOOLTIP
                                                : IDS_SENSORS_BLOCKED_TOOLTIP));
}

// Base class ------------------------------------------------------------------

gfx::Image ContentSettingImageModel::GetIcon(SkColor icon_color) const {
  int icon_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);
  return gfx::Image(gfx::CreateVectorIconWithBadge(*icon_, icon_size,
                                                   icon_color, *icon_badge_));
}

ContentSettingImageModel::ContentSettingImageModel(ImageType image_type)
    : is_visible_(false),
      icon_(&gfx::kNoneIcon),
      icon_badge_(&gfx::kNoneIcon),
      explanatory_string_id_(0),
      image_type_(image_type) {}

ContentSettingBubbleModel* ContentSettingImageModel::CreateBubbleModel(
    ContentSettingBubbleModel::Delegate* delegate,
    content::WebContents* web_contents,
    Profile* profile) {
  UMA_HISTOGRAM_ENUMERATION(
      "ContentSettings.ImagePressed", image_type(),
      ContentSettingImageModel::ImageType::NUM_IMAGE_TYPES);
  return CreateBubbleModelImpl(delegate, web_contents, profile);
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
      ImageType::PPAPI_BROKER,
      ImageType::PLUGINS,
      ImageType::POPUPS,
      ImageType::GEOLOCATION,
      ImageType::MIXEDSCRIPT,
      ImageType::PROTOCOL_HANDLERS,
      ImageType::MEDIASTREAM,
      ImageType::SENSORS,
      ImageType::ADS,
      ImageType::AUTOMATIC_DOWNLOADS,
      ImageType::MIDI_SYSEX,
      ImageType::SOUND,
      ImageType::FRAMEBUST,
      ImageType::CLIPBOARD_READ,
  };

  std::vector<std::unique_ptr<ContentSettingImageModel>> result;
  for (auto type : kContentSettingImageOrder)
    result.push_back(CreateForContentType(type));

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

#if defined(OS_MACOSX)
bool ContentSettingImageModel::UpdateFromWebContentsAndCheckIfIconChanged(
    content::WebContents* web_contents) {
  const gfx::VectorIcon* old_icon = icon_;
  const gfx::VectorIcon* old_badge_icon = icon_badge_;
  UpdateFromWebContents(web_contents);
  return old_icon != icon_ && old_badge_icon != icon_badge_;
}
#endif
