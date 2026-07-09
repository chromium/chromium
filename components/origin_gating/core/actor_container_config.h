// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_H_
#define COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_H_

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace origin_gating {

// ActorContainerConfig manages client-side security boundaries for the
// actor codebase based on an AgentContainerConfig proto.
class ActorContainerConfig {
 public:
  ActorContainerConfig() = delete;
  ActorContainerConfig(const ActorContainerConfig&);
  ActorContainerConfig(ActorContainerConfig&&);
  ActorContainerConfig& operator=(const ActorContainerConfig& other) = delete;
  ActorContainerConfig& operator=(ActorContainerConfig&& other) = delete;
  ~ActorContainerConfig();

  explicit ActorContainerConfig(
      const optimization_guide::proto::AgentContainerConfig& config);

  // Indicates whether or not navigation from `source` to `destination` is
  // allowed according to this config.
  bool IsNavigationAllowed(const url::Origin& source,
                           const url::Origin& destination) const;

  // Indicates whether or not the actor can actuate when the browser is
  // navigated to `location_origin`.
  bool IsActuationAllowed(const url::Origin& location_origin) const;

 private:
  // Represents a wildcard location, i.e. matches every origin/site.
  using Wildcard = std::monostate;

  // Represents a location pattern, for looking up `Rule`s by origin/site/etc.
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

    // Factory to create an instance from a `Location` proto.
    static base::expected<Location, std::string_view> Create(
        const optimization_guide::proto::Location& location);

    // Returns true if `this` matches the given origin.
    bool Matches(const url::Origin& origin) const;

    // Serializes `this` as a string for debugging.
    std::string ToDebugString() const;

    friend auto operator<=>(const Location&, const Location&) = default;

   private:
    std::variant<Wildcard, net::SchemefulSite, url::Origin> data_;
  };

  class Rule {
   public:
    Rule();
    Rule(const Rule&);
    Rule(Rule&&);
    Rule& operator=(const Rule&);
    Rule& operator=(Rule&&);
    explicit Rule(std::vector<Location> navigation_sources,
                  optimization_guide::proto::RuleMetadata metadata);
    ~Rule();

    static base::expected<Rule, std::string_view> Create(
        const optimization_guide::proto::LocationRule& location_rule);

    bool MatchesNavigationSource(const url::Origin& source_origin) const;
    bool CanNavigate() const;

   private:
    std::vector<Location> navigation_sources_;
    optimization_guide::proto::RuleMetadata metadata_;
  };

  base::flat_map<Location, Rule> rules_;
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_ACTOR_CONTAINER_CONFIG_H_
