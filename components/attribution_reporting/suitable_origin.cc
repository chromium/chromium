// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/suitable_origin.h"

#include <utility>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace attribution_reporting {

// static
bool SuitableOrigin::IsSuitable(const url::Origin& origin) {
  return network::IsOriginPotentiallyTrustworthy(origin);
}

// static
absl::optional<SuitableOrigin> SuitableOrigin::Create(url::Origin origin) {
  if (!IsSuitable(origin))
    return absl::nullopt;

  return SuitableOrigin(std::move(origin));
}

// static
absl::optional<SuitableOrigin> SuitableOrigin::Deserialize(
    base::StringPiece str) {
  return Create(url::Origin::Create(GURL(str)));
}

SuitableOrigin::SuitableOrigin(url::Origin origin)
    : origin_(std::move(origin)) {
  DCHECK(IsValid());
}

SuitableOrigin::~SuitableOrigin() = default;

SuitableOrigin::SuitableOrigin(const SuitableOrigin&) = default;

SuitableOrigin& SuitableOrigin::operator=(const SuitableOrigin&) = default;

SuitableOrigin::SuitableOrigin(SuitableOrigin&&) = default;

SuitableOrigin& SuitableOrigin::operator=(SuitableOrigin&&) = default;

bool SuitableOrigin::IsValid() const {
  return IsSuitable(origin_);
}

}  // namespace attribution_reporting
