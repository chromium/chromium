// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_

#include <string>
#include <utility>

#include "base/check.h"
#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

class GURL;

namespace mojo {
struct DefaultConstructTraits;
}  // namespace mojo

namespace attribution_reporting {

// A thin wrapper around `url::Origin` that enforces invariants required for an
// origin to be used as a source origin, a destination origin, or a reporting
// origin.
//
// These origins must be potentially trustworthy, as determined by
// `network::IsOriginPotentiallyTrustworthy()`, and their scheme must be HTTP or
// HTTPS.
//
// It is an error to use instances of this type after moving.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) SuitableOrigin {
 public:
  static bool IsSuitable(const url::Origin&);

  static absl::optional<SuitableOrigin> Create(url::Origin);

  static absl::optional<SuitableOrigin> Create(const GURL&);

  // Creates a `SuitableOrigin` from the given string, which is first converted
  // to a `GURL`, then to a `url::Origin`, and then subject to this class's
  // invariants.
  //
  // All parts of the URL other than the origin are ignored.
  static absl::optional<SuitableOrigin> Deserialize(base::StringPiece);

  ~SuitableOrigin();

  SuitableOrigin(const SuitableOrigin&);
  SuitableOrigin& operator=(const SuitableOrigin&);

  SuitableOrigin(SuitableOrigin&&);
  SuitableOrigin& operator=(SuitableOrigin&&);

  const url::Origin& operator*() const& {
    DCHECK(IsValid());
    return origin_;
  }

  url::Origin&& operator*() && {
    DCHECK(IsValid());
    return std::move(origin_);
  }

  const url::Origin* operator->() const& {
    DCHECK(IsValid());
    return &origin_;
  }

  // This implicit "widening" conversion is allowed to ease drop-in use of
  // this type in places currently requiring `url::Origin`s with
  // guaranteed preconditions.
  operator const url::Origin&() const {  // NOLINT
    DCHECK(IsValid());
    return origin_;
  }

  // Allows this type to be used as a key in a set or map.
  bool operator<(const SuitableOrigin&) const;

  std::string Serialize() const;

  bool IsValid() const;

 private:
  friend struct SourceRegistration;
  friend struct TriggerRegistration;
  friend mojo::DefaultConstructTraits;

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  SuitableOrigin();

  explicit SuitableOrigin(url::Origin);

  url::Origin origin_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_
