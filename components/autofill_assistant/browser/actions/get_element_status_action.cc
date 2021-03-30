// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_element_status_action.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/web/web_controller.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {
namespace {

struct MaybeRe2 {
  std::string value;
  bool is_re2 = false;
};

std::string RemoveWhitespace(const std::string& value) {
  std::string copy = value;
  base::EraseIf(copy, base::IsUnicodeWhitespace);
  return copy;
}

GetElementStatusProto::ComparisonReport CreateComparisonReport(
    const std::string& actual,
    const MaybeRe2& re2,
    bool case_sensitive,
    bool remove_space) {
  GetElementStatusProto::ComparisonReport report;
  report.mutable_match_options()->set_case_sensitive(case_sensitive);
  report.mutable_match_options()->set_remove_space(remove_space);

  std::string actual_for_match =
      remove_space ? RemoveWhitespace(actual) : actual;
  report.set_empty(actual_for_match.empty());

  std::string value_for_match =
      !re2.is_re2 && remove_space ? RemoveWhitespace(re2.value) : re2.value;

  if (!re2.is_re2 && value_for_match.empty()) {
    if (actual_for_match.empty()) {
      report.set_expected_empty_match(true);
      report.set_full_match(true);
      report.set_contains(true);
      report.set_starts_with(true);
      report.set_ends_with(true);
    }
    return report;
  }

  std::string re2_for_match =
      re2.is_re2 ? re2.value : re2::RE2::QuoteMeta(value_for_match);

  re2::RE2::Options options;
  options.set_case_sensitive(case_sensitive);
  re2::RE2 regexp(re2_for_match, options);
  std::string match;
  bool found_match = RE2::Extract(actual_for_match, regexp, "\\0", &match);

  if (!found_match) {
    return report;
  }

  report.set_expected_empty_match(match.empty());
  report.set_full_match(actual_for_match == match);
  size_t pos = actual_for_match.find(match);
  report.set_contains(pos != std::string::npos);
  report.set_starts_with(pos != std::string::npos && pos == 0);
  report.set_ends_with(pos != std::string::npos &&
                       pos == actual_for_match.size() - match.size());
  return report;
}

}  // namespace

GetElementStatusAction::GetElementStatusAction(ActionDelegate* delegate,
                                               const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_get_element_status());
}

GetElementStatusAction::~GetElementStatusAction() = default;

void GetElementStatusAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);
  selector_ = Selector(proto_.get_element_status().element());

  if (selector_.empty()) {
    VLOG(1) << __func__ << ": empty selector";
    EndAction(ClientStatus(INVALID_SELECTOR));
    return;
  }

  delegate_->ShortWaitForElementWithSlowWarning(
      selector_,
      base::BindOnce(&GetElementStatusAction::OnWaitForElementTimed,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::BindOnce(&GetElementStatusAction::OnWaitForElement,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void GetElementStatusAction::OnWaitForElement(
    const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  std::vector<std::string> attribute_list;
  switch (proto_.get_element_status().value_source()) {
    case GetElementStatusProto::VALUE:
      attribute_list.emplace_back("value");
      break;
    case GetElementStatusProto::INNER_TEXT:
      attribute_list.emplace_back("innerText");
      break;
    case GetElementStatusProto::NOT_SET:
      EndAction(ClientStatus(INVALID_ACTION));
      return;
  }

  delegate_->FindElement(
      selector_,
      base::BindOnce(
          &action_delegate_util::TakeElementAndGetProperty<std::string>,
          base::BindOnce(&WebController::GetStringAttribute,
                         delegate_->GetWebController()->GetWeakPtr(),
                         attribute_list),
          base::BindOnce(&GetElementStatusAction::OnGetStringAttribute,
                         weak_ptr_factory_.GetWeakPtr())));
}

void GetElementStatusAction::OnGetStringAttribute(const ClientStatus& status,
                                                  const std::string& text) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  const auto& expected_match =
      proto_.get_element_status().expected_value_match().text_match();
  MaybeRe2 expected_re2;
  switch (expected_match.value_source_case()) {
    case GetElementStatusProto::TextMatch::kValue:
      expected_re2.value = expected_match.value();
      break;
    case GetElementStatusProto::TextMatch::kAutofillValue: {
      ClientStatus autofill_status = GetFormattedAutofillValue(
          expected_match.autofill_value(), delegate_->GetUserData(),
          &expected_re2.value);
      if (!autofill_status.ok()) {
        EndAction(autofill_status);
        return;
      }
      break;
    }
    case GetElementStatusProto::TextMatch::kRe2:
      expected_re2.value = expected_match.re2();
      expected_re2.is_re2 = true;
      break;
    case GetElementStatusProto::TextMatch::VALUE_SOURCE_NOT_SET:
      EndAction(ClientStatus(INVALID_ACTION));
      return;
  }

  auto* result = processed_action_proto_->mutable_get_element_status_result();
  result->set_not_empty(!text.empty());

  bool success = true;
  *result->add_reports() = CreateComparisonReport(
      text, expected_re2, /* case_sensitive= */ true, /* remove_space= */ true);
  *result->add_reports() =
      CreateComparisonReport(text, expected_re2, /* case_sensitive= */ true,
                             /* remove_space= */ false);
  *result->add_reports() =
      CreateComparisonReport(text, expected_re2, /* case_sensitive= */ false,
                             /* remove_space= */ true);
  *result->add_reports() =
      CreateComparisonReport(text, expected_re2, /* case_sensitive= */ false,
                             /* remove_space= */ false);

  if (expected_match.has_match_expectation()) {
    const auto& expectation = expected_match.match_expectation();
    auto report = CreateComparisonReport(
        text, expected_re2, expectation.match_options().case_sensitive(),
        expectation.match_options().remove_space());

    switch (expectation.match_level_case()) {
      case GetElementStatusProto::MatchExpectation::MATCH_LEVEL_NOT_SET:
      case GetElementStatusProto::MatchExpectation::kFullMatch:
        success = report.full_match();
        break;
      case GetElementStatusProto::MatchExpectation::kContains:
        success = report.contains();
        break;
      case GetElementStatusProto::MatchExpectation::kStartsWith:
        success = report.starts_with();
        break;
      case GetElementStatusProto::MatchExpectation::kEndsWith:
        success = report.ends_with();
        break;
    }

    result->set_expected_empty_match(report.expected_empty_match());
    result->set_match_success(success);
  }

  EndAction(!success && proto_.get_element_status().mismatch_should_fail()
                ? ClientStatus(ELEMENT_MISMATCH)
                : OkClientStatus());
}

void GetElementStatusAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
