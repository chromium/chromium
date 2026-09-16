// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/task_policy_config.h"

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
    TaskPolicyConfig::Rule::Capability capability) {
  switch (capability) {
    case TaskPolicyConfig::Rule::Capability::kAll:
      return "CAPABILITY_ALL";
  }
}

std::string_view ResourceToString(TaskPolicyConfig::Rule::Resource resource) {
  switch (resource) {
    case TaskPolicyConfig::Rule::Resource::kSession:
      return "RESOURCE_SESSION";
  }
}

}  // namespace

TaskPolicyConfig::TaskPolicyConfig() = default;

TaskPolicyConfig::TaskPolicyConfig(const TaskPolicyConfig&) = default;

TaskPolicyConfig::TaskPolicyConfig(TaskPolicyConfig&&) = default;

TaskPolicyConfig::~TaskPolicyConfig() = default;

TaskPolicyConfig::TaskPolicyConfig(LocationRules location_rules)
    : location_rules_(std::move(location_rules)) {}

TaskPolicyConfig::Location::Location(Wildcard) : data_(Wildcard()) {}

TaskPolicyConfig::Location::Location(net::SchemefulSite site)
    : data_(std::move(site)) {}

TaskPolicyConfig::Location::Location(url::Origin origin)
    : data_(std::move(origin)) {}

TaskPolicyConfig::Location::Location(const Location&) = default;
TaskPolicyConfig::Location::Location(Location&&) = default;
TaskPolicyConfig::Location& TaskPolicyConfig::Location::operator=(
    const Location&) = default;
TaskPolicyConfig::Location& TaskPolicyConfig::Location::operator=(Location&&) =
    default;

TaskPolicyConfig::Location::~Location() = default;

bool TaskPolicyConfig::Location::Matches(const url::Origin& origin) const {
  return std::visit(absl::Overload([](const Wildcard&) { return true; },
                                   [&](const net::SchemefulSite& site) {
                                     return site.IsSameSiteWith(origin);
                                   },
                                   [&](const url::Origin& loc_origin) {
                                     return loc_origin.IsSameOriginWith(origin);
                                   }),
                    data_);
}

std::string TaskPolicyConfig::Location::ToDebugString() const {
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

TaskPolicyConfig::Rule::Rule() = default;

TaskPolicyConfig::Rule::Rule(const Rule&) = default;

TaskPolicyConfig::Rule::Rule(Rule&&) = default;

TaskPolicyConfig::Rule& TaskPolicyConfig::Rule::operator=(const Rule&) =
    default;

TaskPolicyConfig::Rule& TaskPolicyConfig::Rule::operator=(Rule&&) = default;

TaskPolicyConfig::Rule::Rule(std::vector<Location> navigation_sources,
                             ResourceSet resources,
                             CapabilitySet capabilities)
    : navigation_sources_(std::move(navigation_sources)),
      resources_(std::move(resources)),
      capabilities_(std::move(capabilities)) {}

TaskPolicyConfig::Rule::~Rule() = default;

bool TaskPolicyConfig::Rule::MatchesNavigationSource(
    const url::Origin& source_origin) const {
  return navigation_sources_.empty() ||
         std::ranges::any_of(navigation_sources_, [&](const auto& source) {
           return source.Matches(source_origin);
         });
}

bool TaskPolicyConfig::Rule::CanNavigate() const {
  return capabilities_.Has(Capability::kAll) &&
         resources_.Has(Resource::kSession);
}

base::Value TaskPolicyConfig::Rule::ToDebugValue() const {
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
    resources.Append(ResourceToString(resource));
  }

  return base::Value(base::DictValue()
                         .Set("navigation_sources", std::move(sources))
                         .Set("capabilities", std::move(capabilities))
                         .Set("accessible_resources", std::move(resources)));
}

bool TaskPolicyConfig::IsNavigationAllowed(
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

bool TaskPolicyConfig::IsActuationAllowed(
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

base::Value TaskPolicyConfig::ToDebugValue() const {
  base::DictValue rules;
  for (const auto& [location, rule] : location_rules_) {
    rules.Set(location.ToDebugString(), rule.ToDebugValue());
  }
  return base::Value(base::DictValue().Set("rules", std::move(rules)));
}

}  // namespace origin_gating
