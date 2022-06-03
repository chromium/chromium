// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_
#define COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_

#include "base/time/time.h"
#include "base/version.h"
#include "components/prefs/prefs_export.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom-forward.h"

#include <stdint.h>

#include <string>
#include <unordered_set>

class PrefRegistrySimple;
class PrefService;

namespace federated_learning {

// ID used to represent a cohort of people with similar browsing habits. For
// more context, see the explainer at
// https://github.com/jkarlin/floc/blob/master/README.md
class FlocId {
 public:
  enum class Status {
    kValid = 0,

    // The default invalid reason that a fresh profile comes with. An
    // outstanding cohort can also have this state after a prefs update where
    // the previous reason is unknown or ambiguous.
    kInvalidNoStatusPrefs = 1,

    // The cohort computation is ready to run, but is waiting for other signals
    // to start (e.g. sorting-lsh file is ready).
    kInvalidWaitingToStart = 2,

    kInvalidDisallowedByUserSettings = 3,
    kInvalidNotEnoughElgibleHistoryDomains = 4,

    // Set if the sorting-lsh process returns an empty hash value. This often
    // implies that the cohort is blocked, but in principle can also be due to
    // file sanitization errors.
    // TODO(yaoxia): consider distinguishing between the underlying reasons.
    kInvalidBlocked = 5,

    kInvalidHistoryDeleted = 6,

    // This reason is set when the user deletes cookies or resets cohort through
    // a dedicated FLoC settings page until it's time to calculate at the next
    // epoch.
    kInvalidReset = 7,
  };

  static uint64_t SimHashHistory(
      const std::unordered_set<std::string>& domains);

  // Create an invalid floc. The `finch_config_version_` and the `compute_time_`
  // will be set to the current.
  static FlocId CreateInvalid(Status status);

  // Create a newly computed and valid floc. The `finch_config_version_` and
  // the `compute_time_` will be set to the current.
  static FlocId CreateValid(uint64_t id,
                            base::Time history_begin_time,
                            base::Time history_end_time,
                            uint32_t sorting_lsh_version);

  FlocId(const FlocId& id);
  ~FlocId();
  FlocId& operator=(const FlocId& id);
  FlocId& operator=(FlocId&& id);

  bool operator==(const FlocId& other) const;
  bool operator!=(const FlocId& other) const;

  // True if the floc is successfully computed and hasn't been invalidated
  // since the last computation. Note that an invalid FlocId still often has a
  // legitimate compute time and finch config version, unless it's read from a
  // fresh profile prefs.
  bool IsValid() const;

  // Get the blink::mojom::InterestCohort representation of this floc, with
  // interest_cohort.id being "<id>" and interest_cohort.version being
  // "chrome.<finch_config_version>.<sorting_lsh_version>". This is the format
  // to be exposed to the JS API. Precondition: the floc must be valid.
  blink::mojom::InterestCohortPtr ToInterestCohortForJsApi() const;

  // Returns the internal uint64_t number. Precondition: the floc must be valid.
  uint64_t ToUint64() const;

  Status status() const { return status_; }

  base::Time history_begin_time() const { return history_begin_time_; }

  base::Time history_end_time() const { return history_end_time_; }

  uint32_t finch_config_version() const { return finch_config_version_; }

  uint32_t sorting_lsh_version() const { return sorting_lsh_version_; }

  base::Time compute_time() const { return compute_time_; }

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void SaveToPrefs(PrefService* prefs);
  static FlocId ReadFromPrefs(PrefService* prefs);

  // Update `status_` and the corresponding prefs entry. This can be used to
  // invalidate the floc or to assign a new invalid reason.
  void UpdateStatusAndSaveToPrefs(PrefService* prefs, Status status);

  // Resets `compute_time_` to provided `compute_time` and saves it to prefs.
  // This should at least be called if the floc compute timer is reset, to
  // ensure that the compute cycle continues at the expected frequency.
  void ResetComputeTimeAndSaveToPrefs(base::Time compute_time,
                                      PrefService* prefs);

 private:
  friend class FlocIdTester;

  // Create a floc with stated params. This will only be used to create a floc
  // read from prefs.
  explicit FlocId(uint64_t id,
                  Status status,
                  base::Time history_begin_time,
                  base::Time history_end_time,
                  uint32_t finch_config_version,
                  uint32_t sorting_lsh_version,
                  base::Time compute_time);

  uint64_t id_ = 0;

  Status status_;

  // The time range of the actual history used to compute the floc. This should
  // always be within the time range of each history query.
  base::Time history_begin_time_;
  base::Time history_end_time_;

  // The kFlocIdFinchConfigVersion feature param. When floc is loaded from
  // prefs, this could be different from the current feature param state.
  uint32_t finch_config_version_ = 0;

  // The main version (i.e. 1st int) of the sorting lsh component version.
  uint32_t sorting_lsh_version_ = 0;

  // The time when the floc was computed. compute_time_.is_null() means the
  // floc has never been computed before, and implies that the floc is also
  // invalid.
  base::Time compute_time_;
};

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_
