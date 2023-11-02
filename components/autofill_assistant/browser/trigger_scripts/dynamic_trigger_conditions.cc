// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {

namespace {

// Walks through the trigger condition tree and retrieves the set of all
// trigger conditions to check. This ensures that trigger conditions are
// evaluated only once, even if multiple trigger scripts refer to them.
void ExtractTriggerConditions(
    const TriggerScriptConditionProto& proto,
    base::flat_set<Selector>* selectors,
    base::flat_set<Selector>* dom_ready_state_selectors) {
  switch (proto.type_case()) {
    case TriggerScriptConditionProto::kAllOf:
      for (const auto& condition : proto.all_of().conditions()) {
        ExtractTriggerConditions(condition, selectors,
                                 dom_ready_state_selectors);
      }
      return;
    case TriggerScriptConditionProto::kAnyOf:
      for (const auto& condition : proto.any_of().conditions()) {
        ExtractTriggerConditions(condition, selectors,
                                 dom_ready_state_selectors);
      }
      return;
    case TriggerScriptConditionProto::kNoneOf:
      for (const auto& condition : proto.none_of().conditions()) {
        ExtractTriggerConditions(condition, selectors,
                                 dom_ready_state_selectors);
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
      selectors->insert(Selector(proto.selector()));
      return;
    case TriggerScriptConditionProto::kDocumentReadyState:
      dom_ready_state_selectors->insert(
          Selector(proto.document_ready_state().frame()));
      return;
  }
}

}  // namespace

DynamicTriggerConditions::DynamicTriggerConditions() = default;
DynamicTriggerConditions::~DynamicTriggerConditions() = default;

void DynamicTriggerConditions::AddConditionsFromTriggerScript(
    const TriggerScriptProto& proto) {
  ExtractTriggerConditions(proto.trigger_condition(), &selectors_,
                           &dom_ready_state_selectors_);
}

void DynamicTriggerConditions::ClearConditions() {
  selectors_.clear();
  dom_ready_state_selectors_.clear();
}

absl::optional<bool> DynamicTriggerConditions::GetSelectorMatches(
    const Selector& selector) const {
  auto it = selector_matches_.find(selector);
  if (it == selector_matches_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

absl::optional<DocumentReadyState>
DynamicTriggerConditions::GetDocumentReadyState(const Selector& frame) const {
  auto it = dom_ready_states_.find(frame);
  if (it == dom_ready_states_.end()) {
    return absl::nullopt;
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

  callback_ = std::move(callback);
  temporary_selector_matches_.clear();
  temporary_dom_ready_states_.clear();

  // Concurrently look up all requested selectors.
  if (selectors_.empty() && dom_ready_state_selectors_.empty()) {
    MaybeRunCallback();
    return;
  }
  for (const auto& selector : selectors_) {
    web_controller->FindElement(
        selector, /* strict = */ false,
        base::BindOnce(&DynamicTriggerConditions::OnFindElement,
                       weak_ptr_factory_.GetWeakPtr(), selector));
  }
  for (const auto& selector : dom_ready_state_selectors_) {
    if (selector.empty()) {
      web_controller->GetDocumentReadyState(
          ElementFinderResult(),
          base::BindOnce(&DynamicTriggerConditions::OnGetDocumentReadyState,
                         weak_ptr_factory_.GetWeakPtr(), selector));
    } else {
      web_controller->FindElement(
          selector, /* strict= */ false,
          base::BindOnce(
              &element_action_util::TakeElementAndGetProperty<
                  DocumentReadyState>,
              base::BindOnce(&WebController::GetDocumentReadyState,
                             web_controller->GetWeakPtr()),
              DocumentReadyState::DOCUMENT_UNKNOWN_READY_STATE,
              base::BindOnce(&DynamicTriggerConditions::OnGetDocumentReadyState,
                             weak_ptr_factory_.GetWeakPtr(), selector)));
    }
  }
}

bool DynamicTriggerConditions::HasResults() const {
  return selector_matches_.size() == selectors_.size();
}

void DynamicTriggerConditions::OnFindElement(
    const Selector& selector,
    const ClientStatus& client_status,
    std::unique_ptr<ElementFinderResult> element) {
  temporary_selector_matches_.emplace(
      std::make_pair(selector, client_status.ok()));

  if (temporary_selector_matches_.size() == selectors_.size()) {
    selector_matches_ = temporary_selector_matches_;
    MaybeRunCallback();
  }
}

void DynamicTriggerConditions::OnGetDocumentReadyState(
    const Selector& selector,
    const ClientStatus& client_status,
    DocumentReadyState document_ready_state) {
  temporary_dom_ready_states_.emplace(
      std::make_pair(selector, document_ready_state));
  if (temporary_dom_ready_states_.size() == dom_ready_state_selectors_.size()) {
    dom_ready_states_ = temporary_dom_ready_states_;
    MaybeRunCallback();
  }
}

void DynamicTriggerConditions::MaybeRunCallback() {
  if (temporary_selector_matches_.size() != selectors_.size() ||
      temporary_dom_ready_states_.size() != dom_ready_state_selectors_.size()) {
    return;
  }
  std::move(callback_).Run();
}

}  // namespace autofill_assistant
