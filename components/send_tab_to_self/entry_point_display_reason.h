// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_ENTRY_POINT_DISPLAY_REASON_H_
#define COMPONENTS_SEND_TAB_TO_SELF_ENTRY_POINT_DISPLAY_REASON_H_

#include <optional>

class GURL;
class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace send_tab_to_self {

class SendTabToSelfModel;

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.share.send_tab_to_self)
enum class EntryPointDisplayReason {
  // The send-tab-to-self entry point should be shown because all the conditions
  // are met and the feature is ready to be used.
  kOfferFeature,
  // The user might be able to use send-tab-to-self if they sign in, so offer
  // that. "Might" because the list of target devices can't be known yet, it
  // could be empty (see below).
  kOfferSignIn,
  // All the conditions for send-tab-to-self are met, but there is no valid
  // target device. In that case the entry point should inform the user they
  // can enjoy the feature by signing in on other devices.
  kInformNoTargetDevice,
};

namespace internal {

// Use SendTabToSelfService::GetEntryPointDisplayReason() instead.
// `sync_service` and `send_tab_to_self_sync_model` can be null and that's
// handled as if send-tab-to-self or the sync backbone aren't working, so the
// entry point should be hidden (returns nullopt).
std::optional<EntryPointDisplayReason> GetEntryPointDisplayReason(
    const GURL& url_to_share,
    syncer::SyncService* sync_service,
    SendTabToSelfModel* send_tab_to_self_model,
    PrefService* pref_service);

}  // namespace internal

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_ENTRY_POINT_DISPLAY_REASON_H_
