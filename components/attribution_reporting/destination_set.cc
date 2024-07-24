// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/destination_set.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/overloaded.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/source_registration_error.mojom-shared.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "net/base/schemeful_site.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationError;

bool DestinationsValid(const DestinationSet::Destinations& destinations) {
  return !destinations.empty() && destinations.size() <= kMaxDestinations &&
         base::ranges::all_of(destinations, &IsSitePotentiallySuitable);
}

}  // namespace

// static
std::optional<DestinationSet> DestinationSet::Create(
    Destinations destinations) {
  if (!DestinationsValid(destinations)) {
    return std::nullopt;
  }
  return DestinationSet(std::move(destinations));
}

// static
base::expected<DestinationSet, SourceRegistrationError>
DestinationSet::FromJSON(const base::Value* v) {
  if (!v) {
    return base::unexpected(SourceRegistrationError::kDestinationMissing);
  }

  // Although we build this set iteratively, which results in O(n^2)
  // construction, n is very small, so this is fine.
  static_assert(kMaxDestinations == 3,
                "Consider using more performant set construction if the size "
                "limit increases.");
  base::flat_set<net::SchemefulSite> destination_sites;

  using AppendIfValidResult = base::expected<void, SourceRegistrationError>;

  const auto append_if_valid =
      [&](const std::string& str,
          SourceRegistrationError error) -> AppendIfValidResult {
    auto origin = SuitableOrigin::Deserialize(str);
    if (!origin.has_value()) {
      return base::unexpected(error);
    }
    destination_sites.emplace(*origin);
    return base::ok();
  };

  RETURN_IF_ERROR(v->Visit(base::Overloaded{
      [&](const std::string& str) {
        return append_if_valid(
            str, SourceRegistrationError::kDestinationUntrustworthy);
      },
      [&](const base::Value::List& list) -> AppendIfValidResult {
        if (list.empty()) {
          return base::unexpected(
              SourceRegistrationError::kDestinationWrongType);
        }

        destination_sites.reserve(std::min(list.size(), kMaxDestinations));

        for (const auto& item : list) {
          const std::string* str = item.GetIfString();
          if (!str) {
            return base::unexpected(
                SourceRegistrationError::kDestinationWrongType);
          }
          RETURN_IF_ERROR(append_if_valid(
              *str, SourceRegistrationError::kDestinationListUntrustworthy));

          if (destination_sites.size() > kMaxDestinations) {
            return base::unexpected(
                SourceRegistrationError::kDestinationWrongType);
          }
        }

        return base::ok();
      },
      [](const auto&) -> AppendIfValidResult {
        return base::unexpected(SourceRegistrationError::kDestinationWrongType);
      },
  }));

  return DestinationSet(std::move(destination_sites));
}

DestinationSet::DestinationSet(Destinations destinations)
    : destinations_(std::move(destinations)) {
  CHECK(IsValid());
}

DestinationSet::DestinationSet(mojo::DefaultConstruct::Tag) {
  CHECK(!IsValid());
}

DestinationSet::~DestinationSet() = default;

DestinationSet::DestinationSet(const DestinationSet&) = default;

DestinationSet::DestinationSet(DestinationSet&&) = default;

DestinationSet& DestinationSet::operator=(const DestinationSet&) = default;

DestinationSet& DestinationSet::operator=(DestinationSet&&) = default;

bool DestinationSet::IsValid() const {
  return DestinationsValid(destinations_);
}

base::Value DestinationSet::ToJson() const {
  CHECK(IsValid());
  if (destinations_.size() == 1) {
    return base::Value(destinations_.begin()->Serialize());
  }

  auto list = base::Value::List::with_capacity(destinations_.size());
  for (const auto& destination : destinations_) {
    list.Append(destination.Serialize());
  }
  return base::Value(std::move(list));
}

}  // namespace attribution_reporting
