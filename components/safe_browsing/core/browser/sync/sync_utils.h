// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SYNC_UTILS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SYNC_UTILS_H_

namespace syncer {
class SyncService;
}

namespace signin {
class IdentityManager;
}

namespace safe_browsing {

// This class implements sync and signin-related safe browsing utilities.
class SyncUtils {
 public:
  SyncUtils() = delete;
  ~SyncUtils() = delete;

  // Returns true if signin and sync are configured such that access token
  // fetches for safe browsing can be enabled.
  static bool AreSigninAndSyncSetUpForSafeBrowsingTokenFetches(
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager,
      bool user_has_enabled_enhanced_protection);

  // Returns true iff history sync is enabled in |sync_service|. |sync_service|
  // may be null, in which case this method returns false.
  static bool IsHistorySyncEnabled(syncer::SyncService* sync_service);

  // Whether the primary account is signed in. Sync is not required.
  static bool IsPrimaryAccountSignedIn(
      signin::IdentityManager* identity_manager);
};  // class SyncUtils

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_SYNC_SYNC_UTILS_H_
