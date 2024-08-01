// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_

#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/component_export.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "url/origin.h"

class GURL;

namespace net {
class SchemefulSite;
}  // namespace net

namespace attribution_reporting {

// Returns true if the given site is potentially suitable as a destination site,
// that is, it `net::SchemefulSite::has_registrable_domain_or_host()` and its
// scheme is HTTP or HTTPS.
//
// Other requirements may be enforced in the future.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
bool IsSitePotentiallySuitable(const net::SchemefulSite&);

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

  static std::optional<SuitableOrigin> Create(url::Origin);

  static std::optional<SuitableOrigin> Create(const GURL&);

  // Creates a `SuitableOrigin` from the given string, which is first converted
  // to a `GURL`, then to a `url::Origin`, and then subject to this class's
  // invariants.
  //
  // All parts of the URL other than the origin are ignored.
  static std::optional<SuitableOrigin> Deserialize(std::string_view);

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  explicit SuitableOrigin(mojo::DefaultConstruct::Tag);

  ~SuitableOrigin();

  SuitableOrigin(const SuitableOrigin&);
  SuitableOrigin& operator=(const SuitableOrigin&);

  SuitableOrigin(SuitableOrigin&&);
  SuitableOrigin& operator=(SuitableOrigin&&);

  const url::Origin& operator*() const& {
    CHECK(IsValid());
    return origin_;
  }

  url::Origin&& operator*() && {
    CHECK(IsValid());
    return std::move(origin_);
  }

  const url::Origin* operator->() const& {
    CHECK(IsValid());
    return &origin_;
  }

  // This implicit "widening" conversion is allowed to ease drop-in use of
  // this type in places currently requiring `url::Origin`s with
  // guaranteed preconditions.
  operator const url::Origin&() const {  // NOLINT
    CHECK(IsValid());
    return origin_;
  }

  // Allows this type to be used as a key in a set or map.
  friend std::weak_ordering operator<=>(const SuitableOrigin&,
                                        const SuitableOrigin&) = default;

  std::string Serialize() const;

  bool IsValid() const;

 private:
  explicit SuitableOrigin(url::Origin);

  url::Origin origin_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_
