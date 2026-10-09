// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_TASK_POLICY_CONFIG_H_
#define COMPONENTS_ORIGIN_GATING_CORE_TASK_POLICY_CONFIG_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/types/pass_key.h"
#include "components/origin_gating/core/client_tool.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

namespace base {
class Value;
}  // namespace base

namespace origin_gating {

// TaskPolicyConfig manages client-side security boundaries based on location
// rules.
//
// An empty set of location rules acts as a global blocklist: any attempt to
// navigate to or actuate on a location that has no matching location rule
// will be disallowed.
//
// Location rules may include a wildcard Location to allow visiting any site
// (with a given set of resources and allowed tools).
class TaskPolicyConfig {
 public:
  class Location;
  class Rule;

  using LocationRules = base::flat_map<Location, Rule>;

  // Constructs an empty TaskPolicyConfig (block-all configuration).
  TaskPolicyConfig();
  TaskPolicyConfig(const TaskPolicyConfig&);
  TaskPolicyConfig(TaskPolicyConfig&&);
  TaskPolicyConfig& operator=(const TaskPolicyConfig& other) = delete;
  TaskPolicyConfig& operator=(TaskPolicyConfig&& other) = delete;
  ~TaskPolicyConfig();

  // Constructs a TaskPolicyConfig mapping locations to security rules.
  // An empty `location_rules` map acts as a global blocklist, disallowing all
  // navigations and actuations. A wildcard Location entry can be provided to
  // allow access to all sites with specified resources and allowed tools.
  // Navigation to a location is allowed only if its rule allows
  // `navigate_tool`. All allowed tools across all rules, and `navigate_tool`,
  // must share the same `ToolDomain`.
  TaskPolicyConfig(LocationRules location_rules, ClientTool navigate_tool);

  // Indicates whether or not navigation from `source` to `destination` is
  // allowed according to this config.
  bool IsNavigationAllowed(const url::Origin& source,
                           const url::Origin& destination) const;

  // Indicates whether or not the actor can execute `tool` when the browser is
  // navigated to `location_origin`.
  bool IsActuationAllowed(const url::Origin& location_origin,
                          const ClientTool& tool) const;

  // Serializes `this` as a Value for debugging. Note: the precise format of
  // this object is not guaranteed to be stable.
  base::Value ToDebugValue() const;

  using Wildcard = std::monostate;

  class Location {
   public:
    Location() = delete;
    explicit Location(Wildcard);
    explicit Location(net::SchemefulSite site);
    explicit Location(url::Origin origin);
    Location(const Location&);
    Location(Location&&);
    Location& operator=(const Location&);
    Location& operator=(Location&&);
    ~Location();

    // Returns true if `this` matches the given origin.
    bool Matches(const url::Origin& origin) const;

    // Serializes `this` as a string for debugging.
    std::string ToDebugString() const;

    template <typename H>
    friend H AbslHashValue(H h, const Location& location) {
      return H::combine(std::move(h), location.data_);
    }

    friend auto operator<=>(const Location&, const Location&) = default;
    friend bool operator==(const Location&, const Location&) = default;

   private:
    std::variant<Wildcard, net::SchemefulSite, url::Origin> data_;
  };

  // A rule governing how a web location may or may not be used.
  class Rule {
   public:
    // The set of resources that may be accessed when visiting or actuating on a
    // matching location. May be empty if no resources are accessible.
    enum class Resource {
      // The user's existing session may be used.
      kSession,
      kMin = kSession,
      kMax = kSession
    };

    using ResourceSet = base::EnumSet<Resource, Resource::kMin, Resource::kMax>;

    Rule();
    Rule(const Rule&);
    Rule(Rule&&);
    Rule& operator=(const Rule&);
    Rule& operator=(Rule&&);
    // Constructs a security rule for a location.
    // - `navigation_sources`: Navigations to the associated location must have
    //   one of these source locations to match this rule. If empty, all
    //   navigations to the location match this rule regardless of source.
    // - `resources`: The set of accessible resources granted by this rule.
    // - `allowed_tools`: The set of tools allowed on this location. An empty
    //   set means no tools are allowed and interaction/navigation is blocked.
    explicit Rule(absl::flat_hash_set<Location> navigation_sources,
                  ResourceSet resources,
                  absl::flat_hash_set<ClientTool> allowed_tools);
    ~Rule();

    bool MatchesNavigationSource(const url::Origin& source_origin) const;
    bool CanActuate(const ClientTool& tool) const;

    // Exposed so `TaskPolicyConfig` can check that all tools share a
    // `ToolDomain`.
    const absl::flat_hash_set<ClientTool>& allowed_tools(
        base::PassKey<TaskPolicyConfig>) const {
      return allowed_tools_;
    }

    // Serializes `this` as a value for debugging.
    base::Value ToDebugValue() const;

    friend bool operator==(const Rule&, const Rule&) = default;

   private:
    absl::flat_hash_set<Location> navigation_sources_;
    ResourceSet resources_;
    absl::flat_hash_set<ClientTool> allowed_tools_;
  };

 private:
  LocationRules location_rules_;
  std::optional<ClientTool> navigate_tool_;

  friend bool operator==(const TaskPolicyConfig&,
                         const TaskPolicyConfig&) = default;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_TASK_POLICY_CONFIG_H_
