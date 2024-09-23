// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/aggregation_service/aggregation_coordinator_utils.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "components/aggregation_service/features.h"
#include "components/attribution_reporting/is_origin_suitable.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace aggregation_service {

namespace {

std::vector<url::Origin> DefaultOrigins() {
  return {url::Origin::Create(GURL(kDefaultAggregationCoordinatorAwsCloud)),
          url::Origin::Create(GURL(kDefaultAggregationCoordinatorGcpCloud))};
}

std::vector<url::Origin> Parse(std::string_view unparsed) {
  std::vector<url::Origin> parsed;

  std::vector<std::string_view> tokens = base::SplitStringPiece(
      unparsed, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string_view token : tokens) {
    auto origin = url::Origin::Create(GURL(token));
    if (!attribution_reporting::IsOriginSuitable(origin)) {
      return DefaultOrigins();
    }
    parsed.push_back(std::move(origin));
  }

  if (parsed.empty()) {
    return DefaultOrigins();
  }

  return parsed;
}

class CoordinatorOrigins {
 public:
  CoordinatorOrigins() = default;
  ~CoordinatorOrigins() = default;

  explicit CoordinatorOrigins(std::string_view unparsed)
      : CoordinatorOrigins(Parse(unparsed)) {}

  explicit CoordinatorOrigins(std::vector<url::Origin> origins)
      : origins_(std::move(origins)) {
    CHECK(origins_.empty() || IsValid());
  }

  CoordinatorOrigins(const CoordinatorOrigins&) = delete;
  CoordinatorOrigins& operator=(const CoordinatorOrigins&) = delete;

  CoordinatorOrigins(CoordinatorOrigins&&) = default;
  CoordinatorOrigins& operator=(CoordinatorOrigins&&) = default;

  bool contains(const url::Origin& origin) const {
    CHECK(IsValid());
    return base::Contains(origins_, origin);
  }

  const url::Origin& default_origin() const {
    CHECK(IsValid());
    return origins_.front();
  }

  const std::vector<url::Origin>& origins() const { return origins_; }

  [[nodiscard]] bool IsValid() const {
    if (origins_.empty()) {
      return false;
    }
    return base::ranges::all_of(origins_,
                                &attribution_reporting::IsOriginSuitable);
  }

 private:
  std::vector<url::Origin> origins_;
};

CoordinatorOrigins& GetCoordinatorOrigins() {
  static base::NoDestructor<CoordinatorOrigins> g_origins;

  if (!g_origins->origins().empty()) {
    return *g_origins;
  }

  *g_origins =
      CoordinatorOrigins(kAggregationServiceCoordinatorAllowlist.Get());

  return *g_origins;
}

}  // namespace

url::Origin GetDefaultAggregationCoordinatorOrigin() {
  return GetCoordinatorOrigins().default_origin();
}

bool IsAggregationCoordinatorOriginAllowed(const url::Origin& origin) {
  return GetCoordinatorOrigins().contains(origin);
}

ScopedAggregationCoordinatorAllowlistForTesting::
    ScopedAggregationCoordinatorAllowlistForTesting(
        std::vector<url::Origin> origins)
    : previous_(GetCoordinatorOrigins().origins()) {
  GetCoordinatorOrigins() = CoordinatorOrigins(std::move(origins));
}

ScopedAggregationCoordinatorAllowlistForTesting::
    ~ScopedAggregationCoordinatorAllowlistForTesting() {
  GetCoordinatorOrigins() = CoordinatorOrigins(std::move(previous_));
}

}  // namespace aggregation_service
