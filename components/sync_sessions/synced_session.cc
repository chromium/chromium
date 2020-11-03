// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/synced_session.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/sessions/core/serialized_navigation_driver.h"
#include "components/sync/base/time.h"
#include "ui/base/page_transition_types.h"

namespace sync_sessions {
namespace {

using sessions::SerializedNavigationEntry;

// The previous referrer policy value corresponding to |Never|.
// See original constant in serialized_navigation_entry.cc.
const int kObsoleteReferrerPolicyNever = 2;

sync_pb::SyncEnums_PageTransition ToSyncPageTransition(
    ui::PageTransition transition_type) {
  switch (ui::PageTransitionStripQualifier(transition_type)) {
    case ui::PAGE_TRANSITION_LINK:
      return sync_pb::SyncEnums_PageTransition_LINK;

    case ui::PAGE_TRANSITION_TYPED:
      return sync_pb::SyncEnums_PageTransition_TYPED;

    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      return sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK;

    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      return sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME;

    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      return sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME;

    case ui::PAGE_TRANSITION_GENERATED:
      return sync_pb::SyncEnums_PageTransition_GENERATED;

    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      return sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL;

    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      return sync_pb::SyncEnums_PageTransition_FORM_SUBMIT;

    case ui::PAGE_TRANSITION_RELOAD:
      return sync_pb::SyncEnums_PageTransition_RELOAD;

    case ui::PAGE_TRANSITION_KEYWORD:
      return sync_pb::SyncEnums_PageTransition_KEYWORD;

    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      return sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED;

    // Non-core values listed here although unreachable:
    case ui::PAGE_TRANSITION_CORE_MASK:
    case ui::PAGE_TRANSITION_BLOCKED:
    case ui::PAGE_TRANSITION_FORWARD_BACK:
    case ui::PAGE_TRANSITION_FROM_ADDRESS_BAR:
    case ui::PAGE_TRANSITION_HOME_PAGE:
    case ui::PAGE_TRANSITION_FROM_API:
    case ui::PAGE_TRANSITION_FROM_API_2:
    case ui::PAGE_TRANSITION_FROM_API_3:
    case ui::PAGE_TRANSITION_CHAIN_START:
    case ui::PAGE_TRANSITION_CHAIN_END:
    case ui::PAGE_TRANSITION_CLIENT_REDIRECT:
    case ui::PAGE_TRANSITION_SERVER_REDIRECT:
    case ui::PAGE_TRANSITION_IS_REDIRECT_MASK:
    case ui::PAGE_TRANSITION_QUALIFIER_MASK:
      break;
  }
  NOTREACHED();
  return sync_pb::SyncEnums_PageTransition_LINK;
}

ui::PageTransition FromSyncPageTransition(
    sync_pb::SyncEnums_PageTransition transition_type) {
  switch (transition_type) {
    case sync_pb::SyncEnums_PageTransition_LINK:
      return ui::PAGE_TRANSITION_LINK;

    case sync_pb::SyncEnums_PageTransition_TYPED:
      return ui::PAGE_TRANSITION_TYPED;

    case sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK:
      return ui::PAGE_TRANSITION_AUTO_BOOKMARK;

    case sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME:
      return ui::PAGE_TRANSITION_AUTO_SUBFRAME;

    case sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME:
      return ui::PAGE_TRANSITION_MANUAL_SUBFRAME;

    case sync_pb::SyncEnums_PageTransition_GENERATED:
      return ui::PAGE_TRANSITION_GENERATED;

    case sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL:
      return ui::PAGE_TRANSITION_AUTO_TOPLEVEL;

    case sync_pb::SyncEnums_PageTransition_FORM_SUBMIT:
      return ui::PAGE_TRANSITION_FORM_SUBMIT;

    case sync_pb::SyncEnums_PageTransition_RELOAD:
      return ui::PAGE_TRANSITION_RELOAD;

    case sync_pb::SyncEnums_PageTransition_KEYWORD:
      return ui::PAGE_TRANSITION_KEYWORD;

    case sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED:
      return ui::PAGE_TRANSITION_KEYWORD_GENERATED;
  }
  return ui::PAGE_TRANSITION_LINK;
}

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

  uint32_t transition = FromSyncPageTransition(sync_data.page_transition());

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
  if (sync_data.navigation_forward_back())
    transition |= ui::PAGE_TRANSITION_FORWARD_BACK;
  if (sync_data.navigation_from_address_bar())
    transition |= ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  if (sync_data.navigation_home_page())
    transition |= ui::PAGE_TRANSITION_HOME_PAGE;
  if (sync_data.navigation_chain_start())
    transition |= ui::PAGE_TRANSITION_CHAIN_START;
  if (sync_data.navigation_chain_end())
    transition |= ui::PAGE_TRANSITION_CHAIN_END;

  navigation.set_transition_type(static_cast<ui::PageTransition>(transition));

  navigation.set_timestamp(syncer::ProtoTimeToTime(sync_data.timestamp_msec()));
  if (sync_data.has_favicon_url())
    navigation.set_favicon_url(GURL(sync_data.favicon_url()));

  if (sync_data.has_password_state()) {
    navigation.set_password_state(
        static_cast<SerializedNavigationEntry::PasswordState>(
            sync_data.password_state()));
  }

  navigation.set_http_status_code(sync_data.http_status_code());

  if (sync_data.has_replaced_navigation()) {
    SerializedNavigationEntry::ReplacedNavigationEntryData replaced_entry_data;
    replaced_entry_data.first_committed_url =
        GURL(sync_data.replaced_navigation().first_committed_url());
    replaced_entry_data.first_timestamp = syncer::ProtoTimeToTime(
        sync_data.replaced_navigation().first_timestamp_msec());
    replaced_entry_data.first_transition_type = FromSyncPageTransition(
        sync_data.replaced_navigation().first_page_transition());
    navigation.set_replaced_entry_data(replaced_entry_data);
  }

  sessions::SerializedNavigationDriver::Get()->Sanitize(&navigation);

  navigation.set_is_restored(true);

  return navigation;
}

// TODO(zea): perhaps sync state (scroll position, form entries, etc.) as well?
// See http://crbug.com/67068.
sync_pb::TabNavigation SessionNavigationToSyncData(
    const SerializedNavigationEntry& navigation) {
  sync_pb::TabNavigation sync_data;
  sync_data.set_virtual_url(navigation.virtual_url().spec());
  sync_data.set_referrer(navigation.referrer_url().spec());
  sync_data.set_correct_referrer_policy(navigation.referrer_policy());
  sync_data.set_title(base::UTF16ToUTF8(navigation.title()));

  // Page transition core.
  static_assert(static_cast<int32_t>(ui::PAGE_TRANSITION_LAST_CORE) ==
                    static_cast<int32_t>(ui::PAGE_TRANSITION_KEYWORD_GENERATED),
                "PAGE_TRANSITION_LAST_CORE must equal "
                "PAGE_TRANSITION_KEYWORD_GENERATED");
  const ui::PageTransition transition_type = navigation.transition_type();
  sync_data.set_page_transition(ToSyncPageTransition(transition_type));

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
  sync_data.set_navigation_chain_start(
      (transition_type & ui::PAGE_TRANSITION_CHAIN_START) != 0);
  sync_data.set_navigation_chain_end(
      (transition_type & ui::PAGE_TRANSITION_CHAIN_END) != 0);

  sync_data.set_unique_id(navigation.unique_id());
  sync_data.set_timestamp_msec(syncer::TimeToProtoTime(navigation.timestamp()));
  // The full-resolution timestamp works as a global ID.
  sync_data.set_global_id(navigation.timestamp().ToInternalValue());

  sync_data.set_http_status_code(navigation.http_status_code());

  if (navigation.favicon_url().is_valid())
    sync_data.set_favicon_url(navigation.favicon_url().spec());

  if (navigation.blocked_state() != SerializedNavigationEntry::STATE_INVALID) {
    sync_data.set_blocked_state(
        static_cast<sync_pb::TabNavigation_BlockedState>(
            navigation.blocked_state()));
  }

  sync_data.set_password_state(
      static_cast<sync_pb::TabNavigation_PasswordState>(
          navigation.password_state()));

  for (const std::string& content_pack_category :
       navigation.content_pack_categories()) {
    sync_data.add_content_pack_categories(content_pack_category);
  }

  // Copy all redirect chain entries except the last URL (which should match
  // the virtual_url).
  const std::vector<GURL>& redirect_chain = navigation.redirect_chain();
  if (redirect_chain.size() > 1) {  // Single entry chains have no redirection.
    size_t last_entry = redirect_chain.size() - 1;
    for (size_t i = 0; i < last_entry; i++) {
      sync_pb::NavigationRedirect* navigation_redirect =
          sync_data.add_navigation_redirect();
      navigation_redirect->set_url(redirect_chain[i].spec());
    }
    // If the last URL didn't match the virtual_url, record it separately.
    if (sync_data.virtual_url() != redirect_chain[last_entry].spec()) {
      sync_data.set_last_navigation_redirect_url(
          redirect_chain[last_entry].spec());
    }
  }

  const base::Optional<SerializedNavigationEntry::ReplacedNavigationEntryData>&
      replaced_entry_data = navigation.replaced_entry_data();
  if (replaced_entry_data.has_value()) {
    sync_pb::ReplacedNavigation* replaced_navigation =
        sync_data.mutable_replaced_navigation();
    replaced_navigation->set_first_committed_url(
        replaced_entry_data->first_committed_url.spec());
    replaced_navigation->set_first_timestamp_msec(
        syncer::TimeToProtoTime(replaced_entry_data->first_timestamp));
    replaced_navigation->set_first_page_transition(
        ToSyncPageTransition(replaced_entry_data->first_transition_type));
  }

  sync_data.set_is_restored(navigation.is_restored());

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
  tab->navigations.clear();
  for (int i = 0; i < sync_data.navigation_size(); ++i) {
    tab->navigations.push_back(
        SessionNavigationFromSyncData(i, sync_data.navigation(i)));
  }
  tab->session_storage_persistent_id.clear();
}

sync_pb::SessionTab SessionTabToSyncData(const sessions::SessionTab& tab) {
  sync_pb::SessionTab sync_data;
  sync_data.set_tab_id(tab.tab_id.id());
  sync_data.set_window_id(tab.window_id.id());
  sync_data.set_tab_visual_index(tab.tab_visual_index);
  sync_data.set_current_navigation_index(tab.current_navigation_index);
  sync_data.set_pinned(tab.pinned);
  sync_data.set_extension_app_id(tab.extension_app_id);
  for (const SerializedNavigationEntry& navigation : tab.navigations) {
    SessionNavigationToSyncData(navigation).Swap(sync_data.add_navigation());
  }
  return sync_data;
}

SyncedSessionWindow::SyncedSessionWindow() {}

SyncedSessionWindow::~SyncedSessionWindow() {}

sync_pb::SessionWindow SyncedSessionWindow::ToSessionWindowProto() const {
  sync_pb::SessionWindow sync_data;
  sync_data.set_browser_type(window_type);
  sync_data.set_window_id(wrapped_window.window_id.id());
  sync_data.set_selected_tab_index(wrapped_window.selected_tab_index);

  for (const auto& tab : wrapped_window.tabs)
    sync_data.add_tab(tab->tab_id.id());

  return sync_data;
}

SyncedSession::SyncedSession()
    : session_tag("invalid"), device_type(sync_pb::SyncEnums::TYPE_UNSET) {}

SyncedSession::~SyncedSession() {}

sync_pb::SessionHeader SyncedSession::ToSessionHeaderProto() const {
  sync_pb::SessionHeader header;
  for (const auto& window_pair : windows) {
    sync_pb::SessionWindow* w = header.add_window();
    w->CopyFrom(window_pair.second->ToSessionWindowProto());
  }
  header.set_client_name(session_name);
  header.set_device_type(device_type);
  return header;
}

}  // namespace sync_sessions
