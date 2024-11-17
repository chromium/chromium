// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_IDS_PROVIDER_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_IDS_PROVIDER_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/synchronization/lock.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_associated_data.h"

namespace variations {
class VariationsClient;

namespace cros_early_boot::evaluate_seed {
// For friend purposes (for DestroyInstanceForTesting().)
class VariationsCrosEvaluateSeedMainTest;
}  // namespace cros_early_boot::evaluate_seed

// The key for a VariationsIdsProvider's |variations_headers_map_|. A
// VariationsHeaderKey provides more details about the VariationsIDs included in
// a particular header. For example, the header associated with a key with true
// for |is_signed_in| and Study_GoogleWebVisibility_ANY for |web_visibility| has
// (i) VariationsIDs associated with external experiments, which can be sent
// only for signed-in users and (ii) VariationsIDs that can be sent in first-
// and third-party contexts.
struct COMPONENT_EXPORT(VARIATIONS) VariationsHeaderKey {
  bool is_signed_in;
  Study_GoogleWebVisibility web_visibility;

  // This allows the struct to be used as a key in a map.
  bool operator<(const VariationsHeaderKey& other) const;
};

// A helper class for maintaining client experiments and metrics state
// transmitted in custom HTTP request headers.
// This class is a thread-safe singleton.
class COMPONENT_EXPORT(VARIATIONS) VariationsIdsProvider
    : public base::FieldTrialList::Observer,
      public SyntheticTrialObserver {
 public:
  class COMPONENT_EXPORT(VARIATIONS) Observer {
   public:
    // Called when variation ids headers are updated.
    virtual void VariationIdsHeaderUpdated() = 0;

   protected:
    virtual ~Observer() = default;
  };

  enum class Mode {
    // Indicates the signed-in parameter supplied to GetClientDataHeaders() is
    // honored.
    kUseSignedInState,

    // Indicates the signed-in parameter supplied to GetClientDataHeaders() is
    // treated as true, regardless of what is supplied. This is intended for
    // embedders (such as WebLayer) that do not have the notion of signed-in.
    kIgnoreSignedInState,

    // Indicates the signed-in parameter supplied to GetClientDataHeaders() is
    // treated as false, regardless of what is supplied.
    kDontSendSignedInVariations,
  };

  // Creates the VariationsIdsProvider instance. This must be called before
  // GetInstance(). Only one instance of VariationsIdsProvider may be created.
  static VariationsIdsProvider* Create(Mode mode);

  static VariationsIdsProvider* GetInstance();

  VariationsIdsProvider(const VariationsIdsProvider&) = delete;
  VariationsIdsProvider& operator=(const VariationsIdsProvider&) = delete;

  Mode mode() const { return mode_; }

  // Returns the X-Client-Data headers corresponding to |is_signed_in|: a header
  // that may be sent in first-party requests and a header that may be sent in
  // third-party requests. For more details, see IsFirstPartyContext() in
  // variations_http_headers.cc.
  //
  // If |is_signed_in| is false, VariationIDs that should be sent for only
  // signed in users (i.e. GOOGLE_WEB_PROPERTIES_SIGNED_IN entries) are not
  // included. Also, computes and caches the header if necessary. |is_signed_in|
  // is impacted by the Mode supplied when VariationsIdsProvider is created.
  // See Mode for details.
  variations::mojom::VariationsHeadersPtr GetClientDataHeaders(
      bool is_signed_in);

  // Returns a space-separated string containing the list of current active
  // variations (as would be reported in the |variation_id| repeated field of
  // the ClientVariations proto). Does not include variation ids that should be
  // sent for signed-in users only and does not include Google app variations.
  // The returned string is guaranteed to have a leading and trailing space,
  // e.g. " 123 234 345 ".
  std::string GetVariationsString();

  // Same as GetVariationString(), but returns Google App variation ids rather
  // than Google Web variations.
  // IMPORTANT: This string is only approved for integrations with the Android
  // Google App and must receive a privacy review before extending to other
  // apps.
  std::string GetGoogleAppVariationsString();

  // Same as GetVariationString(), but returns all triggering IDs.
  // IMPORTANT: This string should only be used for debugging and diagnostics.
  std::string GetTriggerVariationsString();

  // Returns the collection of VariationIDs associated with |keys|. Each entry
  // in the returned vector is unique.
  std::vector<VariationID> GetVariationsVector(
      const std::set<IDCollectionKey>& keys);

  // Returns the collection of variations ids for all Google Web Properties
  // related keys.
  std::vector<VariationID> GetVariationsVectorForWebPropertiesKeys();

  // Sets low entropy source value that was used for client-side randomization
  // of variations.
  void SetLowEntropySourceValue(std::optional<int> low_entropy_source_value);

  // Result of ForceVariationIds() call.
  enum class ForceIdsResult {
    SUCCESS,
    INVALID_VECTOR_ENTRY,  // Invalid entry in |variation_ids|.
    INVALID_SWITCH_ENTRY,  // Invalid entry in |command_line_variation_ids|.
  };

  // Sets *additional* variation ids and trigger variation ids to be encoded in
  // the X-Client-Data request header. This is intended for development use to
  // force a server side experiment id. |variation_ids| should be a list of
  // strings of numeric experiment ids. Ids explicitly passed in |variation_ids|
  // and those in the comma-separated |command_line_variation_ids| are added.
  ForceIdsResult ForceVariationIds(
      const std::vector<std::string>& variation_ids,
      const std::string& command_line_variation_ids);

  // Ensures that the given variation ids and trigger variation ids are not
  // encoded in the X-Client-Data request header. This is intended for
  // development use to force that a server side experiment id is not set.
  // |command_line_variation_ids| are comma-separted experiment ids.
  // Returns true on success.
  bool ForceDisableVariationIds(const std::string& command_line_variation_ids);

  // Methods to register or remove observers of variation ids header update.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Resets any cached state for tests.
  void ResetForTesting();

 private:
  typedef std::pair<VariationID, IDCollectionKey> VariationIDEntry;

  friend class ScopedVariationsIdsProvider;
  // For DestroyInstanceForTesting
  friend class cros_early_boot::evaluate_seed::
      VariationsCrosEvaluateSeedMainTest;

  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest, ForceVariationIds_Valid);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           ForceVariationIds_ValidCommandLine);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           ForceVariationIds_Invalid);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           ForceDisableVariationIds_ValidCommandLine);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           ForceDisableVariationIds_Invalid);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           OnFieldTrialGroupFinalized);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           LowEntropySourceValue_Valid);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           LowEntropySourceValue_Null);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           GetGoogleAppVariationsString);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest, GetVariationsString);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest, GetVariationsVector);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest,
                           GetVariationsVectorForWebPropertiesKeys);
  FRIEND_TEST_ALL_PREFIXES(VariationsIdsProviderTest, GetVariationsVectorImpl);

  explicit VariationsIdsProvider(Mode mode);
  ~VariationsIdsProvider() override;

  static void CreateInstanceForTesting(Mode mode);
  static void DestroyInstanceForTesting();

  // Returns a space-separated string containing the list of current active
  // variations (as would be reported in the |variation_id| repeated field of
  // the ClientVariations proto) for a given ID collection.
  std::string GetVariationsString(const std::set<IDCollectionKey>& keys);

  // base::FieldTrialList::Observer:
  // This will add the variation ID associated with |trial_name| and
  // |group_name| to the variation ID cache.
  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override;

  // metrics::SyntheticTrialObserver:
  void OnSyntheticTrialsChanged(
      const std::vector<SyntheticTrialGroup>& trials_updated,
      const std::vector<SyntheticTrialGroup>& trials_removed,
      const std::vector<SyntheticTrialGroup>& groups) override;

  // Prepares the variation IDs cache with initial values if not already done.
  // This method also registers the caller with the FieldTrialList to receive
  // new variation IDs.
  void InitVariationIDsCacheIfNeeded();

  // Looks up the VariationID associated with |trial_name| and |group_name|, and
  // if found, adds an entry for it to |variation_ids_set_|.
  void CacheVariationsId(const std::string& trial_name,
                         const std::string& group_name);

  // Takes whatever is currently in |variation_ids_set_| and recreates
  // |variation_ids_header_| with it.  Assumes the the |lock_| is currently
  // held.
  void UpdateVariationIDsHeaderValue();

  // Generates a base64-encoded ClientVariations proto to be used as a header
  // value for the given |is_signed_in| and |is_first_party_context| states.
  std::string GenerateBase64EncodedProto(bool is_signed_in,
                                         bool is_first_party_context);

  // Adds variation ids and trigger variation ids to |target_set|. If
  // |should_dedupe| is true, the ids in |variation_ids| that have already been
  // added as non-Google-app ids are not added to |target_set|. Returns false if
  // any variation ids are malformed or duplicated. Returns true otherwise.
  bool AddVariationIdsToSet(const std::vector<std::string>& variation_ids,
                            bool should_dedupe,
                            std::set<VariationIDEntry>* target_set);

  // Parses a comma-separated string of variation ids and trigger variation ids
  // and adds them to |target_set|. If |should_dedupe| is true, ids that have
  // already been added as non-Google-app ids are not added to |target_set|.
  // Returns false if any variation ids are malformed or duplicated. Returns
  // true otherwise.
  bool ParseVariationIdsParameter(const std::string& command_line_variation_ids,
                                  bool should_dedupe,
                                  std::set<VariationIDEntry>* target_set);

  // Returns the value of the X-Client-Data header corresponding to
  // |is_signed_in| and |web_visibility|. Considering |web_visibility| may allow
  // fewer VariationIDs to be sent in third-party contexts.
  std::string GetClientDataHeaderWhileLocked(
      bool is_signed_in,
      Study_GoogleWebVisibility web_visibility);

  // Returns the currently active set of variation ids, which includes ids from
  // field trials, synthetic trials, and forced ids.
  std::set<VariationIDEntry> GetAllVariationIds();

  // Returns the collection of variation ids matching any of the given
  // |keys|. Each entry in the returned vector will be unique.
  std::vector<VariationID> GetVariationsVectorImpl(
      const std::set<IDCollectionKey>& key);

  // Returns whether |id| has already been added to the active set of variation
  // ids. This includes ids from field trials, synthetic trials, and forced ids.
  // Note that Google app ids are treated differently. They may be reused as a
  // Google Web id.
  bool IsDuplicateId(VariationID id);

  const Mode mode_;

  // Guards access to variables below.
  base::Lock lock_;

  // Low entropy source value from client that was used for client-side
  // randomization of variations.
  std::optional<int> low_entropy_source_value_;

  // Whether or not we've initialized the caches.
  bool variation_ids_cache_initialized_;

  // Keep a cache of variation IDs that are transmitted in headers to Google.
  // This consists of a list of valid IDs, and the actual transmitted header.
  std::set<VariationIDEntry> variation_ids_set_;

  // Provides the google experiment ids that are force-enabled through
  // ForceVariationIds().
  std::set<VariationIDEntry> force_enabled_ids_set_;

  // Variations ids from synthetic field trials.
  std::set<VariationIDEntry> synthetic_variation_ids_set_;

  // Provides the google experiment ids that are force-disabled by command line.
  std::set<VariationIDEntry> force_disabled_ids_set_;

  // A collection of variations headers. Each header is a base64-encoded
  // ClientVariations proto containing VariationIDs that may be sent to Google
  // web properties. For more details about when this may be sent, see
  // AppendHeaderIfNeeded() in variations_http_headers.cc.
  //
  // The key for each header describes the VariationIDs included in its
  // associated header. See VariationsHeaderKey's comments for more information.
  std::map<VariationsHeaderKey, std::string> variations_headers_map_
      GUARDED_BY(lock_);

  // List of observers to notify on variation ids header update.
  // NOTE this should really check observers are unregistered but due to
  // https://crbug.com/1051937 this isn't currently possible. Note that
  // ObserverList is sequence checked so we can't use that here.
  std::vector<Observer*> observer_list_ GUARDED_BY(lock_);

  raw_ptr<const VariationsClient> variations_client_ = nullptr;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_IDS_PROVIDER_H_
