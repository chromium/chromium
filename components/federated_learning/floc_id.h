// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_
#define COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_

#include "base/optional.h"
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
  static uint64_t SimHashHistory(
      const std::unordered_set<std::string>& domains);

  // Create a newly computed but invalid floc (which implies permission errors,
  // insufficient eligible history, or blocked floc). The
  // |finch_config_version_| and the |compute_time_| will be set to the current.
  FlocId();

  // Create a newly computed and valid floc. The |finch_config_version_| and
  // the |compute_time_| will be set to the current.
  explicit FlocId(uint64_t id,
                  base::Time history_begin_time,
                  base::Time history_end_time,
                  uint32_t sorting_lsh_version);

  FlocId(const FlocId& id);
  ~FlocId();
  FlocId& operator=(const FlocId& id);
  FlocId& operator=(FlocId&& id);

  bool operator==(const FlocId& other) const;
  bool operator!=(const FlocId& other) const;

  // True if the |id_| is successfully computed and hasn't been invalidated
  // since the last computation. Note that an invalid FlocId still often has a
  // legitimate compute time and finch config version, unless it's read from a
  // fresh profile prefs.
  bool IsValid() const;

  // Get the blink::mojom::InterestCohort representation of this floc, with
  // interest_cohort.id being "<id>" and interest_cohort.version being
  // "chrome.<finch_config_version>.<sorting_lsh_version>". This is the format
  // to be exposed to the JS API. Precondition: |id_| must be valid.
  blink::mojom::InterestCohortPtr ToInterestCohortForJsApi() const;

  // Returns the internal uint64_t number. Precondition: |id_| must be valid.
  uint64_t ToUint64() const;

  base::Time history_begin_time() const { return history_begin_time_; }

  base::Time history_end_time() const { return history_end_time_; }

  uint32_t finch_config_version() const { return finch_config_version_; }

  uint32_t sorting_lsh_version() const { return sorting_lsh_version_; }

  base::Time compute_time() const { return compute_time_; }

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void SaveToPrefs(PrefService* prefs);
  static FlocId ReadFromPrefs(PrefService* prefs);

  // Reset |id_| and clear the prefs corresponding to the id. This assumes the
  // current floc is already in sync with the prefs and we don't need to save
  // other unaffected field.
  void InvalidateIdAndSaveToPrefs(PrefService* prefs);

 private:
  friend class FlocIdTester;

  // Create a floc with stated params. This will only be used to create a floc
  // read from prefs.
  explicit FlocId(base::Optional<uint64_t> id,
                  base::Time history_begin_time,
                  base::Time history_end_time,
                  uint32_t finch_config_version,
                  uint32_t sorting_lsh_version,
                  base::Time compute_time);

  base::Optional<uint64_t> id_;

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
  // floc has never been computed before, and implies |id_| is also invalid.
  base::Time compute_time_;
};

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_
