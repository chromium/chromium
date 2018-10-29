// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_profile_comparator.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/form_group.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/webdata/common/web_database.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::UTF16ToUTF8;

namespace autofill {

namespace {

std::string LimitData(const std::string& data) {
  std::string sanitized_value(data);
  if (sanitized_value.length() > AutofillTable::kMaxDataLength)
    sanitized_value.resize(AutofillTable::kMaxDataLength);
  return sanitized_value;
}

void* AutofillProfileSyncableServiceUserDataKey() {
  // Use the address of a static that COMDAT folding won't ever fold
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

}  // namespace

const char kAutofillProfileTag[] = "google_chrome_autofill_profiles";

AutofillProfileSyncableService::AutofillProfileSyncableService(
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale)
    : webdata_backend_(webdata_backend),
      app_locale_(app_locale),
      scoped_observer_(this) {
  DCHECK(webdata_backend_);

  scoped_observer_.Add(webdata_backend_);
}

AutofillProfileSyncableService::~AutofillProfileSyncableService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void AutofillProfileSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale) {
  web_data_service->GetDBUserData()->SetUserData(
      AutofillProfileSyncableServiceUserDataKey(),
      base::WrapUnique(
          new AutofillProfileSyncableService(webdata_backend, app_locale)));
}

// static
AutofillProfileSyncableService*
AutofillProfileSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillProfileSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(
          AutofillProfileSyncableServiceUserDataKey()));
}

AutofillProfileSyncableService::AutofillProfileSyncableService()
    : webdata_backend_(nullptr), scoped_observer_(this) {}

syncer::SyncMergeResult
AutofillProfileSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!sync_processor_);
  DCHECK(sync_processor);
  DCHECK(sync_error_factory);
  DVLOG(1) << "Associating Autofill: MergeDataAndStartSyncing";

  syncer::SyncMergeResult merge_result(type);
  sync_error_factory_ = std::move(sync_error_factory);
  if (!LoadAutofillData(&profiles_)) {
    merge_result.set_error(sync_error_factory_->CreateAndUploadError(
        FROM_HERE, "Could not get the autofill data from WebDatabase."));
    return merge_result;
  }

  if (DLOG_IS_ON(INFO)) {
    DVLOG(2) << "[AUTOFILL MIGRATION]"
             << "Printing profiles from web db";

    for (const auto& p : profiles_) {
      DVLOG(2) << "[AUTOFILL MIGRATION]  "
               << UTF16ToUTF8(p->GetRawInfo(NAME_FIRST))
               << UTF16ToUTF8(p->GetRawInfo(NAME_LAST))
               << p->guid();
    }
  }

  sync_processor_ = std::move(sync_processor);

  GUIDToProfileMap remaining_local_profiles;
  CreateGUIDToProfileMap(profiles_, &remaining_local_profiles);
  DataBundle bundle;
  // For every incoming profile from sync, attempt to update a local profile or
  // otherwise create a new one.
  for (const auto& sync_iter : initial_sync_data) {
    auto it =
        CreateOrUpdateProfile(sync_iter, &remaining_local_profiles, &bundle);
    // |it| points to created/updated profile. Add it to the |profiles_map_| and
    // then remove it from |remaining_local_profiles|. After this loop is
    // completed |remaining_local_profiles| will have only those profiles that
    // are not in the sync.
    profiles_map_[it->first] = it->second;
    // This may be a no-op since |it| is sometimes an entirely new profile that
    // came from sync.
    remaining_local_profiles.erase(it);
  }

  // Check for similar unmatched profiles - they are created independently on
  // two systems, so merge them.
  for (const auto& sync_profile_it : bundle.candidates_to_merge) {
    auto profile_to_merge =
        remaining_local_profiles.find(sync_profile_it.first);
    if (profile_to_merge != remaining_local_profiles.end()) {
      bundle.profiles_to_delete.push_back(profile_to_merge->second->guid());
      // For similar profile pairs, the local profile is always removed and its
      // content merged (if applicable) in the profile that came from sync.
      if (MergeSimilarProfiles(*(profile_to_merge->second),
                               sync_profile_it.second, app_locale_)) {
        // if new changes were merged into |sync_profile_it.second| from
        // |profile_to_merge|, they will be synced back.
        bundle.profiles_to_sync_back.push_back(sync_profile_it.second);
      }
      DVLOG(2) << "[AUTOFILL SYNC]"
               << "Found similar profile in sync db but with a "
                  "different guid: "
               << UTF16ToUTF8(sync_profile_it.second->GetRawInfo(NAME_FIRST))
               << UTF16ToUTF8(sync_profile_it.second->GetRawInfo(NAME_LAST))
               << "New guid " << sync_profile_it.second->guid()
               << ". Profile to be deleted "
               << profile_to_merge->second->guid();
      remaining_local_profiles.erase(profile_to_merge);
    }
  }

  if (!SaveChangesToWebData(bundle)) {
    merge_result.set_error(sync_error_factory_->CreateAndUploadError(
        FROM_HERE,
        "Failed to update webdata."));
    return merge_result;
  }

  syncer::SyncChangeList new_changes;
  for (const auto& it : remaining_local_profiles) {
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           CreateData(*(it.second))));
    profiles_map_[it.first] = it.second;
  }

  for (size_t i = 0; i < bundle.profiles_to_sync_back.size(); ++i) {
    new_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_UPDATE,
                           CreateData(*(bundle.profiles_to_sync_back[i]))));
  }

  if (!new_changes.empty()) {
    merge_result.set_error(
        sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes));
  }

  if (webdata_backend_) {
    webdata_backend_->NotifyOfMultipleAutofillChanges();
    webdata_backend_->NotifyThatSyncHasStarted(type);
  }

  return merge_result;
}

void AutofillProfileSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(type, syncer::AUTOFILL_PROFILE);

  sync_processor_.reset();
  sync_error_factory_.reset();
  profiles_.clear();
  profiles_map_.clear();
}

syncer::SyncDataList AutofillProfileSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sync_processor_);
  DCHECK_EQ(type, syncer::AUTOFILL_PROFILE);

  syncer::SyncDataList current_data;
  for (const auto& it : profiles_map_)
    current_data.push_back(CreateData(*(it.second)));
  return current_data;
}

syncer::SyncError AutofillProfileSyncableService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!sync_processor_) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Models not yet associated.",
                            syncer::AUTOFILL_PROFILE);
    return error;
  }

  DataBundle bundle;

  for (const auto& it : change_list) {
    DCHECK(it.IsValid());
    switch (it.change_type()) {
      case syncer::SyncChange::ACTION_ADD:
      case syncer::SyncChange::ACTION_UPDATE:
        CreateOrUpdateProfile(it.sync_data(), &profiles_map_, &bundle);
        break;
      case syncer::SyncChange::ACTION_DELETE: {
        std::string guid = it.sync_data().GetSpecifics().
             autofill_profile().guid();
        bundle.profiles_to_delete.push_back(guid);
        profiles_map_.erase(guid);
      } break;
      default:
        NOTREACHED() << "Unexpected sync change state.";
        return sync_error_factory_->CreateAndUploadError(
              FROM_HERE,
              "ProcessSyncChanges failed on ChangeType " +
                  syncer::SyncChange::ChangeTypeToString(it.change_type()));
    }
  }

  if (!SaveChangesToWebData(bundle)) {
    return sync_error_factory_->CreateAndUploadError(
        FROM_HERE,
        "Failed to update webdata.");
  }

  if (webdata_backend_)
    webdata_backend_->NotifyOfMultipleAutofillChanges();

  return syncer::SyncError();
}

void AutofillProfileSyncableService::AutofillProfileChanged(
    const AutofillProfileChange& change) {
  // Check if sync is on. If we receive notification prior to the sync being set
  // up we are going to process all when MergeData..() is called. If we receive
  // notification after the sync exited, it will be sinced next time Chrome
  // starts.
  if (sync_processor_) {
    ActOnChange(change);
  } else if (!flare_.is_null()) {
    flare_.Run(syncer::AUTOFILL_PROFILE);
    flare_.Reset();
  }
}

bool AutofillProfileSyncableService::LoadAutofillData(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles) {
  return GetAutofillTable()->GetAutofillProfiles(profiles);
}

bool AutofillProfileSyncableService::SaveChangesToWebData(
    const DataBundle& bundle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AutofillTable* autofill_table = GetAutofillTable();

  bool success = true;
  for (size_t i = 0; i< bundle.profiles_to_delete.size(); ++i) {
    if (!autofill_table->RemoveAutofillProfile(bundle.profiles_to_delete[i]))
      success = false;
  }

  for (size_t i = 0; i < bundle.profiles_to_add.size(); i++) {
    if (!autofill_table->AddAutofillProfile(*bundle.profiles_to_add[i]))
      success = false;
  }

  for (size_t i = 0; i < bundle.profiles_to_update.size(); i++) {
    if (!autofill_table->UpdateAutofillProfile(*bundle.profiles_to_update[i]))
      success = false;
  }
  return success;
}

// static
bool AutofillProfileSyncableService::OverwriteProfileWithServerData(
    const sync_pb::AutofillProfileSpecifics& specifics,
    AutofillProfile* profile) {
  bool diff = false;
  if (specifics.has_origin() && profile->origin() != specifics.origin()) {
    bool was_verified = profile->IsVerified();
    // In this case, the local origin must be empty on the local |profile|, but
    // the remote profile was verified.
    if (specifics.origin() == kSettingsOrigin)
      profile->set_origin(kSettingsOrigin);
    diff = true;

    // Verified profiles should never be overwritten by unverified ones.
    DCHECK(!was_verified || profile->IsVerified());
  }

  // Update name, email, and phone fields.
  diff = UpdateField(NAME_FIRST,
                     specifics.name_first_size() ? specifics.name_first(0)
                                                 : std::string(),
                     profile) || diff;
  diff = UpdateField(NAME_MIDDLE,
                     specifics.name_middle_size() ? specifics.name_middle(0)
                                                  : std::string(),
                     profile) || diff;
  diff =
      UpdateField(NAME_LAST, specifics.name_last_size() ? specifics.name_last(0)
                                                        : std::string(),
                  profile) || diff;
  // Older versions don't have a separate full name; don't overwrite full name
  // in this case.
  if (specifics.name_full_size() > 0) {
    diff = UpdateField(NAME_FULL,
                       specifics.name_full_size() ? specifics.name_full(0)
                                                  : std::string(),
                       profile) || diff;
  }
  diff = UpdateField(EMAIL_ADDRESS,
                     specifics.email_address_size() ? specifics.email_address(0)
                                                    : std::string(),
                     profile) || diff;
  diff = UpdateField(PHONE_HOME_WHOLE_NUMBER,
                     specifics.phone_home_whole_number_size()
                         ? specifics.phone_home_whole_number(0)
                         : std::string(),
                     profile) || diff;

  // Update all simple single-valued address fields.
  diff = UpdateField(COMPANY_NAME, specifics.company_name(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_CITY,
                     specifics.address_home_city(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_STATE,
                     specifics.address_home_state(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_ZIP,
                     specifics.address_home_zip(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_SORTING_CODE,
                     specifics.address_home_sorting_code(), profile) || diff;
  diff = UpdateField(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     specifics.address_home_dependent_locality(),
                     profile) || diff;

  // Update the country field, which can contain either a country code (if set
  // by a newer version of Chrome), or a country name (if set by an older
  // version of Chrome).
  base::string16 country_name_or_code =
      ASCIIToUTF16(specifics.address_home_country());
  std::string country_code =
      CountryNames::GetInstance()->GetCountryCode(country_name_or_code);
  diff = UpdateField(ADDRESS_HOME_COUNTRY, country_code, profile) || diff;

  // Update the street address.  In newer versions of Chrome (M34+), this data
  // is stored in the |address_home_street_address| field.  In older versions,
  // this data is stored separated out by address line.
  if (specifics.has_address_home_street_address()) {
    diff = UpdateField(ADDRESS_HOME_STREET_ADDRESS,
                       specifics.address_home_street_address(),
                       profile) || diff;
  } else {
    diff = UpdateField(ADDRESS_HOME_LINE1,
                       specifics.address_home_line1(), profile) || diff;
    diff = UpdateField(ADDRESS_HOME_LINE2,
                       specifics.address_home_line2(), profile) || diff;
  }

  // Update the BCP 47 language code that can be used to format the address for
  // display.
  if (specifics.has_address_home_language_code() &&
      specifics.address_home_language_code() != profile->language_code()) {
    profile->set_language_code(specifics.address_home_language_code());
    diff = true;
  }

  // Update the validity state bitfield.
  if (specifics.has_validity_state_bitfield() &&
      specifics.validity_state_bitfield() !=
          profile->GetClientValidityBitfieldValue()) {
    profile->SetClientValidityFromBitfieldValue(
        specifics.validity_state_bitfield());
    diff = true;
  }

  if (static_cast<size_t>(specifics.use_count()) != profile->use_count()) {
    profile->set_use_count(specifics.use_count());
    diff = true;
  }

  if (specifics.use_date() != profile->use_date().ToTimeT()) {
    profile->set_use_date(base::Time::FromTimeT(specifics.use_date()));
    diff = true;
  }

  if (specifics.is_client_validity_states_updated() !=
      profile->is_client_validity_states_updated()) {
    profile->set_is_client_validity_states_updated(
        specifics.is_client_validity_states_updated());
    diff = true;
  }
  return diff;
}

// static
void AutofillProfileSyncableService::WriteAutofillProfile(
    const AutofillProfile& profile,
    sync_pb::EntitySpecifics* profile_specifics) {
  sync_pb::AutofillProfileSpecifics* specifics =
      profile_specifics->mutable_autofill_profile();

  DCHECK(base::IsValidGUID(profile.guid()));

  // Reset all multi-valued fields in the protobuf.
  specifics->clear_name_first();
  specifics->clear_name_middle();
  specifics->clear_name_last();
  specifics->clear_name_full();
  specifics->clear_email_address();
  specifics->clear_phone_home_whole_number();

  specifics->set_guid(profile.guid());
  specifics->set_origin(profile.origin());
  specifics->set_use_count(profile.use_count());
  specifics->set_use_date(profile.use_date().ToTimeT());

  // TODO(estade): this should be set_name_first.
  specifics->add_name_first(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(NAME_FIRST))));
  specifics->add_name_middle(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(NAME_MIDDLE))));
  specifics->add_name_last(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(NAME_LAST))));
  specifics->add_name_full(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(NAME_FULL))));
  specifics->set_address_home_line1(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE1))));
  specifics->set_address_home_line2(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_LINE2))));
  specifics->set_address_home_city(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_CITY))));
  specifics->set_address_home_state(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STATE))));
  specifics->set_address_home_zip(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_ZIP))));
  specifics->set_address_home_country(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY))));
  specifics->set_address_home_street_address(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS))));
  specifics->set_address_home_sorting_code(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE))));
  specifics->set_address_home_dependent_locality(
      LimitData(
          UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY))));
  specifics->set_address_home_language_code(LimitData(profile.language_code()));
  specifics->set_validity_state_bitfield(
      profile.GetClientValidityBitfieldValue());
  specifics->set_is_client_validity_states_updated(
      profile.is_client_validity_states_updated());

  // TODO(estade): this should be set_email_address.
  specifics->add_email_address(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(EMAIL_ADDRESS))));

  specifics->set_company_name(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(COMPANY_NAME))));

  // TODO(estade): this should be set_phone_home_whole_number.
  specifics->add_phone_home_whole_number(
      LimitData(UTF16ToUTF8(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER))));
}

void AutofillProfileSyncableService::CreateGUIDToProfileMap(
    const std::vector<std::unique_ptr<AutofillProfile>>& profiles,
    GUIDToProfileMap* profile_map) {
  DCHECK(profile_map);
  profile_map->clear();
  for (const auto& profile : profiles)
    (*profile_map)[profile->guid()] = profile.get();
}

AutofillProfileSyncableService::GUIDToProfileMap::iterator
AutofillProfileSyncableService::CreateOrUpdateProfile(
    const syncer::SyncData& data,
    GUIDToProfileMap* profile_map,
    DataBundle* bundle) {
  DCHECK(profile_map);
  DCHECK(bundle);

  DCHECK_EQ(syncer::AUTOFILL_PROFILE, data.GetDataType());

  const sync_pb::EntitySpecifics& specifics = data.GetSpecifics();
  const sync_pb::AutofillProfileSpecifics& autofill_specifics(
      specifics.autofill_profile());

  auto existing_profile = profile_map->find(autofill_specifics.guid());
  if (existing_profile != profile_map->end()) {
    // The synced profile already exists locally.  It might need to be updated.
    if (OverwriteProfileWithServerData(autofill_specifics,
                                       existing_profile->second)) {
      bundle->profiles_to_update.push_back(existing_profile->second);
    }
    return existing_profile;
  }

  // New profile synced.
  std::unique_ptr<AutofillProfile> new_profile =
      std::make_unique<AutofillProfile>(autofill_specifics.guid(),
                                        autofill_specifics.origin());
  AutofillProfile* new_profile_ptr = new_profile.get();
  OverwriteProfileWithServerData(autofill_specifics, new_profile_ptr);

  // Check if profile appears under a different guid. Compares only profile
  // contents. (Ignores origin and language code in comparison.)
  //
  // Unverified profiles should never overwrite verified ones.
  AutofillProfileComparator comparator(app_locale_);
  for (auto it = profile_map->begin(); it != profile_map->end(); ++it) {
    AutofillProfile* local_profile = it->second;
    if (local_profile->Compare(*new_profile) == 0) {
      // Ensure that a verified profile can never revert back to an unverified
      // one.
      if (local_profile->IsVerified() && !new_profile->IsVerified()) {
        new_profile->set_origin(local_profile->origin());
        bundle->profiles_to_sync_back.push_back(new_profile.get());
      }

      bundle->profiles_to_delete.push_back(local_profile->guid());
      DVLOG(2) << "[AUTOFILL SYNC]"
               << "Found in sync db but with a different guid: "
               << UTF16ToUTF8(local_profile->GetRawInfo(NAME_FIRST))
               << UTF16ToUTF8(local_profile->GetRawInfo(NAME_LAST))
               << "New guid " << new_profile->guid()
               << ". Profile to be deleted " << local_profile->guid();
      profile_map->erase(it);
      break;
    }
    if (!local_profile->IsVerified() && !new_profile->IsVerified() &&
        comparator.AreMergeable(*local_profile, *new_profile)) {
      // Add it to candidates for merge - if there is no profile with this guid
      // we will merge them.
      bundle->candidates_to_merge.insert(
          std::make_pair(local_profile->guid(), new_profile_ptr));
      break;
    }
  }
  profiles_.push_back(std::move(new_profile));
  bundle->profiles_to_add.push_back(new_profile_ptr);
  return profile_map
      ->insert(std::make_pair(new_profile_ptr->guid(), new_profile_ptr))
      .first;
}

void AutofillProfileSyncableService::ActOnChange(
     const AutofillProfileChange& change) {
  DCHECK(
      (change.type() == AutofillProfileChange::REMOVE &&
       !change.data_model()) ||
      (change.type() != AutofillProfileChange::REMOVE && change.data_model()));
  DCHECK(sync_processor_);

  if (change.data_model() &&
      change.data_model()->record_type() != AutofillProfile::LOCAL_PROFILE) {
    return;
  }

  syncer::SyncChangeList new_changes;
  DataBundle bundle;
  switch (change.type()) {
    case AutofillProfileChange::ADD:
      new_changes.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_ADD,
                             CreateData(*(change.data_model()))));
      DCHECK(profiles_map_.find(change.data_model()->guid()) ==
             profiles_map_.end());
      profiles_.push_back(
          std::make_unique<AutofillProfile>(*(change.data_model())));
      profiles_map_[change.data_model()->guid()] = profiles_.back().get();
      break;
    case AutofillProfileChange::UPDATE: {
      auto it = profiles_map_.find(change.data_model()->guid());
      DCHECK(it != profiles_map_.end());
      *(it->second) = *(change.data_model());
      new_changes.push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_UPDATE,
                             CreateData(*(change.data_model()))));
      break;
    }
    case AutofillProfileChange::REMOVE: {
      // Removals have no data_model() so this change can still be for a
      // SERVER_PROFILE. Rule it out by a lookup in profiles_map_.
      if (profiles_map_.find(change.key()) != profiles_map_.end()) {
        AutofillProfile empty_profile(change.key(), std::string());
        new_changes.push_back(
            syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_DELETE,
                               CreateData(empty_profile)));
        profiles_map_.erase(change.key());
      }
      break;
    }
    default:
      NOTREACHED();
  }
  syncer::SyncError error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, new_changes);
  if (error.IsSet()) {
    // TODO(isherman): Investigating http://crbug.com/121592
    VLOG(1) << "[AUTOFILL SYNC] "
            << "Failed processing change:\n"
            << "  Error: " << error.message() << "\n"
            << "  Guid: " << change.key();
  }
}

void AutofillProfileSyncableService::set_sync_processor(
    syncer::SyncChangeProcessor* sync_processor) {
  sync_processor_.reset(sync_processor);
}

syncer::SyncData AutofillProfileSyncableService::CreateData(
    const AutofillProfile& profile) {
  sync_pb::EntitySpecifics specifics;
  WriteAutofillProfile(profile, &specifics);
  return
      syncer::SyncData::CreateLocalData(
          profile.guid(), profile.guid(), specifics);
}

bool AutofillProfileSyncableService::UpdateField(
    ServerFieldType field_type,
    const std::string& new_value,
    AutofillProfile* autofill_profile) {
  if (UTF16ToUTF8(autofill_profile->GetRawInfo(field_type)) == new_value)
    return false;
  autofill_profile->SetRawInfo(field_type, UTF8ToUTF16(new_value));
  return true;
}

bool AutofillProfileSyncableService::MergeSimilarProfiles(
    const AutofillProfile& merge_from,
    AutofillProfile* merge_into,
    const std::string& app_locale) {
  const AutofillProfile old_merge_into = *merge_into;
  merge_into->MergeDataFrom(merge_from, app_locale);
  return !merge_into->EqualsForSyncPurposes(old_merge_into);
}

AutofillTable* AutofillProfileSyncableService::GetAutofillTable() const {
  return AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase());
}

void AutofillProfileSyncableService::InjectStartSyncFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

AutofillProfileSyncableService::DataBundle::DataBundle() {}

AutofillProfileSyncableService::DataBundle::DataBundle(
    const DataBundle& other) = default;

AutofillProfileSyncableService::DataBundle::~DataBundle() {}

}  // namespace autofill
