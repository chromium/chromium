// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/actor_container_config.h"

#include <string>
#include <string_view>
#include <variant>

#include "base/check.h"
#include "base/containers/map_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace origin_gating {

namespace {

base::expected<std::string_view, std::string_view> ConvertProtocol(
    optimization_guide::proto::Protocol protocol) {
  switch (protocol) {
    case optimization_guide::proto::Protocol::PROTOCOL_HTTPS:
      return base::ok(url::kHttpsScheme);
    case optimization_guide::proto::Protocol::PROTOCOL_HTTP:
      return base::ok(url::kHttpScheme);
    case optimization_guide::proto::Protocol::PROTOCOL_WSS:
      return base::ok(url::kWssScheme);
    case optimization_guide::proto::Protocol::PROTOCOL_WS:
      return base::ok(url::kWsScheme);
    case optimization_guide::proto::Protocol::PROTOCOL_UNKNOWN:
    default:
      return base::unexpected("Unknown protocol");
  }
}

base::expected<net::SchemefulSite, std::string_view> ConvertSite(
    const optimization_guide::proto::Site& site) {
  if (!site.has_domain()) {
    return base::unexpected("Site message is missing domain");
  }
  ASSIGN_OR_RETURN(std::string_view protocol, ConvertProtocol(site.protocol()));
  net::SchemefulSite converted_site(GURL(
      base::StrCat({protocol, url::kStandardSchemeSeparator, site.domain()})));
  if (converted_site.GetURL().host() != site.domain()) {
    return base::unexpected("SchemefulSite domain does not match the message");
  }
  return converted_site;
}

base::expected<url::Origin, std::string_view> ConvertOrigin(
    const optimization_guide::proto::Origin& origin) {
  if (!origin.has_host()) {
    return base::unexpected("Origin message is missing host");
  }
  ASSIGN_OR_RETURN(std::string_view protocol,
                   ConvertProtocol(origin.protocol()));
  std::string port =
      origin.has_port()
          ? base::StrCat({":", base::NumberToString(origin.port())})
          : "";
  return url::Origin::Create(GURL(base::StrCat(
      {protocol, url::kStandardSchemeSeparator, origin.host(), port})));
}

std::string ActuationCapabilityToString(
    optimization_guide::proto::RuleMetadata::ActuationCapability capability) {
  switch (capability) {
    case optimization_guide::proto::RuleMetadata::CAPABILITY_UNKNOWN:
      return "CAPABILITY_UNKNOWN";
    case optimization_guide::proto::RuleMetadata::CAPABILITY_ALL:
      return "CAPABILITY_ALL";
    default:
      return base::NumberToString(static_cast<int>(capability));
  }
}

std::string AgentResourceToString(
    optimization_guide::proto::RuleMetadata::AgentResource resource) {
  switch (resource) {
    case optimization_guide::proto::RuleMetadata::RESOURCE_UNKNOWN:
      return "RESOURCE_UNKNOWN";
    case optimization_guide::proto::RuleMetadata::RESOURCE_SESSION:
      return "RESOURCE_SESSION";
    default:
      return base::NumberToString(static_cast<int>(resource));
  }
}

}  // namespace

ActorContainerConfig::ActorContainerConfig(const ActorContainerConfig&) =
    default;

ActorContainerConfig::ActorContainerConfig(ActorContainerConfig&&) = default;

ActorContainerConfig::~ActorContainerConfig() = default;

ActorContainerConfig::ActorContainerConfig(
    const optimization_guide::proto::AgentContainerConfig& config) {
  for (const auto& rule_proto : config.location_rules()) {
    base::expected<Location, std::string_view> destination_result =
        Location::Create(rule_proto.location());
    if (!destination_result.has_value()) {
      DLOG(ERROR) << destination_result.error();
      continue;
    }
    base::expected<Rule, std::string_view> rule = Rule::Create(rule_proto);
    if (!rule.has_value()) {
      DLOG(ERROR) << rule.error();
      continue;
    }
    const Location& destination = destination_result.value();
    if (auto it = rules_.find(destination); it != rules_.end()) {
      DLOG(ERROR) << "Duplicate rule for " << destination.ToDebugString();
    }
    rules_.insert_or_assign(destination, rule.value());
  }
}

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

base::expected<ActorContainerConfig::Location, std::string_view>
ActorContainerConfig::Location::Create(
    const optimization_guide::proto::Location& location) {
  switch (location.identifier_oneof_case()) {
    case optimization_guide::proto::Location::kWildcard:
      return Location(Wildcard());
    case optimization_guide::proto::Location::kSite: {
      ASSIGN_OR_RETURN(net::SchemefulSite site, ConvertSite(location.site()));
      return Location(std::move(site));
    }
    case optimization_guide::proto::Location::kOrigin: {
      ASSIGN_OR_RETURN(url::Origin origin, ConvertOrigin(location.origin()));
      return Location(std::move(origin));
    }
    case optimization_guide::proto::Location::IDENTIFIER_ONEOF_NOT_SET:
      return base::unexpected("Location missing value");
    default:
      return base::unexpected("Unknown location type");
  }
}

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

ActorContainerConfig::Rule::Rule(
    std::vector<Location> navigation_sources,
    optimization_guide::proto::RuleMetadata metadata)
    : navigation_sources_(std::move(navigation_sources)),
      metadata_(std::move(metadata)) {}

ActorContainerConfig::Rule::~Rule() = default;

base::expected<ActorContainerConfig::Rule, std::string_view>
ActorContainerConfig::Rule::Create(
    const optimization_guide::proto::LocationRule& location_rule) {
  std::vector<Location> navigation_sources;
  for (const auto& nav_source : location_rule.navigation_sources()) {
    if (!nav_source.has_source()) {
      return base::unexpected("NavigationSource has no source location set");
    }
    ASSIGN_OR_RETURN(Location source, Location::Create(nav_source.source()));
    navigation_sources.emplace_back(source);
  }
  return Rule(navigation_sources, location_rule.metadata());
}

bool ActorContainerConfig::Rule::MatchesNavigationSource(
    const url::Origin& source_origin) const {
  return navigation_sources_.empty() ||
         std::ranges::any_of(navigation_sources_, [&](const auto& source) {
           return source.Matches(source_origin);
         });
}

bool ActorContainerConfig::Rule::CanNavigate() const {
  return std::ranges::contains(
             metadata_.capabilities(),
             optimization_guide::proto::RuleMetadata::CAPABILITY_ALL) &&
         std::ranges::contains(
             metadata_.accessible_resources(),
             optimization_guide::proto::RuleMetadata::RESOURCE_SESSION);
}

base::Value ActorContainerConfig::Rule::ToDebugValue() const {
  base::ListValue sources;
  for (const auto& source : navigation_sources_) {
    sources.Append(source.ToDebugString());
  }

  base::ListValue capabilities;
  for (const auto& capability : metadata_.capabilities()) {
    capabilities.Append(ActuationCapabilityToString(
        static_cast<
            optimization_guide::proto::RuleMetadata::ActuationCapability>(
            capability)));
  }

  base::ListValue resources;
  for (const auto& resource : metadata_.accessible_resources()) {
    resources.Append(AgentResourceToString(
        static_cast<optimization_guide::proto::RuleMetadata::AgentResource>(
            resource)));
  }

  return base::Value(base::DictValue()
                         .Set("navigation_sources", std::move(sources))
                         .Set("capabilities", std::move(capabilities))
                         .Set("accessible_resources", std::move(resources)));
}

bool ActorContainerConfig::IsNavigationAllowed(
    const url::Origin& source,
    const url::Origin& destination) const {
  if (const auto* rule = base::FindOrNull(rules_, Location(destination));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  if (const auto* rule =
          base::FindOrNull(rules_, Location(net::SchemefulSite(destination)));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(rules_, Location(Wildcard()));
      rule && rule->MatchesNavigationSource(source)) {
    return rule->CanNavigate();
  }
  return false;
}

bool ActorContainerConfig::IsActuationAllowed(
    const url::Origin& location_origin) const {
  if (const auto* rule = base::FindOrNull(rules_, Location(location_origin))) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(
          rules_, Location(net::SchemefulSite(location_origin)))) {
    return rule->CanNavigate();
  }
  if (const auto* rule = base::FindOrNull(rules_, Location(Wildcard()))) {
    return rule->CanNavigate();
  }
  return false;
}

base::Value ActorContainerConfig::ToDebugValue() const {
  base::DictValue rules;
  for (const auto& [location, rule] : rules_) {
    rules.Set(location.ToDebugString(), rule.ToDebugValue());
  }
  return base::Value(base::DictValue().Set("rules", std::move(rules)));
}

}  // namespace origin_gating
