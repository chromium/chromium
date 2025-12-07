// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
#define COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/location.h"

namespace syncer {

// A minimal error object that individual datatypes can report.
class ModelError {
 public:
  // This enum should be in sync with ModelErrorType in enums.xml. These
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.

  // LINT.IfChange(Type)
  enum class Type {
    // kUnspecified = 0, //deprecated
    kPasswordDbInitFailed = 1,
    kPasswordMergeDecryptionFailed = 2,
    kPasswordMergeUpdateFailed = 3,
    kPasswordIncrementalAddFailed = 4,
    kPasswordCleanupDbFailed = 5,
    kPasswordMergeReadFromDbFailed = 6,
    kPasswordMergeReadAfterCleanupFailed = 7,
    kPasswordCommitReadFailed = 8,
    kPasswordDebugReadFailed = 9,
    kPasswordMergeAddFailed = 10,
    kPasswordCleanupDecryptionFailed = 11,
    kPasswordIncrementalUpdateFailed = 12,
    kAppListSyncableServiceNotStarted = 13,
    kArcPackageSyncableServiceNotStarted = 14,
    kAutocompleteFailedToClearMetadataForExpiredEntry = 15,
    kContactInfoFailedToAddProfileToTable = 16,
    kContactInfoFailedToDeleteIncompleteHwProfile = 17,
    kContactInfoFailedToDeleteProfileForRemoteDelete = 18,
    kContactInfoFailedToDeleteProfileFromTable = 19,
    kContactInfoFailedToReadMetadataFromWebDatabase = 20,
    kContactInfoFailedToUpdateProfileInTable = 21,
    kAutofillProfileFailedToAddToDatabase = 22,
    kAutofillProfileFailedToAddProfileForSync = 23,
    kAutofillProfileFailedToDeleteFromDatabase = 24,
    kAutofillProfileFailedToDeleteProfileForSync = 25,
    kAutofillProfileFailedToLoadDatabase = 26,
    kAutofillProfileFailedToLoadEntriesForCommit = 27,
    kAutofillProfileFailedToLoadEntriesForDebugging = 28,
    kAutofillProfileFailedToLoadMetadata = 29,
    kAutofillProfileFailedToReadDatabase = 30,
    kAutofillProfileFailedToReadLocalDataForDeletion = 31,
    kAutofillProfileFailedToReadLocalDataForInitialSync = 32,
    kAutofillProfileFailedToReadLocalDataForInitialSyncMerge = 33,
    kAutofillProfileFailedToReadLocalDataForRemoteUpdate = 34,
    kAutofillProfileFailedToUpdateDatabase = 35,
    kAutofillProfileFailedToUpdateProfileForSync = 36,
    kAutofillValuableFailedToLoadDatabase = 37,
    kAutofillValuableFailedToLoadMetadata = 38,
    kAutofillValuableFailedToReadMetadata = 39,
    kAutofillValuableFailedToWriteToDatabase = 40,
    kAutofillValuableUnsupportedActionType = 41,
    kAutofillWalletFailedToLoadBenefitsFromTable = 42,
    kAutofillWalletFailedToLoadDatabase = 43,
    kAutofillWalletFailedToLoadEntriesFromTable = 44,
    kAutofillWalletFailedToReadMetadata = 45,
    kWalletCredentialFailedToAddDatabase = 46,
    kWalletCredentialFailedToDeleteFromDatabase = 47,
    kWalletCredentialFailedToDeleteOnDisable = 48,
    kWalletCredentialFailedToLoadDatabase = 49,
    kWalletCredentialFailedToReadMetadata = 50,
    kWalletCredentialFailedToUpdateDatabase = 51,
    kWalletMetadataFailedToLoadDatabase = 52,
    kWalletMetadataFailedToReadData = 53,
    kWalletMetadataFailedToReadMetadata = 54,
    kAutofillWalletOfferFailedToLoadFromTable = 55,
    kAutofillWalletOfferFailedToLoadTable = 56,
    kAutofillWalletOfferFailedToReadMetadata = 57,
    kAutofillWalletUsageFailedToAddOrUpdateVirtualCardUsageData = 58,
    kAutofillWalletUsageFailedToDeleteAllVirtualCardUsageData = 59,
    kAutofillWalletUsageFailedToDeleteVirtualCardUsageData = 60,
    kAutofillWalletUsageFailedToLoadAutofillTable = 61,
    kAutofillWalletUsageFailedToLoadData = 62,
    kAutofillWalletUsageFailedToReadMetadata = 63,
    kBookmarksInitialMergePermanentEntitiesMissing = 64,
    kBookmarksLocalCountExceededLimitNudgeForCommit = 65,
    kBookmarksPermanentEntitiesMissing = 66,
    kBookmarksRemoteCountExceededLimitInitialMerge = 67,
    kBookmarksRemoteCountExceededLimitLastInitialMerge = 68,
    kBookmarksTestError = 69,
    kCollaborationGroupDeserializeDataFailed = 71,
    kCollaborationGroupReadAllDataFailed = 72,
    kCollaborationGroupReadAllMetadataFailed = 73,
    kCollaborationGroupStoreCreationFailed = 74,
    kDataTypeStoreBackendDbIterationFailed = 75,
    kDataTypeStoreBackendDbOpenFailed = 76,
    kDataTypeStoreBackendDbReadFailed = 77,
    kDataTypeStoreBackendDbWriteFailed = 78,
    kDataTypeStoreBackendDeletePrefixFailed = 79,
    kDataTypeStoreBackendInvalidSchemaDescriptor = 80,
    kDataTypeStoreBackendSchemaUpgradeFailed = 81,
    kDataTypeStoreBackendSchemaVersionTooHigh = 82,
    kDataTypeStoreBackendWriteModificationsFailed = 83,
    kDataTypeStoreFailedToDeserializeDataTypeState = 84,
    kDataTypeStoreFailedToDeserializeEntityMetadata = 85,
    kDataTypeStoreServiceBackendInitializationFailed = 86,
    kProcessorNonEmptyUpdateWithoutVersionWatermark = 87,
    kProcessorTestError = 88,
    kProcessorVersionWatermarkWithIncrementalUpdates = 89,
    kDeviceInfoCommitFailed = 90,
    kDeviceInfoDeserializeSpecificsFailed = 91,
    kDeviceInfoReadDataFailed = 92,
    kDeviceInfoReadMetadataFailed = 93,
    kDeviceInfoStoreCreationFailed = 94,
    kHistoryDatabaseError = 95,
    kHistoryFailedToLoadMetadata = 96,
    kHistoryFailedToLoadMetadataForDebugging = 97,
    kHistoryDeleteDirectiveSyncDisabled = 98,
    kHistoryDeleteDirectiveSyncDisabledOnLocalCreation = 99,
    kHistoryDeleteDirectiveSyncDisabledOnRemoteChange = 100,
    kModelLoadManagerDataTypeInFailedState = 101,
    kNigoriEmptyEntityDataDuringInitialSync = 102,
    kNigoriInvalidEncryptedTypesTransition = 103,
    kNigoriInvalidPassphraseTransition = 104,
    kNigoriInvalidSpecifics = 105,
    kNigoriMissingKeystoreKeysDuringInitialSync = 106,
    kNigoriMissingLastTrustedVaultKeyInKeybag = 107,
    kNigoriMissingNewDefaultKeyInKeybag = 108,
    kPlusAddressAddOrUpdateProfileFailed = 121,
    kPlusAddressClearProfilesFailed = 122,
    kPlusAddressDatabaseInitFailed = 123,
    kPlusAddressMetadataReadFailed = 124,
    kPlusAddressRemoveProfileFailed = 125,
    kPlusAddressSettingFailedToParseSpecifics = 126,
    kPlusAddressTransactionBeginFailedOnDisableSync = 127,
    kPlusAddressTransactionBeginFailedOnIncrementalSync = 128,
    kPlusAddressTransactionCommitFailedOnDisableSync = 129,
    kPlusAddressTransactionCommitFailedOnIncrementalSync = 130,
    // kPowerBookmarkDatabaseCommitFailed = 131,  // Deprecated.
    // kPowerBookmarkDatabaseInitFailed = 132,  // Deprecated.
    // kPowerBookmarkFailedToBeginTransaction = 133,  // Deprecated.
    // kPowerBookmarkFailedToCommitTransaction = 134,  // Deprecated.
    // kPowerBookmarkFailedToDeleteLocalPowers = 135,  // Deprecated.
    // kPowerBookmarkFailedToLoadMetadata = 136,  // Deprecated.
    // kPowerBookmarkFailedToMergeLocalPowers = 137,  // Deprecated.
    kPrefFailedToDeserializeValue = 138,
    kPrefModelsNotAssociated = 139,
    kPrintersFailedToDeserializeSpecifics = 140,
    kProfileAuthServersFailedToDeserializeSpecifics = 141,
    kReadingListStorageLoadFailed = 142,
    kSavedTabGroupFailedToSaveMetadata = 144,
    kSharedTabGroupDataDatabaseSaveFailed = 145,
    kSharedTabGroupDuplicateTabGuid = 146,
    kSharedTabGroupUnexpectedCollaborationIdForGroup = 147,
    kSharedTabGroupUnexpectedCollaborationIdForTab = 148,
    kSearchEngineDeleteNonExistentAtAccountLevel = 149,
    kSearchEngineLocalDbLoadFailed = 150,
    kSearchEngineModelsNotAssociated = 151,
    kSendTabToSelfFailedToDeserializeSpecifics = 152,
    kSettingsFailedToApplySyncDelete = 153,
    kSettingsFailedToApplySyncUpdate = 154,
    kSettingsFailedToGetLocalSettingForKey = 155,
    kSettingsFailedToGetLocalSettings = 156,
    kSettingsFailedToApplySyncAdd = 157,
    kSettingsSyncInactive = 158,
    kSpellcheckCustomDictionaryUpdateFailed = 159,
    kSyncableServiceBasedBridgeFailedToDeserializeData = 160,
    kSyncMetadataStoreChangeListInvalidStore = 161,
    kSyncMetadataStoreClearDataTypeStateFailed = 162,
    kSyncMetadataStoreClearEntityMetadataFailed = 163,
    kSyncMetadataStoreUpdateDataTypeStateFailed = 164,
    kSyncMetadataStoreUpdateEntityMetadataFailed = 165,
    kThemeInvalidChangeType = 166,
    kThemeMissingSpecifics = 167,
    kThemeSyncableServiceNotStarted = 168,
    kThemeTooManyChanges = 169,
    kThemeTooManySpecifics = 170,
    kWorkspaceDeskFailedToDeserializeSpecifics = 171,
    kWorkspaceDeskFailedToParseUuid = 172,
    kBookmarksLocalCountExceededLimitOnSyncStart = 173,
    kBookmarksLocalCountExceededLimitOnUpdateReceived = 174,
    kAutocompleteFailedToDeleteEntriesFromDatabase = 175,
    kAutocompleteFailedToLoadAutofillWebDatabase = 176,
    kAutocompleteFailedToLoadEntriesFromDatabase = 177,
    kAutocompleteFailedToParseStorageKey = 178,
    kAutocompleteFailedToReadEntryFromDatabase = 179,
    kAutocompleteFailedToReadFromDatabase = 180,
    kAutocompleteFailedToReadMetadataFromWebDatabase = 181,
    kAutocompleteFailedToUpdateEntriesInDatabase = 182,
    kContactInfoFailedToDeleteProfilesFromTable = 183,
    kContactInfoFailedToDeleteProfilesOnDisableSync = 184,
    kContactInfoFailedToLoadAutofillWebDatabase = 185,
    kContactInfoFailedToLoadProfilesFromTable = 186,
    kGenericTestError = 187,
    kAutofillValuableMetadataFailedToLoadDatabase = 188,
    kAutofillValuableMetadataTransactionCommitFailedOnIncrementalSync = 189,
    kAutofillValuableMetadataFailedToLoadMetadata = 190,
    kDataTypeControllerInFailedState = 191,
    kMaxValue = kDataTypeControllerInFailedState,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sync/enums.xml:SyncModelError)

  // Creates a set error object with the given location and error type.
  ModelError(const base::Location& location, Type model_error_type);

  ~ModelError();

  // The location of the error this object represents. Can only be called if the
  // error is set.
  const base::Location& location() const;

  // The type of the error this object represents. Only set if the error type is
  // known. Otherwise, returns ModelErrorType::kUnspecified.
  ModelError::Type type() const;

  // Returns string representation of this object, appropriate for logging.
  std::string ToString() const;

 private:
  base::Location location_;
  // The type of the error. This is optional to ensure backwards compatibility.
  // It is used for metrics collection.
  Type type_;
};

// Typedef for a simple error handler callback.
using ModelErrorHandler = base::RepeatingCallback<void(const ModelError&)>;

using OnceModelErrorHandler = base::OnceCallback<void(const ModelError&)>;

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_ERROR_H_
