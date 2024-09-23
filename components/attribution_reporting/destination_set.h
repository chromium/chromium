// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_DESTINATION_SET_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_DESTINATION_SET_H_

#include <optional>

#include "base/check.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"

namespace base {
class Value;
}  // namespace base

namespace net {
class SchemefulSite;
}  // namespace net

namespace attribution_reporting {

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) DestinationSet {
 public:
  using Destinations = base::flat_set<net::SchemefulSite>;

  static std::optional<DestinationSet> Create(Destinations);

  static base::expected<DestinationSet, mojom::SourceRegistrationError>
  FromJSON(const base::Value*);

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  explicit DestinationSet(mojo::DefaultConstruct::Tag);

  ~DestinationSet();

  DestinationSet(const DestinationSet&);
  DestinationSet(DestinationSet&&);

  DestinationSet& operator=(const DestinationSet&);
  DestinationSet& operator=(DestinationSet&&);

  const Destinations& destinations() const {
    CHECK(IsValid());
    return destinations_;
  }

  bool IsValid() const;

  base::Value ToJson() const;

  friend bool operator==(const DestinationSet&,
                         const DestinationSet&) = default;

 private:
  explicit DestinationSet(Destinations);

  Destinations destinations_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_DESTINATION_SET_H_
