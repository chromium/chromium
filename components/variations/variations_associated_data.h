// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_

#include <map>
#include <memory>
#include <optional>
#include <string_view>

#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "components/variations/active_field_trials.h"

// This file provides various helpers that extend the functionality around
// base::FieldTrial.
//
// This includes several simple APIs to handle getting and setting additional
// data related to Chrome variations, such as parameters and Google variation
// IDs. These APIs are meant to extend the base::FieldTrial APIs to offer extra
// functionality that is not offered by the simpler base::FieldTrial APIs.
//
// The AssociateGoogleVariationID function is
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
//  std::string value = base::GetFieldTrialParamValue("trial", "param_x");
//  // use |value|, which will be "" if it does not exist
//
// VariationID id = GetGoogleVariationID(
//     GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, "trial", "group1");
// if (id != variations::EMPTY_ID) {
//   // use |id|
// }

namespace variations {

typedef int VariationID;

// A time window is used to timebox a VariationID. Each VariationID will be
// transmitted via the X-Client-Data header only when the current time is
// between the (inclusive) start and end timestamps of the TimeWindow for that
// VariationID. These times are network times. The client should makes its best
// effort to use a network synchronized time source when comparing the
// `current_time` to the start and end timestamps of a TimeWindow.
class COMPONENT_EXPORT(VARIATIONS) TimeWindow {
 public:
  TimeWindow() = default;
  TimeWindow(base::Time start, base::Time end);

  // Copyable and moveable.
  TimeWindow(const TimeWindow& other) = default;
  TimeWindow(TimeWindow&& other) = default;
  TimeWindow& operator=(const TimeWindow& other) = default;
  TimeWindow& operator=(TimeWindow&& other) = default;

  base::Time start() const { return start_; }
  base::Time end() const { return end_; }

 private:
  base::Time start_ = base::Time::Min();
  base::Time end_ = base::Time::Max();
};

const VariationID EMPTY_ID = 0;

// A key into the Associate/Get methods for VariationIDs. This is used to create
// separate ID associations for separate parties interested in VariationIDs.
enum IDCollectionKey {
  // The IDs in this collection are used by Google web properties and are
  // transmitted via the X-Client-Data header. These IDs are transmitted in
  // first- and third-party contexts.
  GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
  // The IDs in this collection are used by Google web properties and are
  // transmitted via the X-Client-Data header. Transmitted in only first-party
  // contexts.
  GOOGLE_WEB_PROPERTIES_FIRST_PARTY,
  // This collection is used by Google web properties for signed in users only,
  // transmitted through the X-Client-Data header.
  GOOGLE_WEB_PROPERTIES_SIGNED_IN,
  // The IDs in this collection are used by Google web properties to trigger
  // server-side experimental behavior and are transmitted via the X-Client-Data
  // header. These IDs are transmitted in first- and third-party contexts.
  GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
  // The IDs in this collection are used by Google web properties to trigger
  // server-side experimental behavior and are transmitted via the X-Client-Data
  // header. Transmitted in only first-party contexts.
  GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY,
  // This collection is used by the Google App and is passed at the time
  // the cross-app communication is triggered.
  GOOGLE_APP,
  // The total count of collections.
  ID_COLLECTION_COUNT,
};

// Associate a variations::VariationID value with a FieldTrial group for
// collection |key|. If an id was previously set for |trial_name| and
// |group_name|, this does nothing. The group is denoted by |trial_name| and
// |group_name|. This must be called whenever a FieldTrial is prepared (create
// the trial and append groups) and needs to have a variations::VariationID
// associated with it so Google servers can recognize the FieldTrial. The
// transmission of the VariationID will be limited to the |time_window|.
// Thread safe.
COMPONENT_EXPORT(VARIATIONS)
void AssociateGoogleVariationID(IDCollectionKey key,
                                std::string_view trial_name,
                                std::string_view group_name,
                                VariationID id,
                                TimeWindow time_window = TimeWindow());

// As above, but overwrites any previously set id. Thread safe.
COMPONENT_EXPORT(VARIATIONS)
void AssociateGoogleVariationIDForce(IDCollectionKey key,
                                     std::string_view trial_name,
                                     std::string_view group_name,
                                     VariationID id,
                                     TimeWindow time_window = TimeWindow());

// As above, but takes an ActiveGroupId hash pair, rather than the string names.
COMPONENT_EXPORT(VARIATIONS)
void AssociateGoogleVariationIDForceHashes(
    IDCollectionKey key,
    ActiveGroupId active_group,
    VariationID id,
    TimeWindow time_window = TimeWindow());

// Retrieve the variations::VariationID associated with a FieldTrial group for
// collection |key|. The group is denoted by |trial_name| and |group_name|.
// This will return variations::EMPTY_ID if there is currently no associated ID
// for the named group. This API can be nicely combined with
// FieldTrial::GetActiveFieldTrialGroups() to enumerate the variation IDs for
// all active FieldTrial groups. If a |current_time| is provided, the
// VariationID will be returned only if the current time is between the
// (inclusive) start and end timestamps of the TimeWindow for that VariationID.
// Thread safe.
COMPONENT_EXPORT(VARIATIONS)
VariationID GetGoogleVariationID(
    IDCollectionKey key,
    std::string_view trial_name,
    std::string_view group_name,
    std::optional<base::Time> current_time = std::nullopt);

// Same as GetGoogleVariationID(), but takes in a hashed |active_group| rather
// than the string trial and group name.
COMPONENT_EXPORT(VARIATIONS)
VariationID GetGoogleVariationIDFromHashes(
    IDCollectionKey key,
    ActiveGroupId active_group,
    std::optional<base::Time> current_time = std::nullopt);

// Given `current_time`, returns the next time that a time windows will start or
// end for a VariationID.
COMPONENT_EXPORT(VARIATIONS)
base::Time GetNextTimeWindowEvent(base::Time current_time);

// Expose some functions for testing.
namespace testing {

// Clears all of the mapped associations. Deprecated, use ScopedFeatureList
// instead as it does a lot of work for you automatically.
COMPONENT_EXPORT(VARIATIONS) void ClearAllVariationIDs();

// Clears all of the associated params. Deprecated, use ScopedFeatureList
// instead as it does a lot of work for you automatically.
COMPONENT_EXPORT(VARIATIONS) void ClearAllVariationParams();

}  // namespace testing

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_
