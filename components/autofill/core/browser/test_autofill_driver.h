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
#include "url/origin.h"

#if !BUILDFLAG(IS_IOS)
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif

namespace autofill {

// This class is only for easier writing of tests.
#if BUILDFLAG(IS_IOS)
class TestAutofillDriver : public AutofillDriver {
#else
class TestAutofillDriver : public ContentAutofillDriver {
#endif
 public:
  TestAutofillDriver();
  TestAutofillDriver(const TestAutofillDriver&) = delete;
  TestAutofillDriver& operator=(const TestAutofillDriver&) = delete;
  ~TestAutofillDriver() override;

#if BUILDFLAG(IS_IOS)
  void set_autofill_manager(std::unique_ptr<AutofillManager> autofill_manager) {
    autofill_manager_ = std::move(autofill_manager);
  }

  AutofillManager* autofill_manager() { return autofill_manager_.get(); }
#endif

  // AutofillDriver implementation overrides.
  bool IsInActiveFrame() const override;
  bool IsInAnyMainFrame() const override;
  bool IsPrerendering() const override;
  bool CanShowAutofillUi() const override;
  ui::AXTreeID GetAxTreeId() const override;
  bool RendererIsAvailable() override;
  // The return value contains the members (field, type) of `field_type_map` for
  // which `field_type_filter_.Run(triggered_origin, field, type)` is true.
  std::vector<FieldGlobalId> FillOrPreviewForm(
      mojom::RendererFormDataAction action,
      const FormData& data,
      const url::Origin& triggered_origin,
      const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map)
      override;
  void HandleParsedForms(const std::vector<FormData>& forms) override {}
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override {}
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field,
      const std::u16string& value) override {}
  void RendererShouldClearFilledSection() override {}
  void RendererShouldClearPreviewedForm() override {}
  void RendererShouldFillFieldWithValue(const FieldGlobalId& field,
                                        const std::u16string& value) override {}
  void RendererShouldPreviewFieldWithValue(
      const FieldGlobalId& field,
      const std::u16string& value) override {}
  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field,
      const mojom::AutofillState state) override {}
  void PopupHidden() override {}
  net::IsolationInfo IsolationInfo() override;
  void SendFieldsEligibleForManualFillingToRenderer(
      const std::vector<FieldGlobalId>& fields) override {}
  void SetShouldSuppressKeyboard(bool suppress) override {}
  void TriggerReparseInAllFrames(
      base::OnceCallback<void(bool)> trigger_reparse_finished_callback)
      override {}

  // Methods unique to TestAutofillDriver that tests can use to specialize
  // functionality.

  void SetIsInActiveFrame(bool is_in_active_frame);
  void SetIsInAnyMainFrame(bool is_in_any_main_frame);
  void SetIsolationInfo(const net::IsolationInfo& isolation_info);

  // The filter that determines the return value of FillOrPreviewForm().
  void SetFieldTypeMapFilter(
      base::RepeatingCallback<
          bool(const url::Origin&, FieldGlobalId, ServerFieldType)> callback);

  void SetSharedURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
#if !BUILDFLAG(IS_IOS)
  void SetAuthenticator(webauthn::InternalAuthenticator* authenticator_);
#endif

 private:
  bool is_in_active_frame_ = true;
  bool is_in_any_main_frame_ = true;
  net::IsolationInfo isolation_info_;
  base::RepeatingCallback<
      bool(const url::Origin&, FieldGlobalId, ServerFieldType)>
      field_type_map_filter_;

#if BUILDFLAG(IS_IOS)
  std::unique_ptr<AutofillManager> autofill_manager_;
#endif

#if !BUILDFLAG(IS_IOS)
  std::unique_ptr<webauthn::InternalAuthenticator> test_authenticator_;
#endif
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DRIVER_H_
