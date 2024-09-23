// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DLP_RULES_MANAGER_BASE_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DLP_RULES_MANAGER_BASE_H_

#include <map>
#include <set>
#include <string>

#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace data_controls {

// DlpRulesManagerBase is the generic interface to parse the rules set in the
// DataLeakPreventionRulesList policy and serves as an available service which
// can be queried anytime about the restrictions set by the policy.
class DlpRulesManagerBase : public KeyedService {
 public:
  using Restriction = data_controls::Rule::Restriction;
  using Level = data_controls::Rule::Level;

  // Represents rule metadata that is used for reporting.
  struct RuleMetadata {
    RuleMetadata(const std::string& name, const std::string& obfuscated_id)
        : name(name), obfuscated_id(obfuscated_id) {}
    RuleMetadata(const RuleMetadata&) = default;
    RuleMetadata() = default;
    RuleMetadata& operator=(const RuleMetadata&) = default;
    ~RuleMetadata() = default;

    std::string name;
    std::string obfuscated_id;
  };

  // Mapping from a level to the set of destination URLs for which that level is
  // enforced.
  using AggregatedDestinations = std::map<Level, std::set<std::string>>;

  ~DlpRulesManagerBase() override = default;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source`. ALLOW is returned if there is no matching rule. Requires
  // `restriction` to be one of the following: screenshot, printing,
  // privacy screen, screenshare.
  virtual Level IsRestricted(const GURL& source,
                             Restriction restriction) const = 0;

  // Returns the highest possible restriction enforcement level for
  // 'restriction' given that data comes from 'source' and the destination might
  // be any. ALLOW level rules are ignored.
  // If there's a rule matching, `out_source_pattern` will be changed to any
  // random matching rule URL pattern  and `out_rule_metadata` will be changed
  // to the matched rule metadata.
  virtual Level IsRestrictedByAnyRule(
      const GURL& source,
      Restriction restriction,
      std::string* out_source_pattern,
      RuleMetadata* out_rule_metadata) const = 0;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if there is no matching rule. Requires `restriction` to be
  // clipboard or files.
  // If there's a rule matching, `out_source_pattern` and
  // `out_destination_pattern` will be changed to the original rule URL
  // patterns  and `out_rule_metadata` will be changed to the matched rule
  // metadata.
  virtual Level IsRestrictedDestination(
      const GURL& source,
      const GURL& destination,
      Restriction restriction,
      std::string* out_source_pattern,
      std::string* out_destination_pattern,
      RuleMetadata* out_rule_metadata) const = 0;

  // Returns a mapping from the level to a set of destination URLs for which
  // that level is enforced for `source`. Each destination URL it is mapped to
  // the highest level, if there are multiple applicable rules. Requires
  // `restriction` to be clipboard or files.
  virtual AggregatedDestinations GetAggregatedDestinations(
      const GURL& source,
      Restriction restriction) const = 0;

  // Returns the URL pattern that `source_url` is matched against. The returned
  // URL pattern should be configured in a policy rule with the same
  // `restriction` and `level`.
  virtual std::string GetSourceUrlPattern(
      const GURL& source_url,
      Restriction restriction,
      Level level,
      RuleMetadata* out_rule_metadata) const = 0;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DLP_RULES_MANAGER_BASE_H_
