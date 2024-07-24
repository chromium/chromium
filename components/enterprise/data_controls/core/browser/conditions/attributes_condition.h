// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_ATTRIBUTES_CONDITION_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_ATTRIBUTES_CONDITION_H_

#include <memory>

#include "base/values.h"
#include "components/enterprise/data_controls/core/browser/conditions/condition.h"
#include "components/url_matcher/url_matcher.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <set>

#include "components/enterprise/data_controls/core/browser/component.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace data_controls {

// Implementation of the "root" level condition of a Data Control policy, which
// evaluates all the attributes in an `ActionContext`. This class is a
// representation of the following JSON:
// {
//   urls: [string],
//   incognito: bool,
//   os_clipboard: bool,
//   components: [ARC|CROSTINI|PLUGIN_VM|DRIVE|USB], <= CrOS only
// }
// This can represent either the `sources` or `destinations` fields of the
// DataLeakPreventionRulesList policy (see sub-classes below).
class AttributesCondition {
 public:
  // Constants used to parse sub-dictionaries of Data Controls policies that
  // should map to an AttributesCondition. If an attribute is not used outside
  // of this class, declare it in the anonymous namespace instead.
  static constexpr char kKeyUrls[] = "urls";
  static constexpr char kKeyIncognito[] = "incognito";
  static constexpr char kKeyOsClipboard[] = "os_clipboard";
  static constexpr char kKeyOtherProfile[] = "other_profile";
#if BUILDFLAG(IS_CHROMEOS)
  static constexpr char kKeyComponents[] = "components";
#endif  // BUILDFLAG(IS_CHROMEOS)

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

  // Helpers that compare a given value from a destination/source context to its
  // corresponding conditions.
  bool IncognitoMatches(bool incognito) const;
  bool OsClipboardMatches(bool os_clipboard) const;
  bool OtherProfileMatches(bool other_profile) const;

  // Helpers to help check which attributes are meaningful to the condition.
  bool is_os_clipboard_condition() const;

 private:
  // These attributes represent tab-specific conditions, and are only evaluated
  // for destinations/sources representing tabs. Attributes in a single
  // `AttributesCondition` must all match for the condition to be triggered and
  // as such have no precedence between each other. Null/empty values mean the
  // corresponding attribute was not set in the JSON initializing this
  // `AttributesCondition`, and such attributes are ignored.
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;
  std::optional<bool> incognito_;

  // This attribute indicates the destination/source condition must/mustn't be
  // the OS clipboard. It is always null for non-clipboard conditions.
  std::optional<bool> os_clipboard_;

  // This attribute indicates the destination/source condition must/mustn't be
  // a separate Chrome profile. It is always null for non-clipboard conditions.
  std::optional<bool> other_profile_;

#if BUILDFLAG(IS_CHROMEOS)
  // A destination/source must be in this set to pass the condition, unless the
  // set is empty.
  std::set<Component> components_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// Source-only AttributeCondition that also implement the `Condition` interface.
class SourceAttributesCondition : public AttributesCondition, public Condition {
 public:
  // Returns nullptr if the passed JSON doesn't match the expected schema.
  static std::unique_ptr<Condition> Create(const base::Value& value);
  static std::unique_ptr<Condition> Create(const base::Value::Dict& value);

  // data_controls::Condition:
  bool CanBeEvaluated(const ActionContext& action_context) const override;
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

  // data_controls::Condition:
  bool CanBeEvaluated(const ActionContext& action_context) const override;
  bool IsTriggered(const ActionContext& action_context) const override;

 private:
  explicit DestinationAttributesCondition(
      AttributesCondition&& attributes_condition);
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CONDITIONS_ATTRIBUTES_CONDITION_H_
