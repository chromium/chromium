// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_DATA_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_DATA_MODEL_H_

#include <stddef.h>

#include <optional>

#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/form_group.h"

namespace autofill {

// This class is an interface for the primary data models that back Autofill.
// The information in objects of this class is managed by the
// PersonalDataManager.
class AutofillDataModel : public FormGroup {
 public:
  ~AutofillDataModel() override;
  AutofillDataModel(const AutofillDataModel&);

  // Calculates the number of days since the model was last used by subtracting
  // the model's last recent |use_date_| from the |current_time|.
  int GetDaysSinceLastUse(base::Time current_time) const;

  size_t use_count() const { return use_count_; }
  void set_use_count(size_t count) { use_count_ = count; }

  size_t usage_history_size() const { return usage_history_size_; }

  // Returns the `i`-th last use date for `1 <= i <= usage_history_size()`.
  // If a model hasn't been used at least `i` times, a null time is returned
  // instead.
  std::optional<base::Time> use_date(size_t i) const;
  // Returns the last use date. As creation counts as a use, it's never nullopt.
  base::Time use_date() const { return *use_date(1); }

  // Setter with the same semantics as `use_date()`. In particular, only the
  // `i`-th use date is changed - other use dates are not affected.
  void set_use_date(std::optional<base::Time> time, size_t i = 1);

  // Records a new use of the model, by updating the last used date to `time`
  // and shifting the existing use dates backwards.
  void RecordUseDate(base::Time time);

  bool UseDateEqualsInSeconds(const AutofillDataModel* other) const;

  base::Time modification_date() const { return modification_date_; }
  void set_modification_date(base::Time time) { modification_date_ = time; }

  // Compares two data models according to their ranking score. The score uses
  // a combination of use count and days since last use to determine the
  // relevance of the profile. A greater ranking score corresponds to a higher
  // ranking on the suggestion list.
  // The function defines a strict weak ordering that can be used for sorting.
  // Since data models can have the same score, it doesn't define a total order.
  // `comparison_time_` allows consistent sorting throughout the comparisons.
  bool HasGreaterRankingThan(const AutofillDataModel* other,
                             base::Time comparison_time) const;

  // Given two ranking scores for two data model suggestions, returns if `score`
  // is greater than `other_score`. In the case of a tie-breaker, uses the most
  // recent use date as the winner.
  virtual bool CompareRankingScores(double score,
                                    double other_score,
                                    base::Time other_use_date) const;

 protected:
  explicit AutofillDataModel(size_t usage_history_size = 1);

  // Calculate the ranking score of a card or profile depending on their use
  // count and most recent use date.
  double GetRankingScore(base::Time current_time) const;

  // Merges the use dates of `*this` and `other` into `*this*` by choosing the
  // most recent use dates.
  void MergeUseDates(const AutofillDataModel& other);

 private:
  // The number of times this model has been used.
  size_t use_count_ = 1;

  // The last `usage_history_size_` many use dates of the model are tracked in
  // `use_dates`, which is guaranteed to have size `usage_history_size_`.
  // `use_dates_[0]` represents the last use date, `use_dates_[1]` the second to
  // last use date, etc. A nullopt value means that the model hasn't been used
  // this often. Since creation counts as a use, `use_dates_[0]` is never
  // nullopt.
  size_t usage_history_size_;
  std::vector<std::optional<base::Time>> use_dates_;

  // The last time data in the model was modified, rounded in seconds. Any
  // change should use `set_modification_date()`.
  base::Time modification_date_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_DATA_MODEL_H_
