// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/attributes_condition.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "components/url_matcher/url_util.h"

namespace data_controls {

namespace {

// Constants used to parse sub-dictionaries of DLP policies that should map to
// an AttributesCondition.
constexpr char kKeyUrls[] = "urls";

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kKeyComponents[] = "components";
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

AttributesCondition::~AttributesCondition() = default;

AttributesCondition::AttributesCondition(const base::Value::Dict& value) {
  const base::Value::List* urls_value = value.FindList(kKeyUrls);
  if (urls_value) {
    for (const base::Value& url_pattern : *urls_value) {
      if (!url_pattern.is_string()) {
        return;
      }
    }

    auto url_matcher = std::make_unique<url_matcher::URLMatcher>();
    base::MatcherStringPattern::ID id(0);
    url_matcher::util::AddFilters(url_matcher.get(), true, &id, *urls_value);

    if (!url_matcher->IsEmpty()) {
      url_matcher_ = std::move(url_matcher);
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  const base::Value::List* components_value = value.FindList(kKeyComponents);
  if (components_value) {
    std::set<Component> components;
    for (const auto& component_string : *components_value) {
      if (!component_string.is_string()) {
        continue;
      }

      Component component = GetComponentMapping(component_string.GetString());
      if (component != Component::kUnknownComponent) {
        components.insert(component);
      }
    }
    components_ = std::move(components);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

AttributesCondition::AttributesCondition(AttributesCondition&& other) = default;

bool AttributesCondition::IsValid() const {
#if BUILDFLAG(IS_CHROMEOS)
  return (url_matcher_ && !url_matcher_->IsEmpty()) || !components_.empty();
#else
  return url_matcher_ && !url_matcher_->IsEmpty();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool AttributesCondition::URLMatches(GURL url) const {
  // Without URLs to match, any URL is considered to pass the condition.
  if (!url_matcher_) {
    return true;
  }

  // With URLs to match, an invalid URL is considered as not matching the
  // condition.
  if (!url.is_valid()) {
    return false;
  }

  return !url_matcher_->MatchURL(url).empty();
}

#if BUILDFLAG(IS_CHROMEOS)
bool AttributesCondition::ComponentMatches(Component component) const {
  // Without components to match, any URL is considered to pass the condition.
  if (components_.empty()) {
    return true;
  }

  // With components to match, `component` needs to be in the set to pass the
  // condition.
  return base::Contains(components_, component);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// static
std::unique_ptr<Condition> SourceAttributesCondition::Create(
    const base::Value& value) {
  if (!value.is_dict()) {
    return nullptr;
  }

  return SourceAttributesCondition::Create(value.GetDict());
}

// static
std::unique_ptr<Condition> SourceAttributesCondition::Create(
    const base::Value::Dict& value) {
  AttributesCondition attributes_condition(value);
  if (!attributes_condition.IsValid()) {
    return nullptr;
  }
  return base::WrapUnique(
      new SourceAttributesCondition(std::move(attributes_condition)));
}

bool SourceAttributesCondition::IsTriggered(
    const ActionContext& action_context) const {
  return URLMatches(action_context.source.url);
}

SourceAttributesCondition::SourceAttributesCondition(
    AttributesCondition&& attributes_condition)
    : AttributesCondition(std::move(attributes_condition)) {}

// static
std::unique_ptr<Condition> DestinationAttributesCondition::Create(
    const base::Value& value) {
  if (!value.is_dict()) {
    return nullptr;
  }

  return DestinationAttributesCondition::Create(value.GetDict());
}

// static
std::unique_ptr<Condition> DestinationAttributesCondition::Create(
    const base::Value::Dict& value) {
  AttributesCondition attributes_condition(value);
  if (!attributes_condition.IsValid()) {
    return nullptr;
  }
  return base::WrapUnique(
      new DestinationAttributesCondition(std::move(attributes_condition)));
}

bool DestinationAttributesCondition::IsTriggered(
    const ActionContext& action_context) const {
#if BUILDFLAG(IS_CHROMEOS)
  if (!ComponentMatches(action_context.destination.component)) {
    return false;
  }
#endif
  return URLMatches(action_context.destination.url);
}

DestinationAttributesCondition::DestinationAttributesCondition(
    AttributesCondition&& attributes_condition)
    : AttributesCondition(std::move(attributes_condition)) {}

}  // namespace data_controls
