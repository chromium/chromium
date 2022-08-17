// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_UTILS_H_

#include <iterator>
#include <list>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile_import_process.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace autofill {

// Encapsulates a list of Ts, ordered by the time they were added (newest
// first). All Ts share the same `origin`. This is useful for tracking
// relationships between submitted forms on the same origin, within a small
// period of time.
template <class T>
class TimestampedSameOriginQueue {
 public:
  // The queue stores Ts augmented with a timestamp.
  struct value_type : public T {
    value_type(T t, base::Time timestamp)
        : T(std::move(t)), timestamp(std::move(timestamp)) {}

    const base::Time timestamp;
  };
  using const_iterator = typename std::list<value_type>::const_iterator;

  // Pushes `item` at the current timestamp.
  void Push(T item, const url::Origin& item_origin) {
    DCHECK(!origin_ || *origin_ == item_origin);
    items_.emplace_front(std::move(item), AutofillClock::Now());
    origin_ = item_origin;
  }

  // Removes the oldest element from the queue.
  void Pop() { erase(std::prev(end()), end()); }

  // Removes all `items` from a different `origin` or older than `ttl`.
  // This is not done as part of `Push()`, as outdated items (for example in the
  // multi-step import use-case) should be deleted as soon as possible for
  // privacy reasons, even when no `Push()` happens.
  void RemoveOutdatedItems(const base::TimeDelta& ttl,
                           const url::Origin& new_origin) {
    if (origin_ && *origin_ != new_origin) {
      Clear();
    } else {
      const base::Time now = AutofillClock::Now();
      while (!empty() && now - items_.back().timestamp > ttl)
        Pop();
    }
  }

  // Returns the origin shared by the elements in the queue. Or nullopt, if
  // the queue is currently `Empty()`.
  const absl::optional<url::Origin>& Origin() const { return origin_; }

  size_t size() const { return items_.size(); }
  bool empty() const { return items_.empty(); }

  // Removes the items [first, last[.
  void erase(const_iterator first, const_iterator last) {
    items_.erase(first, last);
    if (empty())
      origin_.reset();
  }

  void Clear() { erase(begin(), end()); }

  // The elements are ordered from newest to latest.
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }

 private:
  std::list<value_type> items_;
  // If the queue is not `empty()`, this represents the origin of all `items_`.
  absl::optional<url::Origin> origin_;
};

// Returns true if minimum requirements for import of a given `profile` have
// been met. An address submitted via a form must have at least the fields
// required as determined by its country code.
// No verification of validity of the contents is performed. This is an
// existence check only.
bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& predicted_country_code,
                      const std::string& app_locale,
                      LogBuffer* import_log_buffer,
                      bool collect_metrics);

// Tries to infer the country `profile` is from, which can be useful to
// verify whether the data is sensible. Returns a two-letter ISO country code
// by considering, in decreasing order of priority:
// - The country specified in `profile`.
// - The country determined by the variation service stored in
//   `variation_country_code`.
// - The country code corresponding to `app_locale`.
std::string GetPredictedCountryCode(const AutofillProfile& profile,
                                    const std::string& variation_country_code,
                                    const std::string& app_locale,
                                    LogBuffer* import_log_buffer);

// Stores recently submitted profile fragments, which are merged against future
// import candidates to construct a complete profile. This enables importing
// from multi-step import flows.
class MultiStepImportMerger {
 public:
  MultiStepImportMerger(const std::string& app_locale,
                        const std::string& variation_country_code);
  ~MultiStepImportMerger();

  // Removes updated multi-step candidates, merges `profile` with multi-step
  // candidates and potentially stores it as a multi-step candidate itself.
  // `profile` and `import_metadata` are updated accordingly, if the profile
  // can be merged. See `MergeProfileWithMultiStepCandidates()` for details.
  // Only applicable when `kAutofillEnableMultiStepImports` is enabled.
  void ProcessMultiStepImport(AutofillProfile& profile,
                              ProfileImportMetadata& import_metadata,
                              const url::Origin& origin);

  const absl::optional<url::Origin>& Origin() const {
    return multistep_candidates_.Origin();
  }

  void Clear() { multistep_candidates_.Clear(); }

 private:
  // Merges a given `profile` stepwise with `multistep_candidates_` to
  // complete it. `profile` is assumed to contain no invalid information.
  // Returns true if the resulting profile satisfies the minimum address
  // requirements. `profile` and `import_metadata` are updated in this case
  // with the result of merging all relevant candidates.
  // Returns false otherwise and leaves `profile` and `import_metadata`
  // unchanged. Any merged or colliding `multistep_candidates_` are cleared.
  // `origin`: The origin of the form where `profile` was imported from.
  bool MergeProfileWithMultiStepCandidates(
      AutofillProfile& profile,
      ProfileImportMetadata& import_metadata,
      const url::Origin& origin);

  // Needed to predict the country code of a merged import candidate, to
  // ultimately decide if the profile meets the minimum import requirements.
  std::string app_locale_;
  std::string variation_country_code_;

  // Represents a submitted form, stored to be considered as a merge candidate
  // for other candidate profiles in future submits in a multi-step import
  // flow.
  struct MultiStepFormProfileCandidate {
    // The import candidate.
    AutofillProfile profile;
    // Metadata about how `profile` was constructed.
    ProfileImportMetadata import_metadata;
  };
  TimestampedSameOriginQueue<MultiStepFormProfileCandidate>
      multistep_candidates_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_UTILS_H_
