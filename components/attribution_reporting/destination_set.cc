// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/destination_set.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

absl::optional<net::SchemefulSite> DeserializeDestination(
    const std::string& s) {
  auto destination = SuitableOrigin::Deserialize(s);
  if (!destination.has_value()) {
    return absl::nullopt;
  }
  return net::SchemefulSite(*destination);
}

bool DestinationsValid(const DestinationSet::Destinations& destinations) {
  return !destinations.empty() && destinations.size() <= kMaxDestinations &&
         base::ranges::all_of(destinations, &IsSitePotentiallySuitable);
}

}  // namespace

// static
absl::optional<DestinationSet> DestinationSet::Create(
    Destinations destinations) {
  if (!DestinationsValid(destinations)) {
    return absl::nullopt;
  }
  return DestinationSet(std::move(destinations));
}

// static
base::expected<DestinationSet, SourceRegistrationError>
DestinationSet::FromJSON(const base::Value* v) {
  if (!v) {
    return base::unexpected(SourceRegistrationError::kDestinationMissing);
  }

  std::vector<net::SchemefulSite> destination_sites;
  if (const std::string* str = v->GetIfString()) {
    auto destination = DeserializeDestination(*str);
    if (!destination.has_value()) {
      return base::unexpected(
          SourceRegistrationError::kDestinationUntrustworthy);
    }

    destination_sites.push_back(std::move(*destination));
  } else if (const base::Value::List* list = v->GetIfList()) {
    if (list->size() > kMaxDestinations) {
      return base::unexpected(SourceRegistrationError::kDestinationListTooLong);
    }
    if (list->empty()) {
      return base::unexpected(SourceRegistrationError::kDestinationMissing);
    }
    destination_sites.reserve(list->size());

    for (const auto& item : *list) {
      const std::string* item_str = item.GetIfString();
      if (!item_str) {
        return base::unexpected(SourceRegistrationError::kDestinationWrongType);
      }
      auto destination = DeserializeDestination(*item_str);
      if (!destination.has_value()) {
        return base::unexpected(
            SourceRegistrationError::kDestinationUntrustworthy);
      }

      destination_sites.push_back(std::move(*destination));
    }
  } else {
    return base::unexpected(SourceRegistrationError::kDestinationWrongType);
  }
  return DestinationSet(std::move(destination_sites));
}

DestinationSet::DestinationSet(Destinations destinations)
    : destinations_(std::move(destinations)) {
  DCHECK(DestinationsValid(destinations_));
}

DestinationSet::DestinationSet() = default;

DestinationSet::~DestinationSet() = default;

DestinationSet::DestinationSet(const DestinationSet&) = default;

DestinationSet::DestinationSet(DestinationSet&&) = default;

DestinationSet& DestinationSet::operator=(const DestinationSet&) = default;

DestinationSet& DestinationSet::operator=(DestinationSet&&) = default;

bool DestinationSet::IsValid() const {
  return DestinationsValid(destinations_);
}

base::Value DestinationSet::ToJson() const {
  DCHECK(IsValid());
  if (destinations_.size() == 1) {
    return base::Value(destinations_.begin()->Serialize());
  }

  base::Value::List list;
  for (const auto& destination : destinations_) {
    list.Append(destination.Serialize());
  }
  return base::Value(std::move(list));
}

}  // namespace attribution_reporting
