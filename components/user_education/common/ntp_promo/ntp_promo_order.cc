// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/ntp_promo/ntp_promo_order.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/ntp_promo/ntp_promo_specification.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_storage_service.h"

namespace user_education {

namespace {

using ShowAfterMap = std::map<NtpPromoIdentifier, std::set<NtpPromoIdentifier>>;

constexpr int kTopRank = 1;

// Helper to check whether all a promo's show-after dependencies are already
// satisfied.
bool DependenciesSatisfied(const NtpPromoIdentifier& id,
                           const ShowAfterMap& show_after_map,
                           const std::set<NtpPromoIdentifier>& satisfied) {
  const auto it = show_after_map.find(id);
  if (it == show_after_map.end()) {
    return true;
  }
  std::vector<NtpPromoIdentifier> difference;
  std::set_difference(it->second.begin(), it->second.end(), satisfied.begin(),
                      satisfied.end(), std::back_inserter(difference));
  return difference.empty();
}

// Helper struct to facilitate sorting pending promos.
struct SortablePendingPromo {
  NtpPromoIdentifier id;
  int rank = 0;
  int last_top_spot_session = 0;
  int top_spot_session_count = 0;
};

// Helper struct to facilitate sorting pending promos.
struct SortableCompletedPromo {
  NtpPromoIdentifier id;
  base::Time completed;
};

}  // namespace

NtpPromoOrderPolicy::NtpPromoOrderPolicy(
    const NtpPromoRegistry& registry,
    const UserEducationStorageService& storage_service,
    int num_sessions_between_rotation)
    : registry_(registry),
      storage_service_(storage_service),
      num_sessions_between_rotation_(num_sessions_between_rotation) {}

NtpPromoOrderPolicy::~NtpPromoOrderPolicy() = default;

std::vector<NtpPromoIdentifier> NtpPromoOrderPolicy::OrderPendingPromos(
    const std::vector<NtpPromoIdentifier>& ids) {
  if (ids.size() <= 1) {
    return ids;
  }

  // Build a dependency map. This is based on the registry, but excludes
  // dependencies on promos that aren't in the input list (ie. aren't being
  // shown for any reason).
  ShowAfterMap show_after_map;
  for (const auto& id : ids) {
    const auto* spec = registry_->GetNtpPromoSpecification(id);
    CHECK(spec);
    for (const auto& after : spec->show_after()) {
      if (base::Contains(ids, after)) {
        show_after_map[id].insert(after);
      }
    }
  }

  // Construct a sortable list of promo ordering objects.
  std::vector<SortablePendingPromo> promos;
  promos.reserve(ids.size());
  for (const auto& id : ids) {
    const auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());
    promos.push_back(SortablePendingPromo{
        .id = id,
        .last_top_spot_session = prefs.last_top_spot_session,
        .top_spot_session_count = prefs.top_spot_session_count,
    });
  }

  // Group promos into 1-based ranks, according to dependencies. Any promo that
  // must show after another promo must be ranked with a higher number. Rank 1
  // is the top rank.
  std::set<NtpPromoIdentifier> ranked_ids;
  for (int rank = kTopRank; ranked_ids.size() < ids.size(); rank++) {
    std::set<NtpPromoIdentifier> newly_ranked_ids;
    for (auto& promo : promos) {
      if (!promo.rank &&
          DependenciesSatisfied(promo.id, show_after_map, ranked_ids)) {
        promo.rank = rank;
        newly_ranked_ids.insert(promo.id);
      }
    }
    // If this fails, there's a circular dependency.
    CHECK(!newly_ranked_ids.empty());
    ranked_ids.insert(newly_ranked_ids.begin(), newly_ranked_ids.end());
  }

  // Sort promos by rank, and then by least-recently shown.
  std::stable_sort(promos.begin(), promos.end(),
                   [](const SortablePendingPromo& a,
                      const SortablePendingPromo& b) {
                     return a.rank < b.rank ||
                            (a.rank == b.rank &&
                             a.last_top_spot_session < b.last_top_spot_session);
                   });

  // If the most-recently-shown top-ranked promo hasn't been shown long enough,
  // elevate it to the top of the list to be shown again. Essentially, this just
  // removes that one element and inserts it at the top of the list. In
  // practice, it uses a C++ container rotation to do that more efficiently than
  // extracting and inserting at the beginning. Since we're searching from the
  // bottom, use a reverse iterator to find the last top-ranked promo, and
  // rotate it to the "end" of the reversed order (ie. the start).
  auto it_r = std::find_if(
      promos.rbegin(), promos.rend(),
      [](const SortablePendingPromo& p) { return (p.rank == kTopRank); });
  CHECK(it_r != promos.rend());
  if (it_r->top_spot_session_count > 0 &&
      it_r->top_spot_session_count < num_sessions_between_rotation_) {
    std::rotate(it_r, it_r + 1, promos.rend());
  }

  // Distill and return the ordered list of IDs.
  std::vector<NtpPromoIdentifier> ordered_ids;
  ordered_ids.reserve(promos.size());
  std::transform(promos.begin(), promos.end(), std::back_inserter(ordered_ids),
                 [](const SortablePendingPromo& promo) { return promo.id; });
  return ordered_ids;
}

std::vector<NtpPromoIdentifier> NtpPromoOrderPolicy::OrderCompletedPromos(
    const std::vector<NtpPromoIdentifier>& ids) {
  // Construct a sortable list of promo ordering objects.
  std::vector<SortableCompletedPromo> promos;
  promos.reserve(ids.size());
  for (const auto& id : ids) {
    const auto prefs =
        storage_service_->ReadNtpPromoData(id).value_or(NtpPromoData());
    promos.push_back(SortableCompletedPromo{
        .id = id,
        .completed = prefs.completed,
    });
  }

  std::stable_sort(
      promos.begin(), promos.end(),
      [](const SortableCompletedPromo& a, const SortableCompletedPromo& b) {
        // Descending time order.
        return a.completed > b.completed;
      });

  // Distill and return the ordered list of IDs.
  std::vector<NtpPromoIdentifier> ordered_ids;
  ordered_ids.reserve(promos.size());
  std::transform(promos.begin(), promos.end(), std::back_inserter(ordered_ids),
                 [](const SortableCompletedPromo& promo) { return promo.id; });
  return ordered_ids;
}

}  // namespace user_education
