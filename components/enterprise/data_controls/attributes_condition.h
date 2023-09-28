// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_ATTRIBUTES_CONDITION_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_ATTRIBUTES_CONDITION_H_

#include <memory>

#include "base/values.h"
#include "components/enterprise/data_controls/condition.h"
#include "components/url_matcher/url_matcher.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <set>

#include "components/enterprise/data_controls/component.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace data_controls {

// Implementation of the "root" level condition of a Data Control policy, which
// evaluates all the attributes in an `ActionContext`. This class is a
// representation of the following JSON:
// {
//   urls: [string],
//   components: [ARC|CROSTINI|PLUGIN_VM|DRIVE|USB], <= CrOS only
// }
// This can represent either the `sources` or `destinations` fields of the
// DataLeakPreventionRulesList policy (see sub-classes below).
class AttributesCondition {
 public:
  explicit AttributesCondition(const base::Value::Dict& value);
  AttributesCondition(AttributesCondition&& other);
  ~AttributesCondition();

  // Returns true if at least one of the internal values is non-null/empty, aka
  // if the JSON represented a valid condition. This should only be checked
  // right after an `AttributesCondition` is constructed, and other methods
  // should not be used if this returns false.
  bool IsValid() const;

 protected:
  // Returns true if `url` should be considered to trigger the condition.
  bool URLMatches(GURL url) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Return true if `component` should be considered to trigger the condition.
  bool ComponentMatches(Component component) const;
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
#if BUILDFLAG(IS_CHROMEOS)
  std::set<Component> components_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// Source-only AttributeCondition that also implement the `Condition` interface.
class SourceAttributesCondition : public AttributesCondition, public Condition {
 public:
  // Returns nullptr if the passed JSON doesn't match the expected schema.
  static std::unique_ptr<Condition> Create(const base::Value& value);
  static std::unique_ptr<Condition> Create(const base::Value::Dict& value);

  bool IsTriggered(const ActionContext& action_context) const override;

 private:
  explicit SourceAttributesCondition(
      AttributesCondition&& attributes_condition);
};

// Destination-only AttributeCondition that also implement the `Condition`
// interface.
class DestinationAttributesCondition : public AttributesCondition,
                                       public Condition {
 public:
  // Returns nullptr if the passed JSON doesn't match the expected schema.
  static std::unique_ptr<Condition> Create(const base::Value& value);
  static std::unique_ptr<Condition> Create(const base::Value::Dict& value);

  bool IsTriggered(const ActionContext& action_context) const override;

 private:
  explicit DestinationAttributesCondition(
      AttributesCondition&& attributes_condition);
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_ATTRIBUTES_CONDITION_H_
