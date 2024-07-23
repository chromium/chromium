// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/conditions/attributes_condition.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "components/url_matcher/url_util.h"

namespace data_controls {

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

  incognito_ = value.FindBool(kKeyIncognito);
  os_clipboard_ = value.FindBool(kKeyOsClipboard);
  other_profile_ = value.FindBool(kKeyOtherProfile);

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
  bool valid = (url_matcher_ && !url_matcher_->IsEmpty()) ||
               incognito_.has_value() || os_clipboard_.has_value() ||
               other_profile_.has_value();
#if BUILDFLAG(IS_CHROMEOS)
  valid |= !components_.empty();
#endif  // BUILDFLAG(IS_CHROMEOS)
  return valid;
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

bool AttributesCondition::IncognitoMatches(bool incognito) const {
  if (!incognito_.has_value()) {
    return true;
  }

  return incognito_.value() == incognito;
}

bool AttributesCondition::OsClipboardMatches(bool os_clipboard) const {
  DCHECK(os_clipboard_.has_value());

  return os_clipboard == os_clipboard_.value();
}

bool AttributesCondition::OtherProfileMatches(bool other_profile) const {
  if (!other_profile_.has_value()) {
    return true;
  }

  return other_profile == other_profile_.value();
}

bool AttributesCondition::is_os_clipboard_condition() const {
  return os_clipboard_.has_value();
}

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

bool SourceAttributesCondition::CanBeEvaluated(
    const ActionContext& action_context) const {
  return !action_context.source.empty();
}

bool SourceAttributesCondition::IsTriggered(
    const ActionContext& action_context) const {
  if (!CanBeEvaluated(action_context)) {
    return false;
  }

  if (is_os_clipboard_condition()) {
    // This returns early as incognito, URLs, etc. don't need to be checked for
    // an OS clipboard condition.
    return OsClipboardMatches(action_context.source.os_clipboard);
  }

  return IncognitoMatches(action_context.source.incognito) &&
         OtherProfileMatches(action_context.source.other_profile) &&
         URLMatches(action_context.source.url);
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

bool DestinationAttributesCondition::CanBeEvaluated(
    const ActionContext& action_context) const {
  return !action_context.destination.empty();
}

bool DestinationAttributesCondition::IsTriggered(
    const ActionContext& action_context) const {
  if (!CanBeEvaluated(action_context)) {
    return false;
  }

  if (is_os_clipboard_condition()) {
#if BUILDFLAG(IS_CHROMEOS)
    if (!ComponentMatches(action_context.destination.component)) {
      return false;
    }
#endif
    // This returns early as incognito, URLs, etc. don't need to be checked for
    // an OS clipboard condition.
    return OsClipboardMatches(action_context.destination.os_clipboard);
  }

  return IncognitoMatches(action_context.destination.incognito) &&
         OtherProfileMatches(action_context.destination.other_profile) &&
#if BUILDFLAG(IS_CHROMEOS)
         ComponentMatches(action_context.destination.component) &&
#endif
         URLMatches(action_context.destination.url);
}

DestinationAttributesCondition::DestinationAttributesCondition(
    AttributesCondition&& attributes_condition)
    : AttributesCondition(std::move(attributes_condition)) {}

}  // namespace data_controls
