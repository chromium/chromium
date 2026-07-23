// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_MANAGER_H_

#include <concepts>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace autofill {

// This class makes AutofillManager renderer event handlers synchronous.
//
// AutofillManager::OnFoo() calls AutofillManager::OnFooImpl() asynchronously if
// the form was parsed. This class waits for the asynchronous form parsing with
// a TestAutofillManagerWaiter.
//
// The template is supposed to be useful for both Android platform Autofill
// (primarily used on WebView) and Chrome Autofill:
// - TestAutofillManagerTemplate<AndroidAutofillManager>
// - TestAutofillManagerTemplate<BrowserAutofillManager>
//
// For Chrome Autofill tests, use TestBrowserAutofillManager instead of using
// TestAutofillManagerTemplate<BrowserAutofillManager> directly.
template <typename T>
  requires(std::derived_from<T, AutofillManager>)
class TestAutofillManagerTemplate : public T {
 public:
  using T::T;
  TestAutofillManagerTemplate(const TestAutofillManagerTemplate&) = delete;
  TestAutofillManagerTemplate& operator=(const TestAutofillManagerTemplate&) =
      delete;
  ~TestAutofillManagerTemplate() override = default;

  // AutofillManager overrides.
  // The overrides ensure that the thread is blocked until the form has been
  // parsed.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override {
    AutofillManager::OnLanguageDetermined(details);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnFormsSeen(std::vector<FormData> updated_forms,
                   std::vector<FormGlobalId> removed_forms,
                   T::RendererEventPassKey pass_key) override {
    AutofillManager::OnFormsSeen(std::move(updated_forms),
                                 std::move(removed_forms), pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnCaretMovedInFormField(const FormData& form,
                               const FieldGlobalId& field_id,
                               const gfx::Rect& caret_bounds,
                               T::RendererEventPassKey pass_key) override {
    AutofillManager::OnCaretMovedInFormField(form, field_id, caret_bounds,
                                             pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnTextFieldValueChanged(const FormData& form,
                               const FieldGlobalId& field_id,
                               const base::TimeTicks timestamp,
                               T::RendererEventPassKey pass_key) override {
    AutofillManager::OnTextFieldValueChanged(form, field_id, timestamp,
                                             pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnDidEndTextFieldEditing(T::RendererEventPassKey pass_key) override {
    AutofillManager::OnDidEndTextFieldEditing(pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnTextFieldDidScroll(const FormData& form,
                            const FieldGlobalId& field_id,
                            T::RendererEventPassKey pass_key) override {
    AutofillManager::OnTextFieldDidScroll(form, field_id, pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnSelectControlSelectionChanged(
      const FormData& form,
      const FieldGlobalId& field_id,
      T::RendererEventPassKey pass_key) override {
    AutofillManager::OnSelectControlSelectionChanged(form, field_id, pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnSelectFieldOptionsDidChange(
      const FormData& form,
      const FieldGlobalId& field_id,
      T::RendererEventPassKey pass_key) override {
    AutofillManager::OnSelectFieldOptionsDidChange(form, field_id, pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnAskForValuesToFill(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source,
      std::optional<PasswordSuggestionRequest> password_request,
      T::RendererEventPassKey pass_key) override {
    AutofillManager::OnAskForValuesToFill(
        form, field_id, caret_bounds, trigger_source,
        std::move(password_request), pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnFocusOnFormField(const FormData& form,
                          const FieldGlobalId& field_id,
                          T::RendererEventPassKey pass_key) override {
    AutofillManager::OnFocusOnFormField(form, field_id, pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnDidAutofillForm(const FormData& form,
                         T::RendererEventPassKey pass_key) override {
    AutofillManager::OnDidAutofillForm(form, pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnJavaScriptChangedAutofilledValue(
      const FormData& form,
      const FieldGlobalId& field_id,
      const std::u16string& old_value,
      T::RendererEventPassKey pass_key) override {
    AutofillManager::OnJavaScriptChangedAutofilledValue(form, field_id,
                                                        old_value, pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  void OnFormSubmitted(const FormData& form,
                       const mojom::SubmissionSource source,
                       T::RendererEventPassKey pass_key) override {
    AutofillManager::OnFormSubmitted(form, source, pass_key);
    ASSERT_TRUE(waiter_.Wait(0));
  }

  // Unique to TestAutofillManager:

  void AddSeenForm(const FormData& form,
                   const std::vector<FieldType>& field_types) {
    AddSeenForm(form, /*heuristic_types=*/field_types,
                /*server_types=*/field_types);
  }

  void AddSeenForm(const FormData& form,
                   const std::vector<FieldType>& heuristic_types,
                   const std::vector<FieldType>& server_types) {
    std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>
        all_heuristic_types;
    for (FieldType type : heuristic_types) {
      all_heuristic_types.push_back({{GetActiveHeuristicSource(), type}});
    }
    AddSeenForm(form, all_heuristic_types, server_types);
  }

  void AddSeenForm(
      const FormData& form,
      const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
          heuristic_types,
      const std::vector<FieldType>& server_types) {
    auto form_structure = std::make_unique<FormStructure>(form);
    test_api(*form_structure).SetFieldTypes(heuristic_types, server_types);
    test_api(*form_structure).AssignSections();
    test_api(*this).AddSeenFormStructure(std::move(form_structure));
    test_api(*this).OnFormsParsed({form});
    // Awaits the CrowdsourcingManager's response if OnFormsParsed() started a
    // request. This is necessary because TestAutofillManagerWaiter fails if
    // there are pending events.
    //
    // This response, i.e., AutofillManager::OnLoadedServerPredictions(), is
    // asynchronous even if crowdsourcing is disabled.
    ASSERT_TRUE(waiter_.Wait(0));
  }

  // Helper to skip irrelevant params.
  void OnAskForValuesToFillTest(
      const FormData& form,
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kTextFieldValueChanged,
      std::optional<PasswordSuggestionRequest> password_request =
          std::nullopt) {
    gfx::PointF p =
        CHECK_DEREF(form.FindFieldByGlobalId(field_id)).bounds().origin();
    gfx::Rect caret_bounds(gfx::ToRoundedPoint(p), gfx::Size(0, 10));
    OnAskForValuesToFill(form, field_id, caret_bounds, trigger_source,
                         std::move(password_request),
                         AutofillManagerTestApi::pass_key());
  }

 private:
  TestAutofillManagerWaiter waiter_{*this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_MANAGER_H_
