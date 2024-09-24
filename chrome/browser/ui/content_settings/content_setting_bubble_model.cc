// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/permissions/permission_actions_history_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/permissions/system/system_permission_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/url_identity.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/permissions/constants.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "components/permissions/permissions_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/page.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/resources/grit/ui_resources.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/permissions/system/system_media_capture_permissions_mac.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#endif

using base::UserMetricsAction;
using content::WebContents;
using content_settings::PageSpecificContentSettings;
using content_settings::SettingInfo;
using content_settings::SettingSource;
using content_settings::mojom::SessionModel;

namespace {
using ContentSettingBubbleAction =
    ContentSettingBubbleModel::ContentSettingBubbleAction;

void RecordActionHistogram(ContentSettingsType type,
                           ContentSettingBubbleAction action) {
  switch (type) {
    case ContentSettingsType::STORAGE_ACCESS:
      base::UmaHistogramEnumeration(
          "ContentSettings.Bubble.StorageAccess.Action", action);
      break;
    default:
      // Currently only defined and implemented for StorageAccess.
      NOTREACHED();
  }
}

using QuietUiReason = permissions::PermissionRequestManager::QuietUiReason;

const std::u16string& GetDefaultDisplayURLForTesting() {
  static const base::NoDestructor<std::u16string> kDefaultDisplayURL(
      u"http://www.example.com");
  return *kDefaultDisplayURL;
}

// An override display URL in content setting bubble UI for testing.
std::optional<bool> g_display_url_override_for_testing = std::nullopt;

// Returns a boolean indicating whether the setting should be managed by the
// user (i.e. it is not controlled by policy). Also takes a (nullable) out-param
// which is populated by the actual setting for the given URL.
bool GetSettingManagedByUser(const GURL& url,
                             ContentSettingsType type,
                             Profile* profile,
                             ContentSetting* out_setting) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  SettingInfo info;
  ContentSetting setting;
  if (type == ContentSettingsType::COOKIES) {
    // TODO(crbug.com/40247160): Consider whether the following check should
    // somehow determine real CookieSettingOverrides rather than default to
    // none.
    setting = CookieSettingsFactory::GetForProfile(profile)->GetCookieSetting(
        url, net::SiteForCookies::FromUrl(url), url,
        net::CookieSettingOverrides(), &info);
  } else {
    setting = map->GetContentSetting(url, url, type, &info);
  }

  if (out_setting)
    *out_setting = setting;

  // Prevent creation of content settings for illegal urls like about:blank by
  // disallowing user management.
  return info.source == SettingSource::kUser &&
         map->CanSetNarrowestContentSetting(url, url, type);
}

ContentSettingBubbleModel::ListItem CreateUrlListItem(int32_t id,
                                                      const GURL& url) {
  // Empty URLs should get a placeholder.
  // TODO(csharrison): See if we can DCHECK that the URL will be valid here.
  std::u16string title = url.spec().empty()
                             ? l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE)
                             : base::UTF8ToUTF16(url.spec());

  // Format the title to include the unicode single dot bullet code-point
  // \u2022 and two spaces.
  title = l10n_util::GetStringFUTF16(IDS_LIST_BULLET, title);
  return ContentSettingBubbleModel::ListItem(nullptr, title, std::u16string(),
                                             true /* has_link */,
                                             false /* has_blocked_badge */, id);
}

struct ContentSettingsTypeIdEntry {
  ContentSettingsType type;
  int id;
};

int GetIdForContentType(const ContentSettingsTypeIdEntry* entries,
                        size_t num_entries,
                        ContentSettingsType type) {
  for (size_t i = 0; i < num_entries; ++i) {
    if (entries[i].type == type)
      return entries[i].id;
  }
  return 0;
}

void SetAllowRunningInsecureContent(
    MixedContentSettingsTabHelper* mixed_content_settings,
    content::RenderFrameHost* frame) {
  // Set Insecure Content flag only if running insecure content is allowed
  if (mixed_content_settings &&
      !mixed_content_settings->IsRunningInsecureContentAllowed(*frame)) {
    return;
  }

  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> agent;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&agent);
  agent->SetAllowRunningInsecureContent();
}

constexpr UrlIdentity::TypeSet allowed_types = {
    UrlIdentity::Type::kDefault, UrlIdentity::Type::kFile,
    UrlIdentity::Type::kIsolatedWebApp};
constexpr UrlIdentity::FormatOptions options;

std::u16string GetUrlForDisplay(Profile* profile, const GURL& url) {
  if (g_display_url_override_for_testing.value_or(false)) {
    return GetDefaultDisplayURLForTesting();  // IN-TEST
  }
  UrlIdentity identity =
      UrlIdentity::CreateFromUrl(profile, url, allowed_types, options);
  return identity.name;
}

}  // namespace

// static
base::AutoReset<std::optional<bool>>
ContentSettingBubbleModel::CreateScopedDisplayURLOverrideForTesting() {
  return base::AutoReset<std::optional<bool>>(
      &g_display_url_override_for_testing, true);
}

// ContentSettingSimpleBubbleModel ---------------------------------------------
ContentSettingBubbleModel::ListItem::ListItem(const gfx::VectorIcon* image,
                                              const std::u16string& title,
                                              const std::u16string& description,
                                              bool has_link,
                                              bool has_blocked_badge,
                                              int32_t item_id)
    : image(image),
      title(title),
      description(description),
      has_link(has_link),
      has_blocked_badge(has_blocked_badge),
      item_id(item_id) {}

ContentSettingBubbleModel::ListItem::ListItem(const ListItem& other) = default;
ContentSettingBubbleModel::ListItem&
ContentSettingBubbleModel::ListItem::operator=(const ListItem& other) = default;

ContentSettingSimpleBubbleModel::ContentSettingSimpleBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    ContentSettingsType content_type)
    : ContentSettingBubbleModel(delegate, web_contents),
      content_type_(content_type) {
  SetTitle();
  SetMessage();
  SetManageText();
  SetCustomLink();
}

ContentSettingSimpleBubbleModel*
ContentSettingSimpleBubbleModel::AsSimpleBubbleModel() {
  return this;
}

bool ContentSettingSimpleBubbleModel::IsContentAllowed() {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument());

  if (content_type() == ContentSettingsType::COOKIES) {
    ContentSetting setting;
    GetSettingManagedByUser(web_contents()->GetLastCommittedURL(),
                            content_type(), GetProfile(), &setting);
    // We check the content setting here as well because 3PC access influences
    // the allowed/blocked status even though the icon is meant for 1PC control.
    return content_settings->IsContentAllowed(content_type()) &&
           setting == CONTENT_SETTING_ALLOW;
  }
  return content_settings->IsContentAllowed(content_type()) &&
         !content_settings->IsContentBlocked(content_type());
}

void ContentSettingSimpleBubbleModel::SetTitle() {
  static const ContentSettingsTypeIdEntry kBlockedTitleIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_ON_DEVICE_SITE_DATA_TITLE},
      {ContentSettingsType::IMAGES, IDS_BLOCKED_IMAGES_TITLE},
      {ContentSettingsType::JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_TITLE},
      {ContentSettingsType::MIXEDSCRIPT,
       IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT_TITLE},
      {ContentSettingsType::SOUND, IDS_BLOCKED_SOUND_TITLE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE, IDS_BLOCKED_CLIPBOARD_TITLE},
      {ContentSettingsType::GEOLOCATION, IDS_BLOCKED_GEOLOCATION_TITLE},
      {ContentSettingsType::MIDI_SYSEX, IDS_BLOCKED_MIDI_SYSEX_TITLE},
      {ContentSettingsType::SENSORS, IDS_BLOCKED_SENSORS_TITLE},
  };
  // Fields as for kBlockedTitleIDs, above.
  static const ContentSettingsTypeIdEntry kAccessedTitleIDs[] = {
      {ContentSettingsType::COOKIES, IDS_ACCESSED_ON_DEVICE_SITE_DATA_TITLE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE, IDS_ALLOWED_CLIPBOARD_TITLE},
      {ContentSettingsType::GEOLOCATION, IDS_ALLOWED_GEOLOCATION_TITLE},
      {ContentSettingsType::MIDI_SYSEX, IDS_ALLOWED_MIDI_SYSEX_TITLE},
      {ContentSettingsType::SENSORS, IDS_ALLOWED_SENSORS_TITLE},
  };
  const ContentSettingsTypeIdEntry* title_ids = kBlockedTitleIDs;
  size_t num_title_ids = std::size(kBlockedTitleIDs);
  if (IsContentAllowed()) {
    title_ids = kAccessedTitleIDs;
    num_title_ids = std::size(kAccessedTitleIDs);
  }
  int title_id = GetIdForContentType(title_ids, num_title_ids, content_type());
  if (title_id)
    set_title(l10n_util::GetStringUTF16(title_id));
}

void ContentSettingSimpleBubbleModel::SetMessage() {
  // TODO(crbug.com/40633805): Make the two arrays below static again once
  // we no longer need to check base::FeatureList.
  const ContentSettingsTypeIdEntry kBlockedMessageIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_ON_DEVICE_SITE_DATA_MESSAGE},
      {ContentSettingsType::IMAGES, IDS_BLOCKED_IMAGES_MESSAGE},
      {ContentSettingsType::JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_MESSAGE},
      // {ContentSettingsType::POPUPS, No message. intentionally left out},
      {ContentSettingsType::MIXEDSCRIPT,
       IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT},
      {ContentSettingsType::GEOLOCATION, IDS_BLOCKED_GEOLOCATION_MESSAGE},
      {ContentSettingsType::MIDI_SYSEX, IDS_BLOCKED_MIDI_SYSEX_MESSAGE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_BLOCKED_CLIPBOARD_MESSAGE},
      {ContentSettingsType::SENSORS,
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_BLOCKED_SENSORS_MESSAGE
           : IDS_BLOCKED_MOTION_SENSORS_MESSAGE},
  };
  // Fields as for kBlockedMessageIDs, above.
  const ContentSettingsTypeIdEntry kAccessedMessageIDs[] = {
      {ContentSettingsType::COOKIES, IDS_ACCESSED_ON_DEVICE_SITE_DATA_MESSAGE},
      {ContentSettingsType::GEOLOCATION, IDS_ALLOWED_GEOLOCATION_MESSAGE},
      {ContentSettingsType::MIDI_SYSEX, IDS_ALLOWED_MIDI_SYSEX_MESSAGE},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_ALLOWED_CLIPBOARD_MESSAGE},
      {ContentSettingsType::SENSORS,
       base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses)
           ? IDS_ALLOWED_SENSORS_MESSAGE
           : IDS_ALLOWED_MOTION_SENSORS_MESSAGE},
  };
  const ContentSettingsTypeIdEntry* message_ids = kBlockedMessageIDs;
  size_t num_message_ids = std::size(kBlockedMessageIDs);
  if (IsContentAllowed()) {
    message_ids = kAccessedMessageIDs;
    num_message_ids = std::size(kAccessedMessageIDs);
  }
  int message_id =
      GetIdForContentType(message_ids, num_message_ids, content_type());
  if (message_id)
    set_message(l10n_util::GetStringUTF16(message_id));
}

void ContentSettingSimpleBubbleModel::SetManageText() {
  set_manage_text(l10n_util::GetStringUTF16(IDS_MANAGE));
}

void ContentSettingSimpleBubbleModel::OnManageButtonClicked() {
  if (delegate()) {
    delegate()->ShowContentSettingsPage(content_type());
  }

  if (content_type() == ContentSettingsType::POPUPS) {
    content_settings::RecordPopupsAction(
        content_settings::POPUPS_ACTION_CLICKED_MANAGE_POPUPS_BLOCKING);
  }
}

void ContentSettingSimpleBubbleModel::SetCustomLink() {
  static const ContentSettingsTypeIdEntry kCustomIDs[] = {
      {ContentSettingsType::MIXEDSCRIPT, IDS_ALLOW_INSECURE_CONTENT_BUTTON},
  };
  int custom_link_id =
      GetIdForContentType(kCustomIDs, std::size(kCustomIDs), content_type());
  if (custom_link_id)
    set_custom_link(l10n_util::GetStringUTF16(custom_link_id));
}

void ContentSettingSimpleBubbleModel::OnCustomLinkClicked() {}

// ContentSettingMixedScriptBubbleModel ----------------------------------------

class ContentSettingMixedScriptBubbleModel
    : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingMixedScriptBubbleModel(Delegate* delegate,
                                       WebContents* web_contents);

  ContentSettingMixedScriptBubbleModel(
      const ContentSettingMixedScriptBubbleModel&) = delete;
  ContentSettingMixedScriptBubbleModel& operator=(
      const ContentSettingMixedScriptBubbleModel&) = delete;

  ~ContentSettingMixedScriptBubbleModel() override = default;

 private:
  void SetManageText();

  // ContentSettingBubbleModel:
  void OnLearnMoreClicked() override;
  void OnCustomLinkClicked() override;
};

ContentSettingMixedScriptBubbleModel::ContentSettingMixedScriptBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      ContentSettingsType::MIXEDSCRIPT) {
  set_custom_link_enabled(true);
  set_show_learn_more(true);
  SetManageText();
}

void ContentSettingMixedScriptBubbleModel::OnLearnMoreClicked() {
  if (delegate())
    delegate()->ShowLearnMorePage(content_type());
}

void ContentSettingMixedScriptBubbleModel::OnCustomLinkClicked() {
  MixedContentSettingsTabHelper* mixed_content_settings =
      MixedContentSettingsTabHelper::FromWebContents(web_contents());
  if (mixed_content_settings) {
    // Update browser side settings to allow active mixed content.
    mixed_content_settings->AllowRunningOfInsecureContent(
        GetPage().GetMainDocument());
  }

  // Update renderer side settings to allow active mixed content.
  GetPage().GetMainDocument().ForEachRenderFrameHostWithAction(
      [mixed_content_settings](content::RenderFrameHost* frame) {
        // Stop the child frame enumeration if we have reached a fenced frame.
        // This is correct since fence frames should ignore InsecureContent
        // setting.
        if (frame->IsFencedFrameRoot())
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        SetAllowRunningInsecureContent(mixed_content_settings, frame);
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
}

// Don't set any manage text since none is displayed.
void ContentSettingMixedScriptBubbleModel::SetManageText() {
  set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kNone);
}

// ContentSettingRPHBubbleModel ------------------------------------------------

namespace {

// These states must match the order of appearance of the radio buttons
// in the XIB file for the Mac port.
enum RPHState {
  RPH_ALLOW = 0,
  RPH_BLOCK,
  RPH_IGNORE,
};

}  // namespace

ContentSettingRPHBubbleModel::ContentSettingRPHBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    custom_handlers::ProtocolHandlerRegistry* registry)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      ContentSettingsType::PROTOCOL_HANDLERS),
      registry_(registry),
      pending_handler_(
          custom_handlers::ProtocolHandler::EmptyProtocolHandler()),
      previous_handler_(
          custom_handlers::ProtocolHandler::EmptyProtocolHandler()) {
  auto* content_settings =
      PageSpecificContentSettingsDelegate::FromWebContents(web_contents);
  pending_handler_ = content_settings->pending_protocol_handler();
  previous_handler_ = content_settings->previous_protocol_handler();

  std::u16string protocol = pending_handler_.GetProtocolDisplayName();

  // Note that we ignore the |title| parameter.
  if (previous_handler_.IsEmpty()) {
    set_title(l10n_util::GetStringFUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM,
        base::UTF8ToUTF16(pending_handler_.url().host()), protocol));
  } else {
    set_title(l10n_util::GetStringFUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE,
        base::UTF8ToUTF16(pending_handler_.url().host()), protocol,
        base::UTF8ToUTF16(previous_handler_.url().host())));
  }

  std::u16string radio_allow_label =
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_ACCEPT);
  std::u16string radio_deny_label =
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_DENY);
  std::u16string radio_ignore_label =
      l10n_util::GetStringUTF16(IDS_REGISTER_PROTOCOL_HANDLER_IGNORE);

  const GURL& url = web_contents->GetLastCommittedURL();
  RadioGroup radio_group;
  radio_group.url = url;

  radio_group.radio_items = {radio_allow_label, radio_deny_label,
                             radio_ignore_label};
  ContentSetting setting = content_settings->pending_protocol_handler_setting();
  if (setting == CONTENT_SETTING_ALLOW)
    radio_group.default_item = RPH_ALLOW;
  else if (setting == CONTENT_SETTING_BLOCK)
    radio_group.default_item = RPH_BLOCK;
  else
    radio_group.default_item = RPH_IGNORE;

  set_radio_group(radio_group);
}

ContentSettingRPHBubbleModel::~ContentSettingRPHBubbleModel() = default;

void ContentSettingRPHBubbleModel::CommitChanges() {
  PerformActionForSelectedItem();

  // The user has one chance to deal with the RPH content setting UI,
  // then we remove it.
  PageSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->ClearPendingProtocolHandler();
  content_settings::UpdateLocationBarUiForWebContents(web_contents());
}

void ContentSettingRPHBubbleModel::RegisterProtocolHandler() {
  // A no-op if the handler hasn't been ignored, but needed in case the user
  // selects sequences like register/ignore/register.
  registry_->RemoveIgnoredHandler(pending_handler_);

  registry_->OnAcceptRegisterProtocolHandler(pending_handler_);
  PageSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler_setting(CONTENT_SETTING_ALLOW);
}

void ContentSettingRPHBubbleModel::UnregisterProtocolHandler() {
  registry_->OnDenyRegisterProtocolHandler(pending_handler_);
  PageSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler_setting(CONTENT_SETTING_BLOCK);
  ClearOrSetPreviousHandler();
}

void ContentSettingRPHBubbleModel::IgnoreProtocolHandler() {
  registry_->OnIgnoreRegisterProtocolHandler(pending_handler_);
  PageSpecificContentSettingsDelegate::FromWebContents(web_contents())
      ->set_pending_protocol_handler_setting(CONTENT_SETTING_DEFAULT);
  ClearOrSetPreviousHandler();
}

void ContentSettingRPHBubbleModel::ClearOrSetPreviousHandler() {
  if (previous_handler_.IsEmpty()) {
    registry_->ClearDefault(pending_handler_.protocol());
  } else {
    registry_->OnAcceptRegisterProtocolHandler(previous_handler_);
  }
}

void ContentSettingRPHBubbleModel::PerformActionForSelectedItem() {
  if (selected_item() == RPH_ALLOW)
    RegisterProtocolHandler();
  else if (selected_item() == RPH_BLOCK)
    UnregisterProtocolHandler();
  else if (selected_item() == RPH_IGNORE)
    IgnoreProtocolHandler();
  else
    NOTREACHED_IN_MIGRATION();
}

// ContentSettingSingleRadioGroup ----------------------------------------------

ContentSettingSingleRadioGroup::ContentSettingSingleRadioGroup(
    Delegate* delegate,
    WebContents* web_contents,
    ContentSettingsType content_type)
    : ContentSettingSimpleBubbleModel(delegate, web_contents, content_type),
      block_setting_(CONTENT_SETTING_BLOCK) {
  SetRadioGroup();
  set_is_user_modifiable(GetSettingManagedByUser(
      web_contents->GetURL(), content_type, GetProfile(), nullptr));
}

ContentSettingSingleRadioGroup::~ContentSettingSingleRadioGroup() = default;

void ContentSettingSingleRadioGroup::CommitChanges() {
  if (settings_changed()) {
    ContentSetting setting = selected_item() == kAllowButtonIndex
                                 ? CONTENT_SETTING_ALLOW
                                 : block_setting_;
    permissions::PermissionUmaUtil::ScopedRevocationReporter
        scoped_revocation_reporter(
            GetProfile(), bubble_content().radio_group.url, GURL(),
            content_type(), permissions::PermissionSourceUI::PAGE_ACTION);
    SetNarrowestContentSetting(setting);
  }
}

bool ContentSettingSingleRadioGroup::settings_changed() const {
  return selected_item() != bubble_content().radio_group.default_item;
}

// Initialize the radio group by setting the appropriate labels for the
// content type and setting the default value based on the content setting.
void ContentSettingSingleRadioGroup::SetRadioGroup() {
  const GURL& url = web_contents()->GetURL();
  const std::u16string& display_url = GetUrlForDisplay(GetProfile(), url);
  bool allowed = IsContentAllowed();

  // For the frame busting case the content is blocked but its content type is
  // popup, and the popup PageSpecificContentSettings is unaware of the frame
  // busting block. Since the popup bubble won't happen without blocking, it's
  // safe to manually set this.
  if (content_type() == ContentSettingsType::POPUPS)
    allowed = false;

  DCHECK(!allowed ||
         PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument())
             ->IsContentAllowed(content_type()));

  RadioGroup radio_group;
  radio_group.url = url;

  static const ContentSettingsTypeIdEntry kBlockedAllowIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_ON_DEVICE_SITE_DATA_UNBLOCK},
      {ContentSettingsType::IMAGES, IDS_BLOCKED_IMAGES_UNBLOCK},
      {ContentSettingsType::JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_UNBLOCK},
      {ContentSettingsType::POPUPS, IDS_BLOCKED_POPUPS_REDIRECTS_UNBLOCK},
      {ContentSettingsType::SOUND, IDS_BLOCKED_SOUND_UNBLOCK},
      {ContentSettingsType::GEOLOCATION, IDS_BLOCKED_GEOLOCATION_UNBLOCK},
      {ContentSettingsType::MIDI_SYSEX, IDS_BLOCKED_MIDI_SYSEX_UNBLOCK},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_BLOCKED_CLIPBOARD_UNBLOCK},
      {ContentSettingsType::SENSORS, IDS_BLOCKED_SENSORS_UNBLOCK},
  };
  // Fields as for kBlockedAllowIDs, above.
  static const ContentSettingsTypeIdEntry kAllowedAllowIDs[] = {
      {ContentSettingsType::COOKIES, IDS_ALLOWED_ON_DEVICE_SITE_DATA_NO_ACTION},
      {ContentSettingsType::GEOLOCATION, IDS_ALLOWED_GEOLOCATION_NO_ACTION},
      {ContentSettingsType::MIDI_SYSEX, IDS_ALLOWED_MIDI_SYSEX_NO_ACTION},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_ALLOWED_CLIPBOARD_NO_ACTION},
      {ContentSettingsType::SENSORS, IDS_ALLOWED_SENSORS_NO_ACTION},
  };

  std::u16string radio_allow_label;
  if (allowed) {
    int resource_id = GetIdForContentType(
        kAllowedAllowIDs, std::size(kAllowedAllowIDs), content_type());
    radio_allow_label = l10n_util::GetStringUTF16(resource_id);
  } else {
    int resource_id = GetIdForContentType(
        kBlockedAllowIDs, std::size(kBlockedAllowIDs), content_type());
    if (content_type() == ContentSettingsType::COOKIES) {
      radio_allow_label = l10n_util::GetStringUTF16(resource_id);
    } else {
      radio_allow_label = l10n_util::GetStringFUTF16(resource_id, display_url);
    }
  }

  static const ContentSettingsTypeIdEntry kBlockedBlockIDs[] = {
      {ContentSettingsType::COOKIES, IDS_BLOCKED_ON_DEVICE_SITE_DATA_NO_ACTION},
      {ContentSettingsType::IMAGES, IDS_BLOCKED_IMAGES_NO_ACTION},
      {ContentSettingsType::JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_NO_ACTION},
      {ContentSettingsType::POPUPS, IDS_BLOCKED_POPUPS_REDIRECTS_NO_ACTION},
      {ContentSettingsType::SOUND, IDS_BLOCKED_SOUND_NO_ACTION},
      {ContentSettingsType::GEOLOCATION, IDS_BLOCKED_GEOLOCATION_NO_ACTION},
      {ContentSettingsType::MIDI_SYSEX, IDS_BLOCKED_MIDI_SYSEX_NO_ACTION},
      {ContentSettingsType::CLIPBOARD_READ_WRITE,
       IDS_BLOCKED_CLIPBOARD_NO_ACTION},
      {ContentSettingsType::SENSORS, IDS_BLOCKED_SENSORS_NO_ACTION},
  };
  static const ContentSettingsTypeIdEntry kAllowedBlockIDs[] = {
      {ContentSettingsType::COOKIES, IDS_ALLOWED_ON_DEVICE_SITE_DATA_BLOCK},
      {ContentSettingsType::GEOLOCATION, IDS_ALLOWED_GEOLOCATION_BLOCK},
      {ContentSettingsType::MIDI_SYSEX, IDS_ALLOWED_MIDI_SYSEX_BLOCK},
      {ContentSettingsType::CLIPBOARD_READ_WRITE, IDS_ALLOWED_CLIPBOARD_BLOCK},
      {ContentSettingsType::SENSORS, IDS_ALLOWED_SENSORS_BLOCK},
  };

  std::u16string radio_block_label;
  if (allowed) {
    int resource_id = GetIdForContentType(
        kAllowedBlockIDs, std::size(kAllowedBlockIDs), content_type());
    if (content_type() == ContentSettingsType::COOKIES) {
      radio_block_label = l10n_util::GetStringUTF16(resource_id);
    } else {
      radio_block_label = l10n_util::GetStringFUTF16(resource_id, display_url);
    }
  } else {
    radio_block_label = l10n_util::GetStringUTF16(GetIdForContentType(
        kBlockedBlockIDs, std::size(kBlockedBlockIDs), content_type()));
  }

  radio_group.radio_items = {radio_allow_label, radio_block_label};

  ContentSetting setting;
  GetSettingManagedByUser(url, content_type(), GetProfile(), &setting);
  if (setting == CONTENT_SETTING_ALLOW) {
    radio_group.default_item = kAllowButtonIndex;
    // |block_setting_| is already set to |CONTENT_SETTING_BLOCK|.
  } else {
    radio_group.default_item = 1;
    block_setting_ = setting;
  }
  set_radio_group(radio_group);
}

void ContentSettingSingleRadioGroup::SetNarrowestContentSetting(
    ContentSetting setting) {
  if (!GetProfile())
    return;

  auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetNarrowestContentSetting(bubble_content().radio_group.url,
                                  bubble_content().radio_group.url,
                                  content_type(), setting);
}

// ContentSettingStorageAccessBubbleModel --------------------------------------

ContentSettingStorageAccessBubbleModel::ContentSettingStorageAccessBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents) {
  RecordActionHistogram(ContentSettingsType::STORAGE_ACCESS,
                        ContentSettingBubbleAction::kOpened);
  set_title(l10n_util::GetStringUTF16(IDS_SITE_SETTINGS_TYPE_STORAGE_ACCESS));

  // TODO(crbug.com/40064079): Consider to add subtitles to all permissions.
  set_subtitle(url_formatter::FormatUrlForSecurityDisplay(
      web_contents->GetURL(),
      url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  set_message(l10n_util::GetStringFUTF16(
      IDS_STORAGE_ACCESS_PERMISSION_BUBBLE_MESSAGE,
      url_formatter::FormatUrlForSecurityDisplay(
          web_contents->GetURL(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC)));

  auto* page_content_settings =
      PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument());
  set_site_list(page_content_settings->GetTwoSiteRequests(
      ContentSettingsType::STORAGE_ACCESS));

  set_manage_text_style(ManageTextStyle::kHoverButton);
  set_manage_text(l10n_util::GetStringUTF16(IDS_STORAGE_ACCESS_MANAGE_TEXT));
  set_manage_tooltip(
      l10n_util::GetStringUTF16(IDS_STORAGE_ACCESS_MANAGE_TOOLTIP));
}

ContentSettingStorageAccessBubbleModel::
    ~ContentSettingStorageAccessBubbleModel() = default;

void ContentSettingStorageAccessBubbleModel::CommitChanges() {
  if (!GetProfile()) {
    return;
  }

  for (const auto& entry : changed_permissions_) {
    GURL primary = entry.first.GetURL();
    const GURL& secondary = web_contents()->GetURL();
    ContentSetting setting =
        entry.second ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
    permissions::PermissionUmaUtil::ScopedRevocationReporter
        scoped_revocation_reporter(
            GetProfile(), primary, secondary,
            ContentSettingsType::STORAGE_ACCESS,
            permissions::PermissionSourceUI::PAGE_ACTION);
    auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
    content_settings::ContentSettingConstraints constraints;
    constraints.set_lifetime(
        permissions::kStorageAccessAPIExplicitPermissionLifetime);
    map->SetNarrowestContentSetting(primary, secondary,
                                    ContentSettingsType::STORAGE_ACCESS,
                                    setting, constraints);
  }
}

void ContentSettingStorageAccessBubbleModel::OnSiteRowClicked(
    const net::SchemefulSite& site,
    bool is_allowed) {
  RecordActionHistogram(ContentSettingsType::STORAGE_ACCESS,
                        is_allowed
                            ? ContentSettingBubbleAction::kPermissionAllowed
                            : ContentSettingBubbleAction::kPermissionBlocked);
  changed_permissions_[site] = is_allowed;
}

void ContentSettingStorageAccessBubbleModel::OnManageButtonClicked() {
  if (!delegate()) {
    return;
  }

  RecordActionHistogram(ContentSettingsType::STORAGE_ACCESS,
                        ContentSettingBubbleAction::kManageButtonClicked);
  delegate()->ShowContentSettingsPage(ContentSettingsType::STORAGE_ACCESS);
}

// ContentSettingCookiesBubbleModel --------------------------------------------

class ContentSettingCookiesBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingCookiesBubbleModel(Delegate* delegate,
                                   WebContents* web_contents);

  ContentSettingCookiesBubbleModel(const ContentSettingCookiesBubbleModel&) =
      delete;
  ContentSettingCookiesBubbleModel& operator=(
      const ContentSettingCookiesBubbleModel&) = delete;

  ~ContentSettingCookiesBubbleModel() override;

  // ContentSettingBubbleModel:
  void CommitChanges() override;
  void OnManageButtonClicked() override;
};

ContentSettingCookiesBubbleModel::ContentSettingCookiesBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     ContentSettingsType::COOKIES) {}

ContentSettingCookiesBubbleModel::~ContentSettingCookiesBubbleModel() = default;

void ContentSettingCookiesBubbleModel::CommitChanges() {
  // On some plattforms e.g. MacOS X it is possible to close a tab while the
  // cookies settings bubble is open. This resets the web contents to NULL.
  if (settings_changed()) {
    CollectedCookiesInfoBarDelegate::Create(
        infobars::ContentInfoBarManager::FromWebContents(web_contents()));
  }
  ContentSettingSingleRadioGroup::CommitChanges();
}

void ContentSettingCookiesBubbleModel::OnManageButtonClicked() {
  delegate()->ShowCollectedCookiesDialog(web_contents());
}

// ContentSettingPopupBubbleModel ----------------------------------------------

class ContentSettingPopupBubbleModel
    : public ContentSettingSingleRadioGroup,
      public blocked_content::UrlListManager::Observer {
 public:
  ContentSettingPopupBubbleModel(Delegate* delegate, WebContents* web_contents);

  ContentSettingPopupBubbleModel(const ContentSettingPopupBubbleModel&) =
      delete;
  ContentSettingPopupBubbleModel& operator=(
      const ContentSettingPopupBubbleModel&) = delete;

  ~ContentSettingPopupBubbleModel() override;

  // ContentSettingBubbleModel:
  void CommitChanges() override;

  // PopupBlockerTabHelper::Observer:
  void BlockedUrlAdded(int32_t id, const GURL& url) override;

 private:
  void OnListItemClicked(int index, const ui::Event& event) override;

  int32_t item_id_from_item_index(int index) const {
    return bubble_content().list_items[index].item_id;
  }

  base::ScopedObservation<blocked_content::UrlListManager,
                          blocked_content::UrlListManager::Observer>
      url_list_observation_{this};
};

ContentSettingPopupBubbleModel::ContentSettingPopupBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     ContentSettingsType::POPUPS) {
  set_title(l10n_util::GetStringUTF16(IDS_BLOCKED_POPUPS_TITLE));

  // Build blocked popup list.
  auto* helper =
      blocked_content::PopupBlockerTabHelper::FromWebContents(web_contents);
  std::map<int32_t, GURL> blocked_popups = helper->GetBlockedPopupRequests();
  for (const auto& blocked_popup : blocked_popups)
    AddListItem(CreateUrlListItem(blocked_popup.first, blocked_popup.second));

  url_list_observation_.Observe(helper->manager());
  content_settings::RecordPopupsAction(
      content_settings::POPUPS_ACTION_DISPLAYED_BUBBLE);
}

void ContentSettingPopupBubbleModel::BlockedUrlAdded(int32_t id,
                                                     const GURL& url) {
  AddListItem(CreateUrlListItem(id, url));
}

void ContentSettingPopupBubbleModel::OnListItemClicked(int index,
                                                       const ui::Event& event) {
  auto* helper =
      blocked_content::PopupBlockerTabHelper::FromWebContents(web_contents());
  helper->ShowBlockedPopup(item_id_from_item_index(index),
                           ui::DispositionFromEventFlags(event.flags()));
  RemoveListItem(index);
  content_settings::RecordPopupsAction(
      content_settings::POPUPS_ACTION_CLICKED_LIST_ITEM_CLICKED);
}

void ContentSettingPopupBubbleModel::CommitChanges() {
  // User selected to always allow pop-ups from.
  if (settings_changed() && selected_item() == kAllowButtonIndex) {
    // Increases the counter.
    content_settings::RecordPopupsAction(
        content_settings::POPUPS_ACTION_SELECTED_ALWAYS_ALLOW_POPUPS_FROM);
  }
  ContentSettingSingleRadioGroup::CommitChanges();
}

ContentSettingPopupBubbleModel::~ContentSettingPopupBubbleModel() = default;

// ContentSettingMediaStreamBubbleModel ----------------------------------------

ContentSettingMediaStreamBubbleModel::ContentSettingMediaStreamBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents) {
  // TODO(msramek): The media bubble has three states - mic only, camera only,
  // and both. There is a lot of duplicated code which does the same thing
  // for camera and microphone separately. Consider refactoring it to avoid
  // duplication.

  // Initialize the content settings associated with the individual radio
  // buttons.
  radio_item_setting_[0] = CONTENT_SETTING_ASK;
  radio_item_setting_[1] = CONTENT_SETTING_BLOCK;

  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument());
  state_ = content_settings->GetMicrophoneCameraState();
  CHECK(CameraAccessed() || MicrophoneAccessed());

  if (CameraAccessed()) {
    content_settings->OnActivityIndicatorBubbleOpened(
        ContentSettingsType::MEDIASTREAM_CAMERA);
  }

  if (MicrophoneAccessed()) {
    content_settings->OnActivityIndicatorBubbleOpened(
        ContentSettingsType::MEDIASTREAM_MIC);
  }

  // If the permission is turned off in MacOS system preferences, overwrite
  // the bubble to enable the user to trigger the system dialog.
  if (ShouldShowSystemMediaPermissions()) {
#if BUILDFLAG(IS_MAC)
    InitializeSystemMediaPermissionBubble();
    return;
#endif  // BUILDFLAG(IS_MAC)
  }

  SetTitle();
  SetMessage();
  SetRadioGroup();
  SetManageText();
  SetCustomLink();
  SetIsUserModifiable();
}

ContentSettingMediaStreamBubbleModel::~ContentSettingMediaStreamBubbleModel() =
    default;

void ContentSettingMediaStreamBubbleModel::CommitChanges() {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument());

  if (CameraAccessed()) {
    content_settings->OnActivityIndicatorBubbleClosed(
        ContentSettingsType::MEDIASTREAM_CAMERA);
  }

  if (MicrophoneAccessed()) {
    content_settings->OnActivityIndicatorBubbleClosed(
        ContentSettingsType::MEDIASTREAM_MIC);
  }

  if (content_settings->media_stream_access_origin().is_empty()) {
    return;
  }

  // No need for radio group in the bubble UI shown when permission is blocked
  // on a system level.
  if (!ShouldShowSystemMediaPermissions()) {
    // Update the media settings if the radio button selection was changed.
    if (selected_item() != bubble_content().radio_group.default_item)
      UpdateSettings(radio_item_setting_[selected_item()]);
  }
}

ContentSettingMediaStreamBubbleModel*
ContentSettingMediaStreamBubbleModel::AsMediaStreamBubbleModel() {
  return this;
}

void ContentSettingMediaStreamBubbleModel::OnManageButtonClicked() {
  CHECK(CameraAccessed() || MicrophoneAccessed());
  if (!delegate())
    return;

  CommitChanges();

  if (MicrophoneAccessed() && CameraAccessed()) {
    delegate()->ShowMediaSettingsPage();
  } else {
    delegate()->ShowContentSettingsPage(
        CameraAccessed() ? ContentSettingsType::MEDIASTREAM_CAMERA
                         : ContentSettingsType::MEDIASTREAM_MIC);
  }
}

void ContentSettingMediaStreamBubbleModel::OnDoneButtonClicked() {
  if (ShouldShowSystemMediaPermissions()) {
#if BUILDFLAG(IS_MAC)
    DCHECK(CameraAccessed() || MicrophoneAccessed());

    base::RecordAction(UserMetricsAction("Media.OpenPreferencesClicked"));
    DCHECK(ShouldShowSystemMediaPermissions());

    if (CameraAccessed()) {
      base::mac::OpenSystemSettingsPane(
          base::mac::SystemSettingsPane::kPrivacySecurity_Camera);
    } else if (MicrophoneAccessed()) {
      base::mac::OpenSystemSettingsPane(
          base::mac::SystemSettingsPane::kPrivacySecurity_Microphone);
    }
    return;
#endif  // BUILDFLAG(IS_MAC)
  }
}

bool ContentSettingMediaStreamBubbleModel::MicrophoneAccessed() const {
  return state_.Has(PageSpecificContentSettings::kMicrophoneAccessed);
}

bool ContentSettingMediaStreamBubbleModel::CameraAccessed() const {
  return state_.Has(PageSpecificContentSettings::kCameraAccessed);
}

bool ContentSettingMediaStreamBubbleModel::MicrophoneBlocked() const {
  return state_.Has(PageSpecificContentSettings::kMicrophoneBlocked);
}

bool ContentSettingMediaStreamBubbleModel::CameraBlocked() const {
  return state_.Has(PageSpecificContentSettings::kCameraBlocked);
}

void ContentSettingMediaStreamBubbleModel::SetIsUserModifiable() {
  CHECK(CameraAccessed() || MicrophoneAccessed());
  PageSpecificContentSettings* page_content_settings =
      PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument());

  bool is_camera_modifiable = GetSettingManagedByUser(
      page_content_settings->media_stream_access_origin(),
      ContentSettingsType::MEDIASTREAM_CAMERA, GetProfile(), nullptr);
  bool is_mic_modifiable = GetSettingManagedByUser(
      page_content_settings->media_stream_access_origin(),
      ContentSettingsType::MEDIASTREAM_MIC, GetProfile(), nullptr);

  set_is_user_modifiable((MicrophoneAccessed() && is_mic_modifiable) ||
                         (CameraAccessed() && is_camera_modifiable));
}

void ContentSettingMediaStreamBubbleModel::SetTitle() {
  CHECK(CameraAccessed() || MicrophoneAccessed());
  int title_id = 0;
  if (MicrophoneBlocked() && CameraBlocked())
    title_id = IDS_MICROPHONE_CAMERA_BLOCKED_TITLE;
  else if (MicrophoneBlocked())
    title_id = IDS_MICROPHONE_BLOCKED_TITLE;
  else if (CameraBlocked())
    title_id = IDS_CAMERA_BLOCKED_TITLE;
  else if (MicrophoneAccessed() && CameraAccessed())
    title_id = IDS_MICROPHONE_CAMERA_ALLOWED_TITLE;
  else if (MicrophoneAccessed())
    title_id = IDS_MICROPHONE_ACCESSED_TITLE;
  else if (CameraAccessed())
    title_id = IDS_CAMERA_ACCESSED_TITLE;
  else
    NOTREACHED();
  set_title(l10n_util::GetStringUTF16(title_id));
}

void ContentSettingMediaStreamBubbleModel::SetMessage() {
  CHECK(CameraAccessed() || MicrophoneAccessed());
  int message_id = 0;
  if (MicrophoneBlocked() && CameraBlocked())
    message_id = IDS_MICROPHONE_CAMERA_BLOCKED;
  else if (MicrophoneBlocked())
    message_id = IDS_MICROPHONE_BLOCKED;
  else if (CameraBlocked())
    message_id = IDS_CAMERA_BLOCKED;
  else if (MicrophoneAccessed() && CameraAccessed())
    message_id = IDS_MICROPHONE_CAMERA_ALLOWED;
  else if (MicrophoneAccessed())
    message_id = IDS_MICROPHONE_ACCESSED;
  else if (CameraAccessed())
    message_id = IDS_CAMERA_ACCESSED;
  else
    NOTREACHED();
  set_message(l10n_util::GetStringUTF16(message_id));
}

void ContentSettingMediaStreamBubbleModel::SetRadioGroup() {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument());
  GURL url = content_settings->media_stream_access_origin();
  RadioGroup radio_group;
  radio_group.url = url;
  const std::u16string& display_url = GetUrlForDisplay(GetProfile(), url);

  CHECK(CameraAccessed() || MicrophoneAccessed());
  int radio_allow_label_id = 0;
  int radio_block_label_id = 0;
  if (state_.Has(PageSpecificContentSettings::kMicrophoneBlocked) ||
      state_.Has(PageSpecificContentSettings::kCameraBlocked)) {
    if (network::IsUrlPotentiallyTrustworthy(url)) {
      radio_item_setting_[0] = CONTENT_SETTING_ALLOW;
      radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_ALLOW;
      if (MicrophoneAccessed())
        radio_allow_label_id =
            CameraAccessed() ? IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_ALLOW
                             : IDS_BLOCKED_MEDIASTREAM_MIC_ALLOW;
    } else {
      radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_ASK;
      if (MicrophoneAccessed())
        radio_allow_label_id = CameraAccessed()
                                   ? IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_ASK
                                   : IDS_BLOCKED_MEDIASTREAM_MIC_ASK;
    }
    radio_block_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_NO_ACTION;
    if (MicrophoneAccessed())
      radio_block_label_id =
          CameraAccessed() ? IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION
                           : IDS_BLOCKED_MEDIASTREAM_MIC_NO_ACTION;
  } else {
    bool has_pan_tilt_zoom_permission_granted =
        web_contents()
            ->GetBrowserContext()
            ->GetPermissionController()
            ->GetPermissionStatusForCurrentDocument(
                blink::PermissionType::CAMERA_PAN_TILT_ZOOM,
                &GetPage().GetMainDocument()) ==
        blink::mojom::PermissionStatus::GRANTED;

    if (MicrophoneAccessed() && CameraAccessed()) {
      radio_allow_label_id =
          has_pan_tilt_zoom_permission_granted
              ? IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_PAN_TILT_ZOOM_NO_ACTION
              : IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_BLOCK;
    } else if (MicrophoneAccessed()) {
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_BLOCK;
    } else {
      radio_allow_label_id = has_pan_tilt_zoom_permission_granted
                                 ? IDS_ALLOWED_CAMERA_PAN_TILT_ZOOM_NO_ACTION
                                 : IDS_ALLOWED_MEDIASTREAM_CAMERA_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_CAMERA_BLOCK;
    }
  }

  std::u16string radio_allow_label =
      l10n_util::GetStringFUTF16(radio_allow_label_id, display_url);
  std::u16string radio_block_label =
      l10n_util::GetStringUTF16(radio_block_label_id);

  radio_group.default_item =
      (MicrophoneAccessed() && content_settings->IsContentBlocked(
                                   ContentSettingsType::MEDIASTREAM_MIC)) ||
              (CameraAccessed() && content_settings->IsContentBlocked(
                                       ContentSettingsType::MEDIASTREAM_CAMERA))
          ? 1
          : 0;
  radio_group.radio_items = {radio_allow_label, radio_block_label};
  set_radio_group(radio_group);
}

void ContentSettingMediaStreamBubbleModel::UpdateSettings(
    ContentSetting setting) {
  PageSpecificContentSettings* page_content_settings =
      PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument());
  // The same urls must be used as in other places (e.g. the infobar) in
  // order to override the existing rule. Otherwise a new rule is created.
  // TODO(markusheintz): Extract to a helper so that there is only a single
  // place to touch.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  if (MicrophoneAccessed()) {
    permissions::PermissionUmaUtil::ScopedRevocationReporter
        scoped_revocation_reporter(
            GetProfile(), page_content_settings->media_stream_access_origin(),
            GURL(), ContentSettingsType::MEDIASTREAM_MIC,
            permissions::PermissionSourceUI::PAGE_ACTION);
    map->SetContentSettingDefaultScope(
        page_content_settings->media_stream_access_origin(), GURL(),
        ContentSettingsType::MEDIASTREAM_MIC, setting);
  }
  if (CameraAccessed()) {
    permissions::PermissionUmaUtil::ScopedRevocationReporter
        scoped_revocation_reporter(
            GetProfile(), page_content_settings->media_stream_access_origin(),
            GURL(), ContentSettingsType::MEDIASTREAM_CAMERA,
            permissions::PermissionSourceUI::PAGE_ACTION);
    map->SetContentSettingDefaultScope(
        page_content_settings->media_stream_access_origin(), GURL(),
        ContentSettingsType::MEDIASTREAM_CAMERA, setting);
  }
}

#if BUILDFLAG(IS_MAC)
void ContentSettingMediaStreamBubbleModel::
    InitializeSystemMediaPermissionBubble() {
  DCHECK(CameraAccessed() || MicrophoneAccessed());
  base::RecordAction(
      base::UserMetricsAction("Media.ShowSystemMediaPermissionBubble"));
  int title_id = 0;
  if (MicrophoneAccessed() && CameraAccessed() &&
      (system_permission_settings::CheckSystemVideoCapturePermission() ==
           system_permission_settings::SystemPermission::kDenied ||
       system_permission_settings::CheckSystemAudioCapturePermission() ==
           system_permission_settings::SystemPermission::kDenied)) {
    title_id = IDS_CAMERA_MIC_TURNED_OFF_IN_MACOS;
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kVideocamIcon, l10n_util::GetStringUTF16(IDS_CAMERA),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 0));
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kMicIcon, l10n_util::GetStringUTF16(IDS_MIC),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 1));
  } else if (CameraAccessed() &&
             system_permission_settings::CheckSystemVideoCapturePermission() ==
                 system_permission_settings::SystemPermission::kDenied) {
    title_id = IDS_CAMERA_TURNED_OFF_IN_MACOS;
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kVideocamIcon, l10n_util::GetStringUTF16(IDS_CAMERA),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 0));
  } else if (MicrophoneAccessed() &&
             system_permission_settings::CheckSystemAudioCapturePermission() ==
                 system_permission_settings::SystemPermission::kDenied) {
    title_id = IDS_MIC_TURNED_OFF_IN_MACOS;
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kMicIcon, l10n_util::GetStringUTF16(IDS_MIC),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 1));
  }

  set_title(l10n_util::GetStringUTF16(title_id));
  set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kNone);
  SetCustomLink();
  set_done_button_text(l10n_util::GetStringUTF16(IDS_OPEN_SETTINGS_LINK));
}
#endif  // BUILDFLAG(IS_MAC)

bool ContentSettingMediaStreamBubbleModel::ShouldShowSystemMediaPermissions() {
#if BUILDFLAG(IS_MAC)
  return (((system_permission_settings::CheckSystemVideoCapturePermission() ==
                system_permission_settings::SystemPermission::kDenied &&
            CameraAccessed() && !CameraBlocked()) ||
           (system_permission_settings::CheckSystemAudioCapturePermission() ==
                system_permission_settings::SystemPermission::kDenied &&
            MicrophoneAccessed() && !MicrophoneBlocked())) &&
          !(CameraAccessed() && CameraBlocked()) &&
          !(MicrophoneAccessed() && MicrophoneBlocked()));
#else
  return false;
#endif  // BUILDFLAG(IS_MAC)
}

void ContentSettingMediaStreamBubbleModel::SetManageText() {
  CHECK(CameraAccessed() || MicrophoneAccessed());
  set_manage_text(l10n_util::GetStringUTF16(IDS_MANAGE));
}

void ContentSettingMediaStreamBubbleModel::SetCustomLink() {
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(&GetPage().GetMainDocument());
  if (content_settings->IsMicrophoneCameraStateChanged()) {
    set_custom_link(
        l10n_util::GetStringUTF16(IDS_MEDIASTREAM_SETTING_CHANGED_MESSAGE));
  }
}

// ContentSettingGeolocationBubbleModel --------------------------------------

ContentSettingGeolocationBubbleModel::ContentSettingGeolocationBubbleModel(
    Delegate* delegate,
    content::WebContents* web_contents)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     ContentSettingsType::GEOLOCATION) {
  SetCustomLink();
    // Get the stored geolocation content setting and the system permission
    // state to determine whether geolocation is blocked by a system permission.
    //
    // The content setting must be read from HostContentSettingsMap.
    // PageSpecificContentSettings cannot be used because it combines the
    // site-level and system-level permissions, indicating the feature is
    // blocked if either the site-level or system-level permission is not
    // granted. We need to distinguish these cases to ensure the bubble that
    // launches the system dialog is not shown if the site-level permission was
    // not granted.
    const GURL& url =
        web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
    ContentSetting content_setting =
        HostContentSettingsMapFactory::GetForProfile(GetProfile())
            ->GetContentSetting(url, url, ContentSettingsType::GEOLOCATION);
    if (content_setting == CONTENT_SETTING_ALLOW &&
        !system_permission_settings::IsAllowed(
            ContentSettingsType::GEOLOCATION)) {
      // If the permission is turned off in system preferences, overwrite the
      // bubble to enable the user to trigger the system dialog.
      InitializeSystemGeolocationPermissionBubble();
    }
}

ContentSettingGeolocationBubbleModel::~ContentSettingGeolocationBubbleModel() =
    default;

void ContentSettingGeolocationBubbleModel::OnDoneButtonClicked() {
  if (show_system_geolocation_bubble_) {
    base::RecordAction(UserMetricsAction(
        "ContentSettings.GeolocationDialog.OpenPreferencesClicked"));

    system_permission_settings::OpenSystemSettings(
        web_contents(), ContentSettingsType::GEOLOCATION);
  }
}

void ContentSettingGeolocationBubbleModel::OnManageButtonClicked() {
  if (delegate())
    delegate()->ShowContentSettingsPage(ContentSettingsType::GEOLOCATION);
}

void ContentSettingGeolocationBubbleModel::CommitChanges() {
  if (show_system_geolocation_bubble_)
    return;
  ContentSettingSingleRadioGroup::CommitChanges();
}

void ContentSettingGeolocationBubbleModel::
    InitializeSystemGeolocationPermissionBubble() {
#if BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  set_title(l10n_util::GetStringUTF16(IDS_GEOLOCATION_TURNED_OFF_IN_OS));
#else
    // The system-level location permission is not supported on Linux.
    NOTREACHED();
#endif

    clear_message();
    AddListItem(ContentSettingBubbleModel::ListItem(
        &vector_icons::kLocationOnIcon,
        l10n_util::GetStringUTF16(IDS_GEOLOCATION),
        l10n_util::GetStringUTF16(IDS_TURNED_OFF), false, true, 0));
    set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kNone);
    set_done_button_text(l10n_util::GetStringUTF16(IDS_OPEN_SETTINGS_LINK));
    set_radio_group(RadioGroup());
    show_system_geolocation_bubble_ = true;
#endif  // BUILDFLAG(OS_LEVEL_GEOLOCATION_PERMISSION_SUPPORTED)
}

void ContentSettingGeolocationBubbleModel::SetCustomLink() {
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      web_contents()->GetBrowserContext());
  SettingInfo info;
  const GURL url =
      GetPage().GetMainDocument().GetLastCommittedOrigin().GetURL();
  map->GetWebsiteSetting(url, url, ContentSettingsType::GEOLOCATION, &info);
  if (info.metadata.session_model() == SessionModel::ONE_TIME) {
    set_custom_link(l10n_util::GetStringUTF16(IDS_GEOLOCATION_WILL_ASK_AGAIN));
  }
}

// ContentSettingNotificationsBubbleModel ------------------------------------

#if BUILDFLAG(IS_MAC)
ContentSettingNotificationsBubbleModel::ContentSettingNotificationsBubbleModel(
    Delegate* delegate,
    content::WebContents* web_contents)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      ContentSettingsType::NOTIFICATIONS) {
  set_title(l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_TURNED_OFF_IN_MACOS));
  AddListItem(ContentSettingBubbleModel::ListItem(
      &vector_icons::kNotificationsOffChromeRefreshIcon,
      l10n_util::GetStringUTF16(IDS_NOTIFICATIONS),
      l10n_util::GetStringUTF16(IDS_TURNED_OFF), /*has_link=*/false,
      /*has_blocked_badge=*/false, 0));
  set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kNone);
  set_done_button_text(l10n_util::GetStringUTF16(IDS_OPEN_SETTINGS_LINK));
}

ContentSettingNotificationsBubbleModel::
    ~ContentSettingNotificationsBubbleModel() = default;

void ContentSettingNotificationsBubbleModel::OnDoneButtonClicked() {
  std::string bundle_identifier = base::apple::MainBundleIdentifier();
  if (std::optional<webapps::AppId> app_id =
          web_app::WebAppTabHelper::GetAppIdForNotificationAttribution(
              web_contents());
      app_id.has_value()) {
    bundle_identifier = web_app::GetBundleIdentifierForShim(*app_id);
  }
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kNotifications, bundle_identifier);
}
#endif

// ContentSettingSubresourceFilterBubbleModel ----------------------------------

ContentSettingSubresourceFilterBubbleModel::
    ContentSettingSubresourceFilterBubbleModel(Delegate* delegate,
                                               WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents) {
  SetTitle();
  SetMessage();
  SetManageText();
  set_done_button_text(l10n_util::GetStringUTF16(IDS_OK));
  set_show_learn_more(true);
  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kDetailsShown);
}

ContentSettingSubresourceFilterBubbleModel::
    ~ContentSettingSubresourceFilterBubbleModel() = default;

void ContentSettingSubresourceFilterBubbleModel::SetTitle() {
  set_title(l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_PROMPT_TITLE));
}

void ContentSettingSubresourceFilterBubbleModel::SetManageText() {
  set_manage_text(l10n_util::GetStringUTF16(IDS_ALWAYS_ALLOW_ADS));
  set_manage_text_style(ContentSettingBubbleModel::ManageTextStyle::kCheckbox);
}

void ContentSettingSubresourceFilterBubbleModel::SetMessage() {
  set_message(l10n_util::GetStringUTF16(IDS_BLOCKED_ADS_PROMPT_EXPLANATION));
}

void ContentSettingSubresourceFilterBubbleModel::OnManageCheckboxChecked(
    bool is_checked) {
  set_done_button_text(
      l10n_util::GetStringUTF16(is_checked ? IDS_APP_MENU_RELOAD : IDS_OK));
  is_checked_ = is_checked;
}

void ContentSettingSubresourceFilterBubbleModel::OnLearnMoreClicked() {
  DCHECK(delegate());
  subresource_filter::ContentSubresourceFilterThrottleManager::LogAction(
      subresource_filter::SubresourceFilterAction::kClickedLearnMore);
  delegate()->ShowLearnMorePage(ContentSettingsType::ADS);
}

void ContentSettingSubresourceFilterBubbleModel::CommitChanges() {
  if (is_checked_) {
    subresource_filter::ContentSubresourceFilterThrottleManager::FromPage(
        web_contents()->GetPrimaryPage())
        ->OnReloadRequested();
  }
}

ContentSettingSubresourceFilterBubbleModel*
ContentSettingSubresourceFilterBubbleModel::AsSubresourceFilterBubbleModel() {
  return this;
}

// ContentSettingDownloadsBubbleModel ------------------------------------------

ContentSettingDownloadsBubbleModel::ContentSettingDownloadsBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents) {
  SetTitle();
  SetManageText();
  SetRadioGroup();
  DownloadRequestLimiter* download_request_limiter =
      g_browser_process->download_request_limiter();
  set_is_user_modifiable(GetSettingManagedByUser(
      download_request_limiter->GetDownloadOrigin(web_contents),
      ContentSettingsType::AUTOMATIC_DOWNLOADS, GetProfile(), nullptr));
}

ContentSettingDownloadsBubbleModel::~ContentSettingDownloadsBubbleModel() =
    default;

void ContentSettingDownloadsBubbleModel::CommitChanges() {
  if (selected_item() != bubble_content().radio_group.default_item) {
    permissions::PermissionUmaUtil::ScopedRevocationReporter
        scoped_revocation_reporter(
            GetProfile(), bubble_content().radio_group.url, GURL(),
            ContentSettingsType::AUTOMATIC_DOWNLOADS,
            permissions::PermissionSourceUI::PAGE_ACTION);
    ContentSetting setting = selected_item() == kAllowButtonIndex
                                 ? CONTENT_SETTING_ALLOW
                                 : CONTENT_SETTING_BLOCK;
    auto* map = HostContentSettingsMapFactory::GetForProfile(GetProfile());
    map->SetNarrowestContentSetting(
        bubble_content().radio_group.url, bubble_content().radio_group.url,
        ContentSettingsType::AUTOMATIC_DOWNLOADS, setting);
  }
}

ContentSettingDownloadsBubbleModel*
ContentSettingDownloadsBubbleModel::AsDownloadsBubbleModel() {
  return this;
}

// Initialize the radio group by setting the appropriate labels for the
// content type and setting the default value based on the content setting.
void ContentSettingDownloadsBubbleModel::SetRadioGroup() {
  DownloadRequestLimiter* download_request_limiter =
      g_browser_process->download_request_limiter();
  const GURL& download_origin =
      download_request_limiter->GetDownloadOrigin(web_contents());

  const std::u16string& display_url =
      GetUrlForDisplay(GetProfile(), download_origin);

  DCHECK(download_request_limiter);

  RadioGroup radio_group;
  radio_group.url = download_origin;
  switch (download_request_limiter->GetDownloadUiStatus(web_contents())) {
    case DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED:
      radio_group.radio_items = {
          l10n_util::GetStringUTF16(IDS_ALLOWED_DOWNLOAD_NO_ACTION),
          l10n_util::GetStringFUTF16(IDS_ALLOWED_DOWNLOAD_BLOCK, display_url)};
      radio_group.default_item = kAllowButtonIndex;
      break;
    case DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED:
      radio_group.radio_items = {
          l10n_util::GetStringFUTF16(IDS_BLOCKED_DOWNLOAD_UNBLOCK, display_url),
          l10n_util::GetStringUTF16(IDS_BLOCKED_DOWNLOAD_NO_ACTION)};
      radio_group.default_item = 1;
      break;
    case DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT:
      DUMP_WILL_BE_NOTREACHED();
      return;
  }
  set_radio_group(radio_group);
}

void ContentSettingDownloadsBubbleModel::SetTitle() {
  DownloadRequestLimiter* download_request_limiter =
      g_browser_process->download_request_limiter();
  DCHECK(download_request_limiter);

  switch (download_request_limiter->GetDownloadUiStatus(web_contents())) {
    case DownloadRequestLimiter::DOWNLOAD_UI_ALLOWED:
      set_title(l10n_util::GetStringUTF16(IDS_ALLOWED_DOWNLOAD_TITLE));
      return;
    case DownloadRequestLimiter::DOWNLOAD_UI_BLOCKED:
      set_title(l10n_util::GetStringUTF16(IDS_BLOCKED_DOWNLOAD_TITLE));
      return;
    case DownloadRequestLimiter::DOWNLOAD_UI_DEFAULT:
      // No title otherwise.
      return;
  }
}

void ContentSettingDownloadsBubbleModel::SetManageText() {
  set_manage_text(l10n_util::GetStringUTF16(IDS_MANAGE));
}

void ContentSettingDownloadsBubbleModel::OnManageButtonClicked() {
  if (delegate())
    delegate()->ShowContentSettingsPage(
        ContentSettingsType::AUTOMATIC_DOWNLOADS);
}

// ContentSettingFramebustBlockBubbleModel -------------------------------------
ContentSettingFramebustBlockBubbleModel::
    ContentSettingFramebustBlockBubbleModel(Delegate* delegate,
                                            WebContents* web_contents)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     ContentSettingsType::POPUPS) {
  set_title(l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_MESSAGE));
  auto* helper = FramebustBlockTabHelper::FromWebContents(web_contents);

  // Build the blocked urls list.
  for (const auto& blocked_url : helper->blocked_urls())
    AddListItem(CreateUrlListItem(0 /* id */, blocked_url));

  url_list_observation_.Observe(helper->manager());
}

ContentSettingFramebustBlockBubbleModel::
    ~ContentSettingFramebustBlockBubbleModel() = default;

void ContentSettingFramebustBlockBubbleModel::OnListItemClicked(
    int index,
    const ui::Event& event) {
  FramebustBlockTabHelper::FromWebContents(web_contents())
      ->OnBlockedUrlClicked(index);
}

ContentSettingFramebustBlockBubbleModel*
ContentSettingFramebustBlockBubbleModel::AsFramebustBlockBubbleModel() {
  return this;
}

void ContentSettingFramebustBlockBubbleModel::BlockedUrlAdded(
    int32_t id,
    const GURL& blocked_url) {
  AddListItem(CreateUrlListItem(0 /* id */, blocked_url));
}

// ContentSettingQuietRequestBubbleModel ----------------------------------
ContentSettingQuietRequestBubbleModel::ContentSettingQuietRequestBubbleModel(
    Delegate* delegate,
    WebContents* web_contents)
    : ContentSettingBubbleModel(delegate, web_contents) {
  // TODO(crbug.com/40110076): This block is more defensive than it needs to be
  // because ContentSettingImageModelBrowserTest exercises it without setting up
  // the correct PermissionRequestManager state. Fix that.
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  auto quiet_ui_reason = manager->ReasonForUsingQuietUi();
  if (!quiet_ui_reason)
    return;
  CHECK_GT(manager->Requests().size(), 0u);
  DCHECK_EQ(manager->Requests().size(), 1u);
  const permissions::RequestType request_type =
      manager->Requests()[0]->request_type();
  int bubble_title_string_id = 0;
  switch (request_type) {
    case permissions::RequestType::kNotifications:
      bubble_title_string_id = IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_TITLE;
      break;
    case permissions::RequestType::kGeolocation:
      bubble_title_string_id = IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_TITLE;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  set_title(l10n_util::GetStringUTF16(bubble_title_string_id));
  switch (*quiet_ui_reason) {
    case QuietUiReason::kEnabledInPrefs:
      DCHECK(request_type == permissions::RequestType::kNotifications ||
             request_type == permissions::RequestType::kGeolocation);
      set_message(l10n_util::GetStringUTF16(
          request_type == permissions::RequestType::kNotifications
              ? IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_DESCRIPTION
              : IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_DESCRIPTION));
      set_done_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON));
      set_show_learn_more(false);
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.AnimatedIconClicked"));
      break;
    case QuietUiReason::kTriggeredByCrowdDeny:
      DCHECK_EQ(request_type, permissions::RequestType::kNotifications);
      set_message(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CROWD_DENY_DESCRIPTION));
      set_done_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON));
      set_show_learn_more(false);
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.StaticIconClicked"));
      break;
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
      DCHECK_EQ(request_type, permissions::RequestType::kNotifications);
      set_message(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ABUSIVE_DESCRIPTION));
      // TODO(crbug.com/40131070): It is rather confusing to have the `Cancel`
      // button allow the permission, but we want the primary to block.
      set_cancel_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_COMPACT_ALLOW_BUTTON));
      set_done_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CONTINUE_BLOCKING_BUTTON));
      set_show_learn_more(true);
      set_manage_text_style(ManageTextStyle::kNone);
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.StaticIconClicked"));
      break;
    case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
      DCHECK_EQ(request_type, permissions::RequestType::kNotifications);
      set_message(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_DISRUPTIVE_DESCRIPTION));
      set_cancel_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_COMPACT_ALLOW_BUTTON));
      set_done_button_text(l10n_util::GetStringUTF16(
          IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CONTINUE_BLOCKING_BUTTON));
      set_show_learn_more(true);
      set_manage_text_style(ManageTextStyle::kNone);
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.StaticIconClicked"));
      break;
    case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
    case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
      int bubble_message_string_id = 0;
      int bubble_done_button_string_id = 0;
      // -1 means `prompt_denies_count` should not be used.
      int prompt_denies_count = -1;
      int kMinimumHistoricalActions = 4;
      if (base::FeatureList::IsEnabled(
              permissions::features::kPermissionPredictionsV3)) {
        prompt_denies_count = 0;
        base::Time cutoff = base::Time::Now() - base::Days(90);

        permissions::PermissionActionsHistory* action_history =
            PermissionActionsHistoryFactory::GetForProfile(GetProfile());

        auto actions =
            action_history->GetHistory(cutoff, request_type,
                                       permissions::PermissionActionsHistory::
                                           EntryFilter::WANT_ALL_PROMPTS);
        for (const auto& entry : actions) {
          if (entry.action == permissions::PermissionAction::DENIED) {
            prompt_denies_count++;
          }
          if (prompt_denies_count == kMinimumHistoricalActions) {
            break;
          }
        }
      }

      switch (request_type) {
        case permissions::RequestType::kNotifications:
          if (prompt_denies_count != -1 &&
              prompt_denies_count < kMinimumHistoricalActions) {
            // We use the same string as Crowd Deny because a user has not
            // enough historic actions to make decisions based on it.
            bubble_message_string_id =
                IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_CROWD_DENY_DESCRIPTION;
          } else {
            bubble_message_string_id =
                IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_PREDICTION_SERVICE_DESCRIPTION;
          }
          bubble_done_button_string_id =
              IDS_NOTIFICATIONS_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON;
          break;
        case permissions::RequestType::kGeolocation:
          if (prompt_denies_count != -1 &&
              prompt_denies_count < kMinimumHistoricalActions) {
            // We use the same string as Crowd Deny because a user has not
            // enough historic actions to make decisions based on it.
            bubble_message_string_id =
                IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_CROWD_DENY_DESCRIPTION;
          } else {
            bubble_message_string_id =
                IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_PREDICTION_SERVICE_DESCRIPTION;
          }
          bubble_done_button_string_id =
              IDS_GEOLOCATION_QUIET_PERMISSION_BUBBLE_ALLOW_BUTTON;
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
      set_message(l10n_util::GetStringUTF16(bubble_message_string_id));
      set_done_button_text(
          l10n_util::GetStringUTF16(bubble_done_button_string_id));
      set_show_learn_more(false);
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.AnimatedIconClicked"));
      break;
  }
}

ContentSettingQuietRequestBubbleModel::
    ~ContentSettingQuietRequestBubbleModel() = default;

ContentSettingQuietRequestBubbleModel*
ContentSettingQuietRequestBubbleModel::AsQuietRequestBubbleModel() {
  return this;
}

void ContentSettingQuietRequestBubbleModel::OnManageButtonClicked() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  CHECK_GT(manager->Requests().size(), 0u);
  DCHECK_EQ(manager->Requests().size(), 1u);
  manager->set_manage_clicked();
  if (is_UMA_for_test) {
    // `delegate()->ShowContentSettingsPage` opens a new tab. It is not needed
    // for UMA tests.
    return;
  }
  const permissions::RequestType request_type =
      manager->Requests()[0]->request_type();
  if (delegate()) {
    switch (request_type) {
      case permissions::RequestType::kNotifications:
        delegate()->ShowContentSettingsPage(ContentSettingsType::NOTIFICATIONS);
        base::RecordAction(base::UserMetricsAction(
            "Permissions.Prompt.QuietBubble.Notifications.ManageClicked"));
        break;
      case permissions::RequestType::kGeolocation:
        delegate()->ShowContentSettingsPage(ContentSettingsType::GEOLOCATION);
        base::RecordAction(base::UserMetricsAction(
            "Permissions.Prompt.QuietBubble.Geolocation.ManageClicked"));
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
}

void ContentSettingQuietRequestBubbleModel::OnLearnMoreClicked() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  manager->set_learn_more_clicked();
  if (is_UMA_for_test) {
    // `delegate()->ShowLearnMorePage` opens a new tab. It is not needed for UMA
    // tests.
    return;
  }

  if (delegate()) {
    // We only show learn more button for Notification quiet ui dialog when it
    // is triggered due to abusive requests or contents. We don't have any learn
    // more button for the geolocation quiet ui dialogs.
    DCHECK_EQ(
        permissions::PermissionRequestManager::FromWebContents(web_contents())
            ->Requests()[0]
            ->request_type(),
        permissions::RequestType::kNotifications);
    delegate()->ShowLearnMorePage(ContentSettingsType::NOTIFICATIONS);
  }
}

void ContentSettingQuietRequestBubbleModel::OnDoneButtonClicked() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());
  CHECK_GT(manager->Requests().size(), 0u);
  DCHECK_EQ(manager->Requests().size(), 1u);
  auto quiet_ui_reason = manager->ReasonForUsingQuietUi();
  const permissions::RequestType request_type =
      manager->Requests()[0]->request_type();
  DCHECK(quiet_ui_reason);
  DCHECK(request_type == permissions::RequestType::kNotifications ||
         request_type == permissions::RequestType::kGeolocation);
  switch (*quiet_ui_reason) {
    case QuietUiReason::kEnabledInPrefs:
    case QuietUiReason::kTriggeredByCrowdDeny:
    case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
    case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
      manager->Accept();
      base::RecordAction(base::UserMetricsAction(
          request_type == permissions::RequestType::kNotifications
              ? "Permissions.Prompt.QuietBubble.Notifications.AllowClicked"
              : "Permissions.Prompt.QuietBubble.Geolocation.AllowClicked"));
      break;
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
    case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
      manager->Deny();
      base::RecordAction(base::UserMetricsAction(
          "Notifications.Quiet.ContinueBlockingClicked"));
      break;
  }
}

void ContentSettingQuietRequestBubbleModel::OnCancelButtonClicked() {
  permissions::PermissionRequestManager* manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents());

  auto quiet_ui_reason = manager->ReasonForUsingQuietUi();
  if (!quiet_ui_reason)
    return;
  switch (*quiet_ui_reason) {
    case QuietUiReason::kEnabledInPrefs:
    case QuietUiReason::kTriggeredByCrowdDeny:
    case QuietUiReason::kServicePredictedVeryUnlikelyGrant:
    case QuietUiReason::kOnDevicePredictedVeryUnlikelyGrant:
      // No-op.
      break;
    case QuietUiReason::kTriggeredDueToAbusiveRequests:
    case QuietUiReason::kTriggeredDueToAbusiveContent:
    case QuietUiReason::kTriggeredDueToDisruptiveBehavior:
      manager->Accept();
      base::RecordAction(
          base::UserMetricsAction("Notifications.Quiet.ShowForSiteClicked"));
      break;
  }
}

// ContentSettingBubbleModel ---------------------------------------------------

// This class must be placed last because it needs the definition of the other
// classes declared in this file.

const int ContentSettingBubbleModel::kAllowButtonIndex = 0;

// static
std::unique_ptr<ContentSettingBubbleModel>
ContentSettingBubbleModel::CreateContentSettingBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    ContentSettingsType content_type) {
  DCHECK(web_contents);
  switch (content_type) {
    case ContentSettingsType::COOKIES:
      return std::make_unique<ContentSettingCookiesBubbleModel>(delegate,
                                                                web_contents);
    case ContentSettingsType::POPUPS:
      return std::make_unique<ContentSettingPopupBubbleModel>(delegate,
                                                              web_contents);
    case ContentSettingsType::MIXEDSCRIPT:
      return std::make_unique<ContentSettingMixedScriptBubbleModel>(
          delegate, web_contents);
    case ContentSettingsType::PROTOCOL_HANDLERS: {
      custom_handlers::ProtocolHandlerRegistry* registry =
          ProtocolHandlerRegistryFactory::GetForBrowserContext(
              web_contents->GetBrowserContext());
      return std::make_unique<ContentSettingRPHBubbleModel>(
          delegate, web_contents, registry);
    }
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      return std::make_unique<ContentSettingDownloadsBubbleModel>(delegate,
                                                                  web_contents);
    case ContentSettingsType::ADS:
      return std::make_unique<ContentSettingSubresourceFilterBubbleModel>(
          delegate, web_contents);
    case ContentSettingsType::IMAGES:
    case ContentSettingsType::JAVASCRIPT:
    case ContentSettingsType::SOUND:
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
    case ContentSettingsType::MIDI_SYSEX:
    case ContentSettingsType::SENSORS:
      return std::make_unique<ContentSettingSingleRadioGroup>(
          delegate, web_contents, content_type);
    case ContentSettingsType::STORAGE_ACCESS:
      return std::make_unique<ContentSettingStorageAccessBubbleModel>(
          delegate, web_contents);
    default:
      NOTREACHED_IN_MIGRATION() << "No bubble for the content type "
                                << static_cast<int32_t>(content_type) << ".";
  }
  return nullptr;
}

ContentSettingBubbleModel::ContentSettingBubbleModel(Delegate* delegate,
                                                     WebContents* web_contents)
    : web_contents_(web_contents), owner_(nullptr), delegate_(delegate) {
  DCHECK(web_contents_);
}

ContentSettingBubbleModel::~ContentSettingBubbleModel() = default;

ContentSettingBubbleModel::RadioGroup::RadioGroup() = default;

ContentSettingBubbleModel::RadioGroup::~RadioGroup() = default;

ContentSettingBubbleModel::MediaMenu::MediaMenu() = default;

ContentSettingBubbleModel::MediaMenu::MediaMenu(const MediaMenu& other) =
    default;

ContentSettingBubbleModel::MediaMenu::~MediaMenu() = default;

ContentSettingBubbleModel::BubbleContent::BubbleContent() = default;

ContentSettingBubbleModel::BubbleContent::~BubbleContent() = default;

ContentSettingSimpleBubbleModel*
ContentSettingBubbleModel::AsSimpleBubbleModel() {
  // In general, bubble models might not inherit from the simple bubble model.
  return nullptr;
}

ContentSettingMediaStreamBubbleModel*
ContentSettingBubbleModel::AsMediaStreamBubbleModel() {
  // In general, bubble models might not inherit from the media bubble model.
  return nullptr;
}

ContentSettingQuietRequestBubbleModel*
ContentSettingBubbleModel::AsQuietRequestBubbleModel() {
  return nullptr;
}

ContentSettingSubresourceFilterBubbleModel*
ContentSettingBubbleModel::AsSubresourceFilterBubbleModel() {
  return nullptr;
}

ContentSettingDownloadsBubbleModel*
ContentSettingBubbleModel::AsDownloadsBubbleModel() {
  return nullptr;
}

ContentSettingFramebustBlockBubbleModel*
ContentSettingBubbleModel::AsFramebustBlockBubbleModel() {
  return nullptr;
}

Profile* ContentSettingBubbleModel::GetProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

void ContentSettingBubbleModel::AddListItem(const ListItem& item) {
  bubble_content_.list_items.push_back(item);
  if (owner_)
    owner_->OnListItemAdded(item);
}

void ContentSettingBubbleModel::RemoveListItem(int index) {
  if (owner_)
    owner_->OnListItemRemovedAt(index);

  bubble_content_.list_items.erase(bubble_content_.list_items.begin() + index);
}
