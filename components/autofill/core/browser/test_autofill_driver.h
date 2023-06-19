// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "ui/accessibility/ax_tree_id.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_IOS)
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif

namespace autofill {

// This class is only for easier writing of tests. There are two instances of
// the template:
//
// - TestAutofillDriver is a simple AutofillDriver;
// - TestContentAutofillDriver is a ContentAutofillDriver, i.e., is associated
//   to a content::WebContents and has a ContentAutofillDriverFactory
//
// As a rule of thumb, TestContentAutofillDriver is preferable in tests that
// have a content::WebContents.
template <typename T>
class TestAutofillDriverTemplate : public T {
 public:
  static_assert(std::is_base_of_v<AutofillDriver, T>);

  using T::T;
  TestAutofillDriverTemplate(const TestAutofillDriverTemplate&) = delete;
  TestAutofillDriverTemplate& operator=(const TestAutofillDriverTemplate&) =
      delete;
  ~TestAutofillDriverTemplate() override = default;

  // AutofillDriver:
  LocalFrameToken GetFrameToken() const override { return {}; }
  TestAutofillDriverTemplate* GetParent() override { return nullptr; }
  absl::optional<LocalFrameToken> Resolve(FrameToken query) override {
    return absl::nullopt;
  }
  bool IsInActiveFrame() const override { return is_in_active_frame_; }
  bool IsInAnyMainFrame() const override { return is_in_any_main_frame_; }
  bool IsPrerendering() const override { return false; }
  bool HasSharedAutofillPermission() const override { return false; }
  bool CanShowAutofillUi() const override { return true; }
  ui::AXTreeID GetAxTreeId() const override {
    NOTIMPLEMENTED() << "See https://crbug.com/985933";
    return ui::AXTreeIDUnknown();
  }
  bool RendererIsAvailable() override { return true; }
  void HandleParsedForms(const std::vector<FormData>& forms) override {}
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override {}
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field,
      const std::u16string& value) override {}
  void RendererShouldClearFilledSection() override {}
  void RendererShouldClearPreviewedForm() override {}
  void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) override {}
  void RendererShouldFillFieldWithValue(const FieldGlobalId& field,
                                        const std::u16string& value) override {}
  void RendererShouldPreviewFieldWithValue(
      const FieldGlobalId& field,
      const std::u16string& value) override {}
  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field,
      const mojom::AutofillState state) override {}
  void PopupHidden() override {}
  net::IsolationInfo IsolationInfo() override { return isolation_info_; }
  void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) override {}
  void SetShouldSuppressKeyboard(bool suppress) override {}
  void TriggerFormExtraction() override {}
  void TriggerFormExtractionInAllFrames(
      base::OnceCallback<void(bool)> form_extraction_finished_callback)
      override {}
  void GetFourDigitCombinationsFromDOM(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) override {}

  // The return value contains the members (field, type) of `field_type_map` for
  // which `field_type_map_filter_.Run(triggered_origin, field, type)` is true.
  std::vector<FieldGlobalId> FillOrPreviewForm(
      mojom::RendererFormDataAction action,
      const FormData& form_data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
      override {
    std::vector<FieldGlobalId> result;
    for (const auto& [id, type] : field_type_map) {
      if (!field_type_map_filter_ ||
          field_type_map_filter_.Run(triggered_origin, id, type)) {
        result.push_back(id);
      }
    }
    return result;
  }

  // Methods unique to TestAutofillDriver that tests can use to specialize
  // functionality.

  void SetIsInActiveFrame(bool is_in_active_frame) {
    is_in_active_frame_ = is_in_active_frame;
  }

  void SetIsInAnyMainFrame(bool is_in_any_main_frame) {
    is_in_any_main_frame_ = is_in_any_main_frame;
  }

  void SetIsolationInfo(const net::IsolationInfo& isolation_info) {
    isolation_info_ = isolation_info;
  }

  // The filter that determines the return value of FillOrPreviewForm().
  void SetFieldTypeMapFilter(
      base::RepeatingCallback<
          bool(const url::Origin&, FieldGlobalId, ServerFieldType)> callback) {
    field_type_map_filter_ = callback;
  }

  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

#if !BUILDFLAG(IS_IOS)
  void SetAuthenticator(webauthn::InternalAuthenticator* authenticator_) {
    test_authenticator_.reset(authenticator_);
  }
#endif

 private:
  bool is_in_active_frame_ = true;
  bool is_in_any_main_frame_ = true;
  net::IsolationInfo isolation_info_;
  base::RepeatingCallback<
      bool(const url::Origin&, FieldGlobalId, ServerFieldType)>
      field_type_map_filter_;

#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<webauthn::InternalAuthenticator> test_authenticator_;
#endif
};

// A simple `AutofillDriver` for tests. Consider `TestContentAutofillDriver` as
// an alternative for tests where the content layer is visible.
//
// Consider using TestAutofillDriverInjector in browser tests.
class TestAutofillDriver : public TestAutofillDriverTemplate<AutofillDriver> {
 public:
  TestAutofillDriver();
  ~TestAutofillDriver() override;

  void set_autofill_manager(std::unique_ptr<AutofillManager> autofill_manager) {
    autofill_manager_ = std::move(autofill_manager);
  }

  AutofillManager* autofill_manager() { return autofill_manager_.get(); }

 private:
  std::unique_ptr<AutofillManager> autofill_manager_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
