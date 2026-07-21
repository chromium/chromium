// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/actor_container_config.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_gating {

namespace {

std::string_view ActuationCapabilityToString(
    ActorContainerConfig::Rule::Capability capability) {
  switch (capability) {
    case ActorContainerConfig::Rule::Capability::kAll:
      return "CAPABILITY_ALL";
  }
}

std::string_view AgentResourceToString(
    ActorContainerConfig::Rule::Resource resource) {
  switch (resource) {
    case ActorContainerConfig::Rule::Resource::kSession:
      return "RESOURCE_SESSION";
  }
}

}  // namespace

ActorContainerConfig::ActorContainerConfig() = default;

ActorContainerConfig::ActorContainerConfig(const ActorContainerConfig&) =
    default;

ActorContainerConfig::ActorContainerConfig(ActorContainerConfig&&) = default;

ActorContainerConfig::~ActorContainerConfig() = default;

ActorContainerConfig::ActorContainerConfig(LocationRules location_rules)
    : location_rules_(std::move(location_rules)) {}

ActorContainerConfig::Location::Location(Wildcard) : data_(Wildcard()) {}

ActorContainerConfig::Location::Location(net::SchemefulSite site)
    : data_(std::move(site)) {}

ActorContainerConfig::Location::Location(url::Origin origin)
    : data_(std::move(origin)) {}

ActorContainerConfig::Location::Location(const Location&) = default;
ActorContainerConfig::Location::Location(Location&&) = default;
ActorContainerConfig::Location& ActorContainerConfig::Location::operator=(
    const Location&) = default;
ActorContainerConfig::Location& ActorContainerConfig::Location::operator=(
    Location&&) = default;

ActorContainerConfig::Location::~Location() = default;

bool ActorContainerConfig::Location::Matches(const url::Origin& origin) const {
  return std::visit(absl::Overload([](const Wildcard&) { return true; },
                                   [&](const net::SchemefulSite& site) {
                                     return site.IsSameSiteWith(origin);
                                   },
                                   [&](const url::Origin& loc_origin) {
                                     return loc_origin.IsSameOriginWith(origin);
                                   }),
                    data_);
}

std::string ActorContainerConfig::Location::ToDebugString() const {
  return std::visit(
      absl::Overload(
          [](const Wildcard&) -> std::string { return "Wildcard"; },
          [](const net::SchemefulSite& site) {
            return base::StrCat({"Site(", site.GetDebugString(), ")"});
          },
          [](const url::Origin& origin) {
            return base::StrCat({"Origin(", origin.GetDebugString(), ")"});
          }),
      data_);
}

ActorContainerConfig::Rule::Rule() = default;

ActorContainerConfig::Rule::Rule(const Rule&) = default;

ActorContainerConfig::Rule::Rule(Rule&&) = default;

ActorContainerConfig::Rule& ActorContainerConfig::Rule::operator=(const Rule&) =
    default;

ActorContainerConfig::Rule& ActorContainerConfig::Rule::operator=(Rule&&) =
    default;

ActorContainerConfig::Rule::Rule(std::vector<Location> navigation_sources,
                                 ResourceSet resources,
                                 CapabilitySet capabilities)
    : navigation_sources_(std::move(navigation_sources)),
      resources_(std::move(resources)),
      capabilities_(std::move(capabilities)) {}

ActorContainerConfig::Rule::~Rule() = default;

bool ActorContainerConfig::Rule::MatchesNavigationSource(
    const url::Origin& source_origin) const {
  return navigation_sources_.empty() ||
         std::ranges::any_of(navigation_sources_, [&](const auto& source) {
           return source.Matches(source_origin);
         });
}

bool ActorContainerConfig::Rule::CanNavigate() const {
  return capabilities_.Has(Capability::kAll) &&
         resources_.Has(Resource::kSession);
}

base::Value ActorContainerConfig::Rule::ToDebugValue() const {
  base::ListValue sources;
  for (const auto& source : navigation_sources_) {
    sources.Append(source.ToDebugString());
  }

  base::ListValue capabilities;
  for (auto capability : capabilities_) {
    capabilities.Append(ActuationCapabilityToString(capability));
  }

  base::ListValue resources;
  for (auto resource : resources_) {
    resources.Append(AgentResourceToString(resource));
  }

  return base::Value(base::DictValue()
                         .Set("navigation_sources", std::move(sources))
                         .Set("capabilities", std::move(capabilities))
                         .Set("accessible_resources", std::move(resources)));
}

bool ActorContainerConfig::IsNavigationAllowed(
    const url::Origin& source,
    const url::Origin& destination) const {
  if (const auto* rule =
          base::FindOrNull(location_rules_, Location(destination));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(
          location_rules_, Location(net::SchemefulSite(destination)));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  if (const auto* rule =
          base::FindOrNull(location_rules_, Location(Wildcard()));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  return false;
}

bool ActorContainerConfig::IsActuationAllowed(
    const url::Origin& location_origin) const {
  if (const auto* rule =
          base::FindOrNull(location_rules_, Location(location_origin))) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(
          location_rules_, Location(net::SchemefulSite(location_origin)))) {
    return rule->CanNavigate();
  }
  if (const auto* rule =
          base::FindOrNull(location_rules_, Location(Wildcard()))) {
    return rule->CanNavigate();
  }
  return false;
}

base::Value ActorContainerConfig::ToDebugValue() const {
  base::DictValue rules;
  for (const auto& [location, rule] : location_rules_) {
    rules.Set(location.ToDebugString(), rule.ToDebugValue());
  }
  return base::Value(base::DictValue().Set("rules", std::move(rules)));
}

}  // namespace origin_gating
