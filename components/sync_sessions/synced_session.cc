// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/serialized_navigation_driver.h"
#include "components/sync/base/features.h"
#include "components/sync/base/page_transition_conversion.h"
#include "components/sync/base/time.h"
#include "components/sync_device_info/device_info_proto_enum_util.h"
#include "ui/base/page_transition_types.h"

namespace sync_sessions {
namespace {

using sessions::SerializedNavigationEntry;

// The previous referrer policy value corresponding to |Never|.
// See original constant in serialized_navigation_entry.cc.
const int kObsoleteReferrerPolicyNever = 2;

// Some pages embed the favicon image itself in the URL, using the data: scheme.
// These cases, or more generally any favicon URL that is unreasonably large,
// should simply be ignored, because it otherwise runs into the risk that the
// entire tab may fail to sync due to max size limits imposed by the sync
// server. And after all, the favicon is somewhat optional.
const int kMaxFaviconUrlSizeToSync = 2048;

}  // namespace

SerializedNavigationEntry SessionNavigationFromSyncData(
    int index,
    const sync_pb::TabNavigation& sync_data) {
  SerializedNavigationEntry navigation;
  navigation.set_index(index);
  navigation.set_unique_id(sync_data.unique_id());
  if (sync_data.has_correct_referrer_policy()) {
    navigation.set_referrer_url(GURL(sync_data.referrer()));
    navigation.set_referrer_policy(sync_data.correct_referrer_policy());
  } else {
    navigation.set_referrer_url(GURL());
    navigation.set_referrer_policy(kObsoleteReferrerPolicyNever);
  }
  navigation.set_virtual_url(GURL(sync_data.virtual_url()));
  navigation.set_title(base::UTF8ToUTF16(sync_data.title()));

  uint32_t transition =
      syncer::FromSyncPageTransition(sync_data.page_transition());

  if (sync_data.has_redirect_type()) {
    switch (sync_data.redirect_type()) {
      case sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT:
        transition |= ui::PAGE_TRANSITION_CLIENT_REDIRECT;
        break;
      case sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT:
        transition |= ui::PAGE_TRANSITION_SERVER_REDIRECT;
        break;
    }
  }
  if (sync_data.navigation_forward_back()) {
    transition |= ui::PAGE_TRANSITION_FORWARD_BACK;
  }
  if (sync_data.navigation_from_address_bar()) {
    transition |= ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  }
  if (sync_data.navigation_home_page()) {
    transition |= ui::PAGE_TRANSITION_HOME_PAGE;
  }

  navigation.set_transition_type(static_cast<ui::PageTransition>(transition));

  navigation.set_timestamp(syncer::ProtoTimeToTime(sync_data.timestamp_msec()));
  if (sync_data.has_favicon_url()) {
    navigation.set_favicon_url(GURL(sync_data.favicon_url()));
  }

  if (sync_data.has_password_state()) {
    navigation.set_password_state(
        static_cast<SerializedNavigationEntry::PasswordState>(
            sync_data.password_state()));
  }

  navigation.set_http_status_code(sync_data.http_status_code());

  sessions::SerializedNavigationDriver::Get()->Sanitize(&navigation);

  navigation.set_is_restored(true);

  return navigation;
}

sync_pb::TabNavigation SessionNavigationToSyncData(
    const SerializedNavigationEntry& navigation) {
  sync_pb::TabNavigation sync_data;
  sync_data.set_virtual_url(navigation.virtual_url().spec());
  sync_data.set_referrer(navigation.referrer_url().spec());
  sync_data.set_correct_referrer_policy(navigation.referrer_policy());
  sync_data.set_title(base::UTF16ToUTF8(navigation.title()));

  // Page transition core.
  const ui::PageTransition transition_type = navigation.transition_type();
  sync_data.set_page_transition(syncer::ToSyncPageTransition(transition_type));

  // Page transition qualifiers.
  if (ui::PageTransitionIsRedirect(transition_type)) {
    if (transition_type & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
      sync_data.set_redirect_type(
          sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT);
    } else if (transition_type & ui::PAGE_TRANSITION_SERVER_REDIRECT) {
      sync_data.set_redirect_type(
          sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT);
    }
  }
  sync_data.set_navigation_forward_back(
      (transition_type & ui::PAGE_TRANSITION_FORWARD_BACK) != 0);
  sync_data.set_navigation_from_address_bar(
      (transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0);
  sync_data.set_navigation_home_page(
      (transition_type & ui::PAGE_TRANSITION_HOME_PAGE) != 0);

  sync_data.set_unique_id(navigation.unique_id());
  sync_data.set_timestamp_msec(syncer::TimeToProtoTime(navigation.timestamp()));
  // The full-resolution timestamp works as a global ID.
  sync_data.set_global_id(navigation.timestamp().ToInternalValue());

  sync_data.set_http_status_code(navigation.http_status_code());

  if (navigation.favicon_url().is_valid() &&
      navigation.favicon_url().spec().size() <= kMaxFaviconUrlSizeToSync) {
    sync_data.set_favicon_url(navigation.favicon_url().spec());
  }

  sync_data.set_password_state(static_cast<sync_pb::SyncEnums_PasswordState>(
      navigation.password_state()));

  return sync_data;
}

void SetSessionTabFromSyncData(const sync_pb::SessionTab& sync_data,
                               base::Time timestamp,
                               sessions::SessionTab* tab) {
  DCHECK(tab);
  tab->window_id = SessionID::FromSerializedValue(sync_data.window_id());
  tab->tab_id = SessionID::FromSerializedValue(sync_data.tab_id());
  tab->tab_visual_index = sync_data.tab_visual_index();
  tab->current_navigation_index = sync_data.current_navigation_index();
  tab->pinned = sync_data.pinned();
  tab->extension_app_id = sync_data.extension_app_id();
  tab->user_agent_override = sessions::SerializedUserAgentOverride();
  tab->timestamp = timestamp;
  tab->last_active_time =
      base::Time::UnixEpoch() +
      base::Milliseconds(sync_data.last_active_time_unix_epoch_millis());
  tab->navigations.clear();
  tab->navigations.reserve(sync_data.navigation_size());
  for (int i = 0; i < sync_data.navigation_size(); ++i) {
    tab->navigations.push_back(
        SessionNavigationFromSyncData(i, sync_data.navigation(i)));
  }
  tab->session_storage_persistent_id.clear();
}

sync_pb::SessionTab SessionTabToSyncData(
    const sessions::SessionTab& tab,
    std::optional<sync_pb::SyncEnums::BrowserType> browser_type) {
  sync_pb::SessionTab sync_data;
  sync_data.set_tab_id(tab.tab_id.id());
  sync_data.set_window_id(tab.window_id.id());
  sync_data.set_tab_visual_index(tab.tab_visual_index);
  sync_data.set_current_navigation_index(tab.current_navigation_index);
  sync_data.set_pinned(tab.pinned);
  sync_data.set_extension_app_id(tab.extension_app_id);
  sync_data.set_last_active_time_unix_epoch_millis(
      (tab.last_active_time - base::Time::UnixEpoch()).InMilliseconds());
  for (const SerializedNavigationEntry& navigation : tab.navigations) {
    SessionNavigationToSyncData(navigation).Swap(sync_data.add_navigation());
  }
  if (browser_type.has_value()) {
    sync_data.set_browser_type(*browser_type);
  }
  return sync_data;
}

SyncedSessionWindow::SyncedSessionWindow() = default;

SyncedSessionWindow::~SyncedSessionWindow() = default;

sync_pb::SessionWindow SyncedSessionWindow::ToSessionWindowProto() const {
  sync_pb::SessionWindow sync_data;
  sync_data.set_browser_type(window_type);
  sync_data.set_window_id(wrapped_window.window_id.id());
  sync_data.set_selected_tab_index(wrapped_window.selected_tab_index);

  for (const auto& tab : wrapped_window.tabs) {
    sync_data.add_tab(tab->tab_id.id());
  }

  return sync_data;
}

SyncedSession::SyncedSession()
    : session_tag_("invalid"), device_type(sync_pb::SyncEnums::TYPE_UNSET) {}

SyncedSession::~SyncedSession() = default;

void SyncedSession::SetSessionTag(const std::string& session_tag) {
  session_tag_ = session_tag;
}

const std::string& SyncedSession::GetSessionTag() const {
  return session_tag_;
}

void SyncedSession::SetSessionName(const std::string& session_name) {
  session_name_ = session_name;
}

const std::string& SyncedSession::GetSessionName() const {
  return session_name_;
}

void SyncedSession::SetStartTime(base::Time start_time) {
  start_time_ = start_time;
}

std::optional<base::Time> SyncedSession::GetStartTime() const {
  return start_time_;
}

void SyncedSession::SetModifiedTime(const base::Time& modified_time) {
  modified_time_ = modified_time;
}

const base::Time& SyncedSession::GetModifiedTime() const {
  return modified_time_;
}

void SyncedSession::SetDeviceTypeAndFormFactor(
    const sync_pb::SyncEnums::DeviceType& local_device_type,
    const syncer::DeviceInfo::FormFactor& local_device_form_factor) {
  device_type = local_device_type;
  device_form_factor = local_device_form_factor;
}

syncer::DeviceInfo::FormFactor SyncedSession::GetDeviceFormFactor() const {
  return device_form_factor;
}

sync_pb::SessionHeader SyncedSession::ToSessionHeaderProto() const {
  sync_pb::SessionHeader header;
  for (const auto& [window_id, window] : windows) {
    sync_pb::SessionWindow* w = header.add_window();
    w->CopyFrom(window->ToSessionWindowProto());
  }
  if (start_time_) {
    header.set_session_start_time_unix_epoch_millis(
        start_time_->InMillisecondsSinceUnixEpoch());
  }
  header.set_client_name(session_name_);
  header.set_device_type(device_type);
  header.set_device_form_factor(ToDeviceFormFactorProto(device_form_factor));
  return header;
}

}  // namespace sync_sessions
