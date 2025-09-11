// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides functions for associating FieldTrials with Google VariationIDs and
// time windows.
//
// Example usage:
//
// AssociateGoogleVariationID(
//   GOOGLE_WEB_PROPERTIES_FIRST_PARTY, "MyStudy", "TreatmentGroup", 1234);
//
// VariationID id = GetGoogleVariationID(
//   GOOGLE_WEB_PROPERTIES_FIRST_PARTY, "MyStudy", "TreatmentGroup");

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_

#include <map>
#include <memory>
#include <optional>
#include <string_view>

#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "components/variations/active_field_trials.h"

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
  // Creates a TimeWindow with the given `start` and `end` times. The `start`
  // time must be strictly less than the `end` time, otherwise the TimeWindow
  // is empty/invalid (i.e. has zero duration).
  TimeWindow(base::Time start, base::Time end)
      : start_(start), end_(end) {}

  // Copyable and moveable.
  TimeWindow(const TimeWindow& other) = default;
  TimeWindow(TimeWindow&& other) = default;
  TimeWindow& operator=(const TimeWindow& other) = default;
  TimeWindow& operator=(TimeWindow&& other) = default;

  // Returns the start and end times of the TimeWindow. These times are
  // best-effort network times.
  base::Time start() const { return start_; }
  base::Time end() const { return end_; }

  // Returns true if the TimeWindow is valid (i.e. the start time is less than
  // the end time).
  bool IsValid() const { return start_ < end_; }

  // Returns true if the `time` is within the TimeWindow and the TimeWindow is
  // valid (non-empty).
  bool Contains(base::Time time) const {
    return (start_ <= time) && (time <= end_) && (start_ < end_);
  }

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

// Associates a VariationID value with a FieldTrial group (denoted by
// `trial_name` and `group_name`) for collection `key`. If an ID was previously
// set for `trial_name` and `group_name`, it is overwritten. This must be called
// whenever a FieldTrial is prepared (create the trial and append groups) and
// needs to have a VariationID associated with it so that Google servers can
// recognize the FieldTrial. The transmission of the VariationID will be limited
// to the `time_window`. Thread safe.
COMPONENT_EXPORT(VARIATIONS)
void AssociateGoogleVariationID(IDCollectionKey key,
                                std::string_view trial_name,
                                std::string_view group_name,
                                VariationID variation_id,
                                TimeWindow time_window = TimeWindow());

// Alias for the above function, but for testing. All test uses should be via
// this function. Once that is the case, the above function will be restricted
// to call sites that are approved to use it.
COMPONENT_EXPORT(VARIATIONS)
void AssociateGoogleVariationIDForTesting(
    IDCollectionKey key,
    std::string_view trial_name,
    std::string_view group_name,
    VariationID variation_id,
    TimeWindow time_window = TimeWindow());

// As above, but takes an ActiveGroupId hash pair, rather than the string names.
COMPONENT_EXPORT(VARIATIONS)
void AssociateGoogleVariationID(
    IDCollectionKey key,
    ActiveGroupId active_group_id,
    VariationID variation_id,
    TimeWindow time_window = TimeWindow());

// Retrieves the VariationID associated with a FieldTrial group (denoted by
// `trial_name` and `group_name`) for collection `key`. Returns EMPTY_ID if
// there is currently no associated ID for the given group. This API can be
// nicely combined with FieldTrial::GetActiveFieldTrialGroups() to enumerate the
// VariationIDs for all active FieldTrial groups. If a `current_time` is
// provided, the VariationID is returned only if the current time is between the
// (inclusive) start and end timestamps of the TimeWindow for that VariationID.
// Thread safe.
COMPONENT_EXPORT(VARIATIONS)
VariationID GetGoogleVariationID(
    IDCollectionKey key,
    std::string_view trial_name,
    std::string_view group_name,
    std::optional<base::Time> current_time = std::nullopt);

// Same as GetGoogleVariationID(), but takes in a hashed `active_group_id`
// rather than the string names.
COMPONENT_EXPORT(VARIATIONS)
VariationID GetGoogleVariationID(
    IDCollectionKey key,
    ActiveGroupId active_group_id,
    std::optional<base::Time> current_time = std::nullopt);

// Returns the next time after the given time that a time window will start or
// end for a VariationID.
COMPONENT_EXPORT(VARIATIONS)
base::Time GetNextTimeWindowEvent(base::Time time);

// Expose some functions for testing.
namespace test {

// Clears all of the mapped associations. Deprecated, use ScopedFeatureList
// instead as it does a lot of work for you automatically.
COMPONENT_EXPORT(VARIATIONS) void ClearAllVariationIDs();

// Clears all of the associated params. Deprecated, use ScopedFeatureList
// instead as it does a lot of work for you automatically.
COMPONENT_EXPORT(VARIATIONS) void ClearAllVariationParams();
}  // namespace test
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_ASSOCIATED_DATA_H_
