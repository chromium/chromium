// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {

namespace {

// Extracts all selectors from the condition tree in |proto| and writes them to
// |results|.
void ExtractSelectors(const TriggerScriptConditionProto& proto,
                      std::set<Selector>* results) {
  switch (proto.type_case()) {
    case TriggerScriptConditionProto::kAllOf:
      for (const auto& condition : proto.all_of().conditions()) {
        ExtractSelectors(condition, results);
      }
      return;
    case TriggerScriptConditionProto::kAnyOf:
      for (const auto& condition : proto.any_of().conditions()) {
        ExtractSelectors(condition, results);
      }
      return;
    case TriggerScriptConditionProto::kNoneOf:
      for (const auto& condition : proto.none_of().conditions()) {
        ExtractSelectors(condition, results);
      }
      return;
    case TriggerScriptConditionProto::kStoredLoginCredentials:
    case TriggerScriptConditionProto::kIsFirstTimeUser:
    case TriggerScriptConditionProto::kExperimentId:
    case TriggerScriptConditionProto::kKeyboardHidden:
    case TriggerScriptConditionProto::kScriptParameterMatch:
    case TriggerScriptConditionProto::kPathPattern:
    case TriggerScriptConditionProto::kDomainWithScheme:
    case TriggerScriptConditionProto::TYPE_NOT_SET:
      return;
    case TriggerScriptConditionProto::kSelector:
      results->insert(Selector(proto.selector()));
      return;
  }
}

}  // namespace

DynamicTriggerConditions::DynamicTriggerConditions() = default;
DynamicTriggerConditions::~DynamicTriggerConditions() = default;

void DynamicTriggerConditions::AddSelectorsFromTriggerScript(
    const TriggerScriptProto& proto) {
  ExtractSelectors(proto.trigger_condition(), &selectors_);
}

void DynamicTriggerConditions::ClearSelectors() {
  selectors_.clear();
}

base::Optional<bool> DynamicTriggerConditions::GetSelectorMatches(
    const Selector& selector) const {
  auto it = selector_matches_.find(selector);
  if (it == selector_matches_.end()) {
    return base::nullopt;
  }
  return it->second;
}

void DynamicTriggerConditions::SetKeyboardVisible(bool visible) {
  keyboard_visible_ = visible;
}

bool DynamicTriggerConditions::GetKeyboardVisible() const {
  return keyboard_visible_;
}

void DynamicTriggerConditions::SetURL(const GURL& url) {
  url_ = url;
}

bool DynamicTriggerConditions::GetPathPatternMatches(
    const std::string& path_pattern) const {
  const re2::RE2 re(path_pattern);
  if (!re.ok()) {
    DCHECK(false)
        << "Should never happen, regexp validity is checked in protocol_utils.";
    return false;
  }

  const std::string url_path =
      url_.has_ref() ? base::StrCat({url_.PathForRequest(), "#", url_.ref()})
                     : url_.PathForRequest();
  return re.Match(url_path, 0, url_path.size(), re2::RE2::ANCHOR_BOTH, nullptr,
                  0);
}

bool DynamicTriggerConditions::GetDomainAndSchemeMatches(
    const GURL& domain_with_scheme) const {
  if (!domain_with_scheme.is_valid()) {
    DCHECK(false)
        << "Should never happen, domain format is checked in protocol_utils.";
    return false;
  }

  // We require the scheme and host parts to match.
  // TODO(crbug.com/806868): Consider using Origin::IsSameOriginWith here.
  return domain_with_scheme.scheme() == url_.scheme() &&
         domain_with_scheme.host() == url_.host();
}

void DynamicTriggerConditions::Update(WebController* web_controller,
                                      base::OnceCallback<void(void)> callback) {
  DCHECK(!callback_) << "Update called while already in progress";
  if (callback_) {
    return;
  }

  temporary_selector_matches_.clear();
  if (selectors_.empty()) {
    selector_matches_ = temporary_selector_matches_;
    std::move(callback).Run();
    return;
  }

  callback_ = std::move(callback);
  for (const auto& selector : selectors_) {
    web_controller->FindElement(
        selector, /* strict = */ false,
        base::BindOnce(&DynamicTriggerConditions::OnFindElement,
                       weak_ptr_factory_.GetWeakPtr(), selector));
  }
}

bool DynamicTriggerConditions::HasResults() const {
  return selector_matches_.size() == selectors_.size();
}

void DynamicTriggerConditions::OnFindElement(
    const Selector& selector,
    const ClientStatus& client_status,
    std::unique_ptr<ElementFinder::Result> element) {
  temporary_selector_matches_.emplace(
      std::make_pair(selector, client_status.ok()));
  if (temporary_selector_matches_.size() == selectors_.size()) {
    selector_matches_ = temporary_selector_matches_;
    std::move(callback_).Run();
  }
}

}  // namespace autofill_assistant
