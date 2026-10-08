// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/task_policy_config.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_gating {

namespace {

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

TaskPolicyConfig::TaskPolicyConfig(LocationRules location_rules,
                                   ClientTool navigate_tool)
    : location_rules_(std::move(location_rules)),
      navigate_tool_(navigate_tool) {
  std::vector<ClientTool> all_tools = {navigate_tool};
  for (const auto& [location, rule] : location_rules_) {
    std::ranges::copy(rule.allowed_tools(base::PassKey<TaskPolicyConfig>()),
                      std::back_inserter(all_tools));
  }
  CHECK(std::ranges::all_of(all_tools, [&](const ClientTool& tool) {
    return tool.IsSameDomain(navigate_tool);
  })) << "All tools in a TaskPolicyConfig must share a ToolDomain";
}

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
                             absl::flat_hash_set<ClientTool> allowed_tools)
    : navigation_sources_(std::move(navigation_sources)),
      resources_(std::move(resources)),
      allowed_tools_(std::move(allowed_tools)) {}

TaskPolicyConfig::Rule::~Rule() = default;

bool TaskPolicyConfig::Rule::MatchesNavigationSource(
    const url::Origin& source_origin) const {
  return navigation_sources_.empty() ||
         std::ranges::any_of(navigation_sources_, [&](const auto& source) {
           return source.Matches(source_origin);
         });
}

bool TaskPolicyConfig::Rule::CanActuate(const ClientTool& tool) const {
  return allowed_tools_.contains(tool) && resources_.Has(Resource::kSession);
}

base::Value TaskPolicyConfig::Rule::ToDebugValue() const {
  base::ListValue sources;
  for (const auto& source : navigation_sources_) {
    sources.Append(source.ToDebugString());
  }

  // Sort for a stable order, since `allowed_tools_` is unordered.
  std::vector<base::Value> tool_values =
      base::ToVector(allowed_tools_, &ClientTool::ToDebugValue);
  std::ranges::sort(tool_values);
  base::ListValue allowed_tools;
  for (auto& tool_value : tool_values) {
    allowed_tools.Append(std::move(tool_value));
  }

  base::ListValue resources;
  for (auto resource : resources_) {
    resources.Append(ResourceToString(resource));
  }

  return base::Value(base::DictValue()
                         .Set("navigation_sources", std::move(sources))
                         .Set("allowed_tools", std::move(allowed_tools))
                         .Set("accessible_resources", std::move(resources)));
}

bool TaskPolicyConfig::IsNavigationAllowed(
    const url::Origin& source,
    const url::Origin& destination) const {
  if (!navigate_tool_.has_value()) {
    return false;
  }
  if (const auto* rule =
          base::FindOrNull(location_rules_, Location(destination));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanActuate(*navigate_tool_);
  }
  if (const auto* rule = base::FindOrNull(
          location_rules_, Location(net::SchemefulSite(destination)));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanActuate(*navigate_tool_);
  }
  if (const auto* rule =
          base::FindOrNull(location_rules_, Location(Wildcard()));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanActuate(*navigate_tool_);
  }
  return false;
}

bool TaskPolicyConfig::IsActuationAllowed(const url::Origin& location_origin,
                                          const ClientTool& tool) const {
  if (const auto* rule =
          base::FindOrNull(location_rules_, Location(location_origin))) {
    return rule->CanActuate(tool);
  }
  if (const auto* rule = base::FindOrNull(
          location_rules_, Location(net::SchemefulSite(location_origin)))) {
    return rule->CanActuate(tool);
  }
  if (const auto* rule =
          base::FindOrNull(location_rules_, Location(Wildcard()))) {
    return rule->CanActuate(tool);
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
