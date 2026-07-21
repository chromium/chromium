// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_H_

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace base {
class Value;
}  // namespace base

namespace origin_gating {

// ActorContainerConfig manages client-side security boundaries for the
// actor codebase based on location rules.
//
// An empty set of location rules acts as a global blocklist: any attempt to
// navigate to or actuate on a location that has no matching location rule
// will be disallowed.
//
// Location rules may include a wildcard Location to allow the agent to visit
// any site (with a given set of resources and capabilities).
class ActorContainerConfig {
 public:
  class Location;
  class Rule;

  using LocationRules = base::flat_map<Location, Rule>;

  // Constructs an empty ActorContainerConfig (block-all configuration).
  ActorContainerConfig();
  ActorContainerConfig(const ActorContainerConfig&);
  ActorContainerConfig(ActorContainerConfig&&);
  ActorContainerConfig& operator=(const ActorContainerConfig& other) = delete;
  ActorContainerConfig& operator=(ActorContainerConfig&& other) = delete;
  ~ActorContainerConfig();

  // Constructs an ActorContainerConfig mapping locations to security rules.
  // An empty `location_rules` map acts as a global blocklist, disallowing all
  // navigations and actuations. A wildcard Location entry can be provided to
  // allow access to all sites with specified resources and capabilities.
  explicit ActorContainerConfig(LocationRules location_rules);

  // Indicates whether or not navigation from `source` to `destination` is
  // allowed according to this config.
  bool IsNavigationAllowed(const url::Origin& source,
                           const url::Origin& destination) const;

  // Indicates whether or not the actor can actuate when the browser is
  // navigated to `location_origin`.
  bool IsActuationAllowed(const url::Origin& location_origin) const;

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

    friend auto operator<=>(const Location&, const Location&) = default;
    friend bool operator==(const Location&, const Location&) = default;

   private:
    std::variant<Wildcard, net::SchemefulSite, url::Origin> data_;
  };

  // A rule governing how a web location may or may not be used by the agent.
  class Rule {
   public:
    // The set of resources that the agent may access when visiting or actuating
    // on a matching location. May be empty if no resources are accessible.
    enum class Resource {
      // The agent may use the user's existing session.
      kSession,
      kMin = kSession,
      kMax = kSession
    };

    // How the agent may interact with a matching location. An empty set of
    // capabilities means no capabilities are allowed and the agent is blocked
    // from interacting with the location.
    enum class Capability {
      // The agent may navigate to and interact with the page, as well as
      // execute tools on it.
      kAll,
      kMin = kAll,
      kMax = kAll
    };

    using ResourceSet = base::EnumSet<Resource, Resource::kMin, Resource::kMax>;
    using CapabilitySet =
        base::EnumSet<Capability, Capability::kMin, Capability::kMax>;

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
    // - `capabilities`: The set of capabilities granted by this rule.
    explicit Rule(std::vector<Location> navigation_sources,
                  ResourceSet resources,
                  CapabilitySet capabilities);
    ~Rule();

    bool MatchesNavigationSource(const url::Origin& source_origin) const;
    bool CanNavigate() const;

    // Serializes `this` as a value for debugging.
    base::Value ToDebugValue() const;

    friend bool operator==(const Rule&, const Rule&) = default;

   private:
    std::vector<Location> navigation_sources_;
    ResourceSet resources_;
    CapabilitySet capabilities_;
  };

 private:
  LocationRules location_rules_;

  friend bool operator==(const ActorContainerConfig&,
                         const ActorContainerConfig&) = default;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_H_
