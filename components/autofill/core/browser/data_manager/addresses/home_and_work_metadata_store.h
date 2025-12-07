// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_HOME_AND_WORK_METADATA_STORE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_HOME_AND_WORK_METADATA_STORE_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace syncer {
class SyncService;
}

namespace autofill {

class AutofillProfile;

// Home and Work profiles are downloaded through CONTACT_INFO but can currently
// not be uploaded there. As a result, syncing of metadata (e.g. use counts) is
// not possible like it is for regular account profiles.
// To support some kind of metadata sync for Home and Work, this class syncs the
// most important metadata through prefs. This is an awful workaround and to be
// removed once Home and Work metadata can be uploaded through CONTACT_INFO.
//
// The following metadata is supported:
// - use count
// - last use timestamp
// - A timestamp, indicating the modification time of the address when the user
//   removed it from Chrome. Once the address is modified outside of Chrome, it
//   will reappear.
//   Note that storing the modification timestamp instead of the time of removal
//   is preferred to avoid issues with incorrect system times.
//
// Ideally, all this logic would be integrated in the CONTACT_INFO sync bridge,
// as close to the sync support for non Home and Work addresses. However, since
// the bridge lives on a background thread and since writing prefs is only
// supported on the UI thread, it is integrated into the AddressDataManager for
// simplicity:
// - When profiles are loaded from the database, metadata changes are applied on
//   top of the result from the database.
// - When profiles are updated locally through the data manager, prefs are
//   updated.
// - When profiles are updated remotely, profiles are reloaded from the database
//   to apply any metadata changes.
class HomeAndWorkMetadataStore : public syncer::SyncServiceObserver {
 public:
  // `on_change` is called whenever the result of `ApplyMetadata()` changes,
  // for example because pref updates through sync were received.
  HomeAndWorkMetadataStore(PrefService* pref_service,
                           syncer::SyncService* sync_service,
                           base::RepeatingClosure on_change);
  ~HomeAndWorkMetadataStore() override;

  // Applies any metadata stored to all Home and Work profiles in `profiles`.
  // If the address was removed from Chrome, it is dropped.
  // Non Home and Work profiles are returned unmodified.
  // Conceptually const, but during the initial integration, this populates H/W
  // metadata with default values.
  std::vector<AutofillProfile> ApplyMetadata(
      std::vector<AutofillProfile> profiles,
      bool is_initial_load);

  // Persists the `change` in prefs, if it applies to a Home and Work profile.
  void ApplyChange(const AutofillProfileChange& change);

  // Metadata around address substructure learned through silent updates are
  // synced for non-H/W addresses. For H/W, they are only kept locally and not
  // synced through this class. In order to understand how much utility is lost
  // metrics around silent updates and the usage of silently updated profiles
  // are tracked.
  void RecordSilentUpdate(const AutofillProfile& profile);
  void RecordProfileFill(const AutofillProfile& profile) const;

 private:
  // Applies metadata to a single profile, returning the modified profile.
  // If the profile was removed from Chrome, nullopt is returned.
  // If there is no metadata for the profile, it is initialized to defaults,
  // ranking H/W above other addresses based on `max_use_count`.
  std::optional<AutofillProfile> ApplyMetadata(AutofillProfile profiles,
                                               int max_use_count);

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync_service) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar change_registrar_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observer_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_ADDRESSES_HOME_AND_WORK_METADATA_STORE_H_
