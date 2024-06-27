// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_UTILS_H_

#include <iterator>
#include <limits>
#include <list>
#include <optional>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_profile_import_process.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"
#include "components/history/core/browser/history_types.h"

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
  using iterator = typename std::list<value_type>::iterator;
  using const_iterator = typename std::list<value_type>::const_iterator;

  explicit TimestampedSameOriginQueue(
      size_t max_size = std::numeric_limits<size_t>::max())
      : max_size_(max_size) {}

  // Pushes `item` at the current timestamp.
  void Push(T item, const url::Origin& item_origin) {
    DCHECK(!origin_ || *origin_ == item_origin);
    items_.emplace_front(std::move(item), AutofillClock::Now());
    origin_ = item_origin;
    if (size() - 1 == max_size_)
      Pop();
  }

  // Removes the oldest element from the queue.
  void Pop() { erase(std::prev(end()), end()); }

  // Removes all `items` from a different `origin` or older than `ttl`.
  // This is not done as part of `Push()`, as outdated items (for example in the
  // multi-step import use-case) should be deleted as soon as possible for
  // privacy reasons, even when no `Push()` happens.
  void RemoveOutdatedItems(base::TimeDelta ttl, const url::Origin& new_origin) {
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
  const std::optional<url::Origin>& origin() const { return origin_; }

  size_t size() const { return items_.size(); }
  bool empty() const { return items_.empty(); }

  // Removes the items [first, last[. Iterators to erased elements are
  // invalidated. Other iterators are not affected.
  void erase(const_iterator first, const_iterator last) {
    items_.erase(first, last);
    if (empty())
      origin_.reset();
  }

  void Clear() { erase(begin(), end()); }

  // The elements are ordered from newest to latest.
  iterator begin() { return items_.begin(); }
  iterator end() { return items_.end(); }
  const_iterator begin() const { return items_.begin(); }
  const_iterator end() const { return items_.end(); }

 private:
  std::list<value_type> items_;
  // If the queue is not `empty()`, this represents the origin of all `items_`.
  std::optional<url::Origin> origin_;
  // The maximum number of elements stored in `items_`. If adding a new item
  // would exceed the `max_size_`, the oldest existing item is removed.
  const size_t max_size_;
};

// Stores recently submitted profile fragments, which are merged against future
// import candidates to construct a complete profile. This enables importing
// from multi-step import flows.
class MultiStepImportMerger {
 public:
  MultiStepImportMerger(const std::string& app_locale,
                        const GeoIpCountryCode& variation_country_code);
  ~MultiStepImportMerger();

  // Removes updated multi-step candidates, merges `profile` with multi-step
  // candidates and potentially stores it as a multi-step candidate itself.
  // `profile` and `import_metadata` are updated accordingly, if the profile
  // can be merged. See `MergeProfileWithMultiStepCandidates()` for details.
  void ProcessMultiStepImport(AutofillProfile& profile,
                              ProfileImportMetadata& import_metadata);

  void AddMultiStepImportCandidate(const AutofillProfile& profile,
                                   const ProfileImportMetadata& import_metadata,
                                   bool is_imported);

  const std::optional<url::Origin>& origin() const {
    return multistep_candidates_.origin();
  }

  void Clear() { multistep_candidates_.Clear(); }

  // Stored profiles can be deleted/modified by the user/through sync, etc. This
  // potentially invalidates `multistep_candidates_`.
  // This function verifies all already imported `multistep_candidates_`
  // against the corresponding profile stored in the `address_data_manager`.
  // Profiles that no longer exist in the address data manager are removed from
  // `multistep_candidates`. Similarly, profiles that were in modified in the
  // address data manager are updated in `multistep_candidates`.
  void OnAddressDataChanged(AddressDataManager& address_data_manager);

  void OnBrowsingHistoryCleared(const history::DeletionInfo& deletion_info);

 private:
  // Merges a given `profile` stepwise with `multistep_candidates_` to
  // complete it. `profile` is assumed to contain no invalid information.
  // Returns true if the resulting profile satisfies the minimum address
  // requirements. `profile` and `import_metadata` are updated in this case
  // with the result of merging all relevant candidates.
  // Returns false otherwise and leaves `profile` and `import_metadata`
  // unchanged. Any merged or colliding `multistep_candidates_` are cleared.
  bool MergeProfileWithMultiStepCandidates(
      AutofillProfile& profile,
      ProfileImportMetadata& import_metadata);

  // `ProfileImportMetadata` is used to log metrics on the user decision,
  // depending on features like invalid phone number removal. When combining two
  // profile fragments, we need to decide which features had an effect on the
  // resulting profile. This function does so by merging `source` into `target`.
  void MergeImportMetadata(const ProfileImportMetadata& source,
                           ProfileImportMetadata& target) const;

  // Needed to predict the country code of a merged import candidate, to
  // ultimately decide if the profile meets the minimum import requirements.
  std::string app_locale_;
  GeoIpCountryCode variation_country_code_;
  AutofillProfileComparator comparator_;

  // Represents a submitted form, stored to be considered as a merge candidate
  // for other candidate profiles in future submits in a multi-step import
  // flow.
  struct MultiStepFormProfileCandidate {
    // The import candidate.
    AutofillProfile profile;
    // Metadata about how `profile` was constructed.
    ProfileImportMetadata import_metadata;
    // Whether the profile is already imported and only stored for multi-step
    // complements.
    bool is_imported;
  };
  TimestampedSameOriginQueue<MultiStepFormProfileCandidate>
      multistep_candidates_;
};

// Tracks which address and credit card forms were submitted in close temporal
// proximity.
class FormAssociator {
 public:
  enum class FormType {
    kAddressForm,
    kCreditCardForm,
  };

  FormAssociator();
  ~FormAssociator();

  // Removes outdated (different `origin` or TTL) form signatures from
  // `recent_credit_card_forms_` and `recent_address_forms_`. Adds
  // `form_signature` to the appropriate list (depending on `form_type`).
  void TrackFormAssociations(const url::Origin& origin,
                             FormSignature form_signature,
                             FormType form_type);

  // Returns the form signatures that are associated with `form_signature`, if
  // any. In particular, the two most recent address and the most recent
  // submitted credit card form signatures from the same origin are returned.
  // One of them is the `form_signature` itself.
  std::optional<FormStructure::FormAssociations> GetFormAssociations(
      FormSignature form_signature) const;

  const std::optional<url::Origin>& origin() const;

  void Clear();

  void OnBrowsingHistoryCleared(const history::DeletionInfo& deletion_info);

 private:
  // Stores the two most recent address form signatures and the most recent
  // credit card form signature submitted, within a small TTL and sharing the
  // same origin.
  TimestampedSameOriginQueue<FormSignature> recent_address_forms_{
      /*max_size=*/2u};
  TimestampedSameOriginQueue<FormSignature> recent_credit_card_forms_{
      /*max_size=*/1u};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_UTILS_H_
