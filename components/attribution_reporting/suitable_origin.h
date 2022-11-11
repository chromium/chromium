// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_

#include <utility>

#include "base/check.h"
#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace attribution_reporting {

// A thin wrapper around `url::Origin` that enforces invariants required for an
// origin to be used as a source origin, a destination origin, or a reporting
// origin.
//
// These origins must be potentially trustworthy, as determined by
// `network::IsOriginPotentiallyTrustworthy()`.
//
// In the future, this type will also enforce the use of HTTP or HTTPS as the
// origin's scheme (crbug.com/1351086).
//
// It is an error to use instances of this type after moving.
class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) SuitableOrigin {
 public:
  static absl::optional<SuitableOrigin> Create(url::Origin);

  // Creates a `SuitableOrigin` from the given string, which is first converted
  // to a `GURL`, then to a `url::Origin`, and then subject to this class's
  // invariants.
  //
  // All parts of the URL other than the origin are ignored.
  static absl::optional<SuitableOrigin> Deserialize(base::StringPiece);

  SuitableOrigin() = delete;

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

 private:
  explicit SuitableOrigin(url::Origin);

  bool IsValid() const;

  url::Origin origin_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_SUITABLE_ORIGIN_H_
