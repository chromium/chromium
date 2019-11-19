// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/variations/active_field_trials.h"

// This file provides various helpers that extend the functionality around
// base::FieldTrial.
//
// This includes several simple APIs to handle getting and setting additional
// data related to Chrome variations, such as parameters and Google variation
// IDs. These APIs are meant to extend the base::FieldTrial APIs to offer extra
// functionality that is not offered by the simpler base::FieldTrial APIs.
//
// The AssociateGoogleVariationID and AssociateVariationParams functions are
// generally meant to be called by the VariationsService based on server-side
// variation configs, but may also be used for client-only field trials by
// invoking them directly after appending all the groups to a FieldTrial.
//
// Experiment code can then use the getter APIs to retrieve variation parameters
// or IDs:
//
//  std::map<std::string, std::string> params;
//  if (GetVariationParams("trial", &params)) {
//    // use |params|
//  }
//
//  std::string value = GetVariationParamValue("trial", "param_x");
//  // use |value|, which will be "" if it does not exist
//
// VariationID id = GetGoogleVariationID(GOOGLE_WEB_PROPERTIES, "trial",
//                                       "group1");
// if (id != variations::EMPTY_ID) {
//   // use |id|
// }

namespace base {
struct Feature;
}  // namespace base

namespace variations {

typedef int VariationID;

const VariationID EMPTY_ID = 0;

// A key into the Associate/Get methods for VariationIDs. This is used to create
// separate ID associations for separate parties interested in VariationIDs.
enum IDCollectionKey {
  // This collection is used by Google web properties, transmitted through the
  // X-Client-Data header.
  GOOGLE_WEB_PROPERTIES,
  // This collection is used by Google web properties for signed in users only,
  // transmitted through the X-Client-Data header.
  GOOGLE_WEB_PROPERTIES_SIGNED_IN,
  // This collection is used by Google web properties for IDs that trigger
  // server side experimental behavior, transmitted through the
  // X-Client-Data header.
  GOOGLE_WEB_PROPERTIES_TRIGGER,
  // The total count of collections.
  ID_COLLECTION_COUNT,
};

// Associate a variations::VariationID value with a FieldTrial group for
// collection |key|. If an id was previously set for |trial_name| and
// |group_name|, this does nothing. The group is denoted by |trial_name| and
// |group_name|. This must be called whenever a FieldTrial is prepared (create
// the trial and append groups) and needs to have a variations::VariationID
// associated with it so Google servers can recognize the FieldTrial.
// Thread safe.
void AssociateGoogleVariationID(IDCollectionKey key,
                                const std::string& trial_name,
                                const std::string& group_name,
                                VariationID id);

// As above, but overwrites any previously set id. Thread safe.
void AssociateGoogleVariationIDForce(IDCollectionKey key,
                                     const std::string& trial_name,
                                     const std::string& group_name,
                                     VariationID id);

// As above, but takes an ActiveGroupId hash pair, rather than the string names.
void AssociateGoogleVariationIDForceHashes(IDCollectionKey key,
                                           const ActiveGroupId& active_group,
                                           VariationID id);

// Retrieve the variations::VariationID associated with a FieldTrial group for
// collection |key|. The group is denoted by |trial_name| and |group_name|.
// This will return variations::EMPTY_ID if there is currently no associated ID
// for the named group. This API can be nicely combined with
// FieldTrial::GetActiveFieldTrialGroups() to enumerate the variation IDs for
// all active FieldTrial groups. Thread safe.
VariationID GetGoogleVariationID(IDCollectionKey key,
                                 const std::string& trial_name,
                                 const std::string& group_name);

// Same as GetGoogleVariationID(), but takes in a hashed |active_group| rather
// than the string trial and group name.
VariationID GetGoogleVariationIDFromHashes(IDCollectionKey key,
                                           const ActiveGroupId& active_group);

// Deprecated. Use base::AssociateFieldTrialParams() instead.
bool AssociateVariationParams(const std::string& trial_name,
                              const std::string& group_name,
                              const std::map<std::string, std::string>& params);

// Deprecated. Use base::GetFieldTrialParams() instead.
bool GetVariationParams(const std::string& trial_name,
                        std::map<std::string, std::string>* params);

// Deprecated. Use base::GetFieldTrialParamsByFeature() instead.
bool GetVariationParamsByFeature(const base::Feature& feature,
                                 std::map<std::string, std::string>* params);

// Deprecated. Use base::GetFieldTrialParamValue() instead.
std::string GetVariationParamValue(const std::string& trial_name,
                                   const std::string& param_name);

// Deprecated. Use base::GetFieldTrialParamValueByFeature() instead.
std::string GetVariationParamValueByFeature(const base::Feature& feature,
                                            const std::string& param_name);

// Deprecated. Use base::GetFieldTrialParamByFeatureAsInt() instead.
int GetVariationParamByFeatureAsInt(const base::Feature& feature,
                                    const std::string& param_name,
                                    int default_value);

// Deprecated. Use base::GetFieldTrialParamByFeatureAsDouble() instead.
double GetVariationParamByFeatureAsDouble(const base::Feature& feature,
                                          const std::string& param_name,
                                          double default_value);

// Deprecated. Use base::GetFieldTrialParamByFeatureAsBool() instead.
bool GetVariationParamByFeatureAsBool(const base::Feature& feature,
                                      const std::string& param_name,
                                      bool default_value);

// Expose some functions for testing.
namespace testing {

// Clears all of the mapped associations. Deprecated, use ScopedFeatureList
// instead as it does a lot of work for you automatically.
void ClearAllVariationIDs();

// Clears all of the associated params. Deprecated, use ScopedFeatureList
// instead as it does a lot of work for you automatically.
void ClearAllVariationParams();

}  // namespace testing

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_
