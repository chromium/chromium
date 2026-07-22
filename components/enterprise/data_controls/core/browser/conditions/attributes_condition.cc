// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/conditions/attributes_condition.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/url_matcher/url_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace data_controls {

AttributesCondition::~AttributesCondition() = default;

AttributesCondition::AttributesCondition(const base::DictValue& value) {
  const base::ListValue* urls_value = value.FindList(kKeyUrls);
  if (urls_value) {
    for (const base::Value& url_pattern : *urls_value) {
      if (!url_pattern.is_string()) {
        return;
      }
    }

    auto url_matcher = std::make_unique<url_matcher::URLMatcher>();
    base::MatcherStringPattern::ID id(0);
    url_matcher::util::AddFiltersWithLimit(url_matcher.get(), true, &id,
                                           *urls_value);

    if (!url_matcher->IsEmpty()) {
      url_matcher_ = std::move(url_matcher);
    }
  }

  incognito_ = value.FindBool(kKeyIncognito);
  os_clipboard_ = value.FindBool(kKeyOsClipboard);
  other_profile_ = value.FindBool(kKeyOtherProfile);
  gemini_in_chrome_ = value.FindBool(kKeyGeminiInChrome);

  if (base::FeatureList::IsEnabled(kDataControlsUrlRegexAndSizeAttributes)) {
    // `base::Value::FindInt` returns `std::optional<int>` (32-bit signed int),
    // which overflows and returns `std::nullopt` for file size thresholds over
    // 2 GB (INT32_MAX). Using `FindDouble` + `saturated_cast<int64_t>`
    // safely preserves exact integer thresholds up to 9 Petabytes (2^53).
    if (std::optional<double> higher = value.FindDouble(kKeySizeHigherThan)) {
      min_size_ = base::saturated_cast<int64_t>(*higher);
    }
    if (std::optional<double> lower = value.FindDouble(kKeySizeLowerThan)) {
      max_size_ = base::saturated_cast<int64_t>(*lower);
    }
    const base::ListValue* url_regexprs_value =
        value.FindList(kKeyUrlRegexprs);
    if (url_regexprs_value) {
      for (const base::Value& url_regexp : *url_regexprs_value) {
        if (!url_regexp.is_string()) {
          continue;
        }
        auto re = std::make_unique<re2::RE2>(url_regexp.GetString());
        if (re->ok()) {
          url_regexprs_.push_back(std::move(re));
        } else {
          LOG(WARNING)
              << "Failed to parse Data Controls URL regular expression \""
              << url_regexp.GetString() << "\": " << re->error();
        }
      }
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  const base::ListValue* components_value = value.FindList(kKeyComponents);
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
               !url_regexprs_.empty() ||
               incognito_.has_value() || os_clipboard_.has_value() ||
               other_profile_.has_value() || gemini_in_chrome_.has_value() ||
               min_size_.has_value() || max_size_.has_value();
#if BUILDFLAG(IS_CHROMEOS)
  valid |= !components_.empty();
#endif  // BUILDFLAG(IS_CHROMEOS)
  return valid;
}

bool AttributesCondition::SizeMatches(
    std::optional<int64_t> optional_size) const {
  if (!is_size_condition()) {
    return true;
  }
  return optional_size &&
         *optional_size > std::max<int64_t>(min_size_.value_or(-1), -1) &&
         *optional_size < max_size_.value_or(INT64_MAX);
}

bool AttributesCondition::URLMatches(GURL url) const {
  // Without URLs to match, any URL is considered to pass the condition.
  if (!url_matcher_ && url_regexprs_.empty()) {
    return true;
  }

  // With URLs to match, an invalid URL is considered as not matching the
  // condition.
  if (!url.is_valid()) {
    return false;
  }

  if (url_matcher_ && !url_matcher_->MatchURL(url).empty()) {
    return true;
  }
  for (const auto& re : url_regexprs_) {
    if (re2::RE2::PartialMatch(url.spec(), *re)) {
      return true;
    }
  }
  return false;
}

#if BUILDFLAG(IS_CHROMEOS)
bool AttributesCondition::ComponentMatches(Component component) const {
  // Without components to match, any URL is considered to pass the condition.
  if (components_.empty()) {
    return true;
  }

  // With components to match, `component` needs to be in the set to pass the
  // condition.
  return components_.contains(component);
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

bool AttributesCondition::GeminiInChromeMatches(bool gemini_in_chrome) const {
  if (!gemini_in_chrome_.has_value()) {
    return true;
  }

  return gemini_in_chrome == gemini_in_chrome_.value();
}

bool AttributesCondition::is_os_clipboard_condition() const {
  return os_clipboard_.has_value();
}

bool AttributesCondition::is_gemini_in_chrome_condition() const {
  return gemini_in_chrome_.has_value();
}

bool AttributesCondition::is_size_condition() const {
  return min_size_.has_value() || max_size_.has_value();
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
    const base::DictValue& value) {
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
    return OsClipboardMatches(action_context.source.os_clipboard) &&
           SizeMatches(action_context.source.content_size);
  }

  // TODO(crbug.com/510383413): Support combining `gemini_in_chrome` with
  // profile-bound attributes like `incognito`.
  if (is_gemini_in_chrome_condition()) {
    return GeminiInChromeMatches(action_context.source.gemini_in_chrome) &&
           SizeMatches(action_context.source.content_size);
  }

  return IncognitoMatches(action_context.source.incognito) &&
         OtherProfileMatches(action_context.source.other_profile) &&
         SizeMatches(action_context.source.content_size) &&
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
    const base::DictValue& value) {
  AttributesCondition attributes_condition(value);
  // Destination conditions do not check file or clipboard content size. Schema
  // validation in `Rule::AddUnsupportedAttributeErrors` rejects size attributes
  // under destinations; returning nullptr here ensures any invalid condition
  // that bypasses schema validation is safely dropped.
  if (!attributes_condition.IsValid() ||
      attributes_condition.is_size_condition()) {
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

  // TODO(crbug.com/510383413): Support combining `gemini_in_chrome` with
  // profile-bound attributes like `incognito`.
  if (is_gemini_in_chrome_condition()) {
    return GeminiInChromeMatches(action_context.destination.gemini_in_chrome);
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
