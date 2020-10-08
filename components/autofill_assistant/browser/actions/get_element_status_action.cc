// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_element_status_action.h"

#include "base/i18n/case_conversion.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"

namespace autofill_assistant {
namespace {

std::string PrepareStringForMatching(const std::string& value,
                                     bool case_sensitive,
                                     bool remove_space) {
  std::string copy = value;
  if (!case_sensitive) {
    copy = base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(copy)));
  }
  if (remove_space) {
    base::EraseIf(copy, base::IsUnicodeWhitespace);
  }
  return copy;
}

GetElementStatusProto::ComparisonReport CreateComparisonReport(
    const std::string& actual,
    const std::string& expected,
    bool case_sensitive,
    bool remove_space) {
  std::string actual_for_match =
      PrepareStringForMatching(actual, case_sensitive, remove_space);
  std::string expected_for_match =
      PrepareStringForMatching(expected, case_sensitive, remove_space);

  GetElementStatusProto::ComparisonReport report;
  report.mutable_match_options()->set_case_sensitive(case_sensitive);
  report.mutable_match_options()->set_remove_space(remove_space);

  report.set_full_match(actual_for_match == expected_for_match);
  size_t pos = actual_for_match.find(expected_for_match);
  report.set_contains(pos != std::string::npos);
  report.set_starts_with(pos != std::string::npos && pos == 0);
  report.set_ends_with(pos != std::string::npos &&
                       pos == actual.size() - expected.size());

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

  delegate_->ShortWaitForElement(
      selector_, base::BindOnce(&GetElementStatusAction::OnWaitForElement,
                                weak_ptr_factory_.GetWeakPtr()));
}

void GetElementStatusAction::OnWaitForElement(
    const ClientStatus& element_status) {
  if (!element_status.ok()) {
    EndAction(element_status);
    return;
  }

  const auto& get_element_status = proto_.get_element_status();
  if (get_element_status.has_expected_value_match()) {
    CheckValue();
    return;
  }

  // TODO(b/169924567): Add option to check inner text.

  EndAction(ClientStatus(INVALID_ACTION));
}

void GetElementStatusAction::CheckValue() {
  // TODO(b/169924567): Add TextFilter option.
  delegate_->GetFieldValue(
      selector_,
      base::BindOnce(
          &GetElementStatusAction::OnGetContentForTextMatch,
          weak_ptr_factory_.GetWeakPtr(),
          proto_.get_element_status().expected_value_match().text_match()));
}

void GetElementStatusAction::OnGetContentForTextMatch(
    const GetElementStatusProto::TextMatch& expected_match,
    const ClientStatus& status,
    const std::string& text) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  std::string expected_text;
  switch (expected_match.value_source_case()) {
    case GetElementStatusProto::TextMatch::kValue:
      expected_text = expected_match.value();
      break;
    case GetElementStatusProto::TextMatch::kAutofillValue: {
      ClientStatus autofill_status =
          GetFormattedAutofillValue(expected_match.autofill_value(),
                                    delegate_->GetUserData(), &expected_text);
      if (!autofill_status.ok()) {
        EndAction(autofill_status);
        return;
      }
      break;
    }
    case GetElementStatusProto::TextMatch::VALUE_SOURCE_NOT_SET:
      EndAction(ClientStatus(INVALID_ACTION));
      return;
  }

  auto* result = processed_action_proto_->mutable_get_element_status_result();
  result->set_not_empty(!text.empty());

  bool success = true;
  if (expected_text.empty()) {
    success = text.empty();
  } else if (text.empty()) {
    success = false;
  } else {
    *result->add_reports() =
        CreateComparisonReport(text, expected_text, true, true);
    *result->add_reports() =
        CreateComparisonReport(text, expected_text, true, false);
    *result->add_reports() =
        CreateComparisonReport(text, expected_text, false, true);
    *result->add_reports() =
        CreateComparisonReport(text, expected_text, false, false);

    if (expected_match.has_match_expectation()) {
      const auto& expectation = expected_match.match_expectation();
      auto report = CreateComparisonReport(
          text, expected_text, expectation.match_options().case_sensitive(),
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
    }
  }

  result->set_match_success(success);
  EndAction(!success && proto_.get_element_status().mismatch_should_fail()
                ? ClientStatus(ELEMENT_MISMATCH)
                : OkClientStatus());
}

void GetElementStatusAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
