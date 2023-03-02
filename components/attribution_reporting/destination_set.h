// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_DESTINATION_SET_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_DESTINATION_SET_H_

#include "base/check.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
struct SourceRegistration;
}  // namespace attribution_reporting

namespace base {
class Value;
}  // namespace base

namespace mojo {
struct DefaultConstructTraits;
}  // namespace mojo

namespace net {
class SchemefulSite;
}  // namespace net

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) DestinationSet {
 public:
  using Destinations = base::flat_set<net::SchemefulSite>;

  static absl::optional<DestinationSet> Create(Destinations);

  static base::expected<DestinationSet, mojom::SourceRegistrationError>
  FromJSON(const base::Value*);

  ~DestinationSet();

  DestinationSet(const DestinationSet&);
  DestinationSet(DestinationSet&&);

  DestinationSet& operator=(const DestinationSet&);
  DestinationSet& operator=(DestinationSet&&);

  const Destinations& destinations() const {
    DCHECK(IsValid());
    return destinations_;
  }

  bool IsValid() const;

  base::Value ToJson() const;

 private:
  friend attribution_reporting::SourceRegistration;
  friend mojo::DefaultConstructTraits;

  DestinationSet();
  explicit DestinationSet(Destinations);

  Destinations destinations_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_DESTINATION_SET_H_
