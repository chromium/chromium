// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"

#include "base/check_deref.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/crowdsourcing/test_votes_uploader.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/test_form_filler.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "components/autofill/core/browser/single_field_fillers/mock_single_field_fill_router.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestBrowserAutofillManager::TestBrowserAutofillManager(AutofillDriver* driver)
    : BrowserAutofillManager(driver) {
  test_api(*this).set_form_filler(std::make_unique<TestFormFiller>(*this));
}

TestBrowserAutofillManager::~TestBrowserAutofillManager() = default;

testing::NiceMock<MockBnplManager>*
TestBrowserAutofillManager::GetPaymentsBnplManager() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  return &mock_bnpl_manager_;
#else
  return nullptr;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

void TestBrowserAutofillManager::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  AutofillManager::OnLanguageDetermined(details);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnFormsSeen(
    const std::vector<FormData>& updated_forms,
    const std::vector<FormGlobalId>& removed_forms) {
  AutofillManager::OnFormsSeen(updated_forms, removed_forms);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnCaretMovedInFormField(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds) {
  AutofillManager::OnCaretMovedInFormField(form, field_id, caret_bounds);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnTextFieldValueChanged(
    const FormData& form,
    const FieldGlobalId& field_id,
    const base::TimeTicks timestamp) {
  AutofillManager::OnTextFieldValueChanged(form, field_id, timestamp);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnTextFieldDidScroll(
    const FormData& form,
    const FieldGlobalId& field_id) {
  AutofillManager::OnTextFieldDidScroll(form, field_id);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnSelectControlSelectionChanged(
    const FormData& form,
    const FieldGlobalId& field_id) {
  AutofillManager::OnSelectControlSelectionChanged(form, field_id);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnAskForValuesToFill(
    const FormData& form,
    const FieldGlobalId& field_id,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source,
    base::optional_ref<const PasswordSuggestionRequest> password_request) {
  AutofillManager::OnAskForValuesToFill(form, field_id, caret_bounds,
                                        trigger_source, password_request);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnFocusOnFormField(
    const FormData& form,
    const FieldGlobalId& field_id) {
  AutofillManager::OnFocusOnFormField(form, field_id);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnDidFillAutofillFormData(
    const FormData& form,
    const base::TimeTicks timestamp) {
  AutofillManager::OnDidFillAutofillFormData(form, timestamp);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnJavaScriptChangedAutofilledValue(
    const FormData& form,
    const FieldGlobalId& field_id,
    const std::u16string& old_value) {
  AutofillManager::OnJavaScriptChangedAutofilledValue(form, field_id,
                                                      old_value);
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::OnFormSubmitted(
    const FormData& form,
    const mojom::SubmissionSource source) {
  AutofillManager::OnFormSubmitted(form, source);
  ASSERT_TRUE(waiter_.Wait(0));
}

const gfx::Image& TestBrowserAutofillManager::GetCardImage(
    const CreditCard& credit_card) {
  return card_image_;
}

void TestBrowserAutofillManager::AddSeenForm(
    const FormData& form,
    const std::vector<FieldType>& heuristic_types,
    const std::vector<FieldType>& server_types,
    bool preserve_values_in_form_structure) {
  std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>
      all_heuristic_types;
  for (FieldType type : heuristic_types) {
    all_heuristic_types.push_back({{GetActiveHeuristicSource(), type}});
  }
  AddSeenForm(form, all_heuristic_types, server_types,
              preserve_values_in_form_structure);
}

void TestBrowserAutofillManager::AddSeenForm(
    const FormData& form,
    const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
        heuristic_types,
    const std::vector<FieldType>& server_types,
    bool preserve_values_in_form_structure) {
  auto form_structure = std::make_unique<FormStructure>(
      preserve_values_in_form_structure ? form : test::WithoutValues(form));
  test_api(*form_structure).SetFieldTypes(heuristic_types, server_types);
  test_api(*form_structure).AssignSections();
  AddSeenFormStructure(std::move(form_structure));
  test_api(*this).OnFormsParsed({form});
  // Awaits the CrowdsourcingManager's response if OnFormsParsed() started a
  // request. This is necessary because TestAutofillManagerWaiter fails if there
  // are pending events.
  //
  // This response, i.e., AutofillManager::OnLoadedServerPredictions(), is
  // asynchronous even if crowdsourcing is disabled.
  ASSERT_TRUE(waiter_.Wait(0));
}

void TestBrowserAutofillManager::AddSeenFormStructure(
    std::unique_ptr<FormStructure> form_structure) {
  const auto id = form_structure->global_id();
  (*mutable_form_structures())[id] = std::move(form_structure);
}

void TestBrowserAutofillManager::ClearFormStructures() {
  mutable_form_structures()->clear();
}

void TestBrowserAutofillManager::OnAskForValuesToFillTest(
    const FormData& form,
    const FieldGlobalId& field_id,
    AutofillSuggestionTriggerSource trigger_source,
    const std::optional<PasswordSuggestionRequest>& password_request) {
  gfx::PointF p =
      CHECK_DEREF(form.FindFieldByGlobalId(field_id)).bounds().origin();
  gfx::Rect caret_bounds(gfx::Point(p.x(), p.y()), gfx::Size(0, 10));
  BrowserAutofillManager::OnAskForValuesToFill(
      form, field_id, caret_bounds, trigger_source, password_request);
  ASSERT_TRUE(waiter_.Wait(0));
}

}  // namespace autofill
