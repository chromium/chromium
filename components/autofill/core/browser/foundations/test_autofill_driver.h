// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_H_

#include <algorithm>
#include <concepts>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/accessibility/ax_tree_id.h"
#include "url/origin.h"

namespace autofill::mojom {
class AutofillVisibilityObserver;
}  // namespace autofill::mojom

namespace autofill {

// This class is only for easier writing of tests. There are two instances of
// the template:
//
// - TestAutofillDriver is a simple AutofillDriver;
// - TestContentAutofillDriver is a ContentAutofillDriver, i.e., is associated
//   with a RenderFrameHost and owned by a ContentAutofillDriverFactory.
//
// As a rule of thumb, TestContentAutofillDriver is preferable in tests that
// have a content::WebContents.
template <std::derived_from<AutofillDriver> T>
class TestAutofillDriverTemplate : public T {
 public:
  using T::T;
  TestAutofillDriverTemplate(const TestAutofillDriverTemplate&) = delete;
  TestAutofillDriverTemplate& operator=(const TestAutofillDriverTemplate&) =
      delete;
  ~TestAutofillDriverTemplate() override = default;

  // AutofillDriver:
  void ApplyFieldAction(mojom::FieldActionType action_type,
                        mojom::ActionPersistence action_persistence,
                        const FieldGlobalId& field,
                        const std::u16string& value) override {}
  void SendTypePredictionsToRenderer(const FormStructure& form) override {}
  void ExposeDomNodeIdsInAllFrames() override {}
  void RendererShouldAcceptDataListSuggestion(
      const FieldGlobalId& field,
      const std::u16string& value) override {}
  void RendererShouldClearPreviewedForm() override {}
  void RendererShouldTriggerSuggestions(
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source) override {}
  void RendererShouldSetSuggestionAvailability(
      const FieldGlobalId& field,
      mojom::AutofillSuggestionAvailability suggestion_availability) override {}
  void TriggerFormExtractionInDriverFrame(
      AutofillDriver::AutofillDriverRouterAndFormForestPassKey pass_key,
      bool force) override {}
  void TriggerFormExtractionInAllFrames(
      bool force,
      base::OnceCallback<void(bool)> form_extraction_finished_callback)
      override {}
  void ExtractFormWithField(
      FieldGlobalId field_id,
      AutofillDriver::BrowserFormHandler response_handler) override {}
  void ObserveFieldVisibility(
      const FieldGlobalId& field_id,
      mojo::PendingRemote<mojom::AutofillVisibilityObserver> observer)
      override {}
  void GetFourDigitCombinationsFromDom(
      base::OnceCallback<void(const std::vector<std::string>&)>
          potential_matches) override {}
  void ExtractLabeledTextNodeValue(
      const std::u16string& value_regex,
      const std::u16string& label_regex,
      uint32_t number_of_ancestor_levels_to_search,
      base::OnceCallback<void(const std::string& amount)> response_callback)
      override {}
  void GetNonceForEmailVerification(
      FieldGlobalId email_field_id,
      base::OnceCallback<void(const std::optional<std::string>&)> callback)
      override {}
  void SendEmailVerificationToken(FieldGlobalId email_field_id,
                                  const std::string& email,
                                  const std::string& token) override {}
  void ScrollFieldIntoView(FieldGlobalId field_id) override {}
  void LoadCustomFormControlScript(
      const std::string& script,
      base::OnceCallback<void(bool success)> callback) override {}

  base::flat_set<FieldGlobalId> ApplyFormAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData> fields,
      const FillId& fill_id,
      bool supports_refill,
      const url::Origin& triggered_origin,
      const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map)
      override {
    return base::ToVector(fields, &FormFieldData::global_id);
  }
};

// A simple `AutofillDriver` for tests. Consider `TestContentAutofillDriver` as
// an alternative for tests where the content layer is visible.
//
// Consider using TestAutofillDriverInjector in browser tests.
class TestAutofillDriver : public TestAutofillDriverTemplate<AutofillDriver> {
 public:
  explicit TestAutofillDriver(TestAutofillClient* client);
  ~TestAutofillDriver() override;

  // AutofillDriver:
  LocalFrameToken GetFrameToken() const override;
  std::optional<LocalFrameToken> Resolve(FrameToken query) override;
  TestAutofillDriver* GetParent() override;
  bool IsActive() const override;
  bool IsEmbedded() const override;
  TestAutofillClient& GetAutofillClient() override;
  AutofillManager& GetAutofillManager() override;
  ukm::SourceId GetPageUkmSourceId() const override;
  ukm::SourceId GetPageUkmSourceId();
  bool IsPolicyControlledFeatureAutofillEnabled() const override;
  bool IsPolicyControlledFeatureManualTextEnabled() const override;
  bool CanShowAutofillUi() const override;
  std::optional<net::IsolationInfo> GetIsolationInfo() override;
  void TriggerFormExtractionInAllFrames(
      bool force,
      base::OnceCallback<void(bool)> form_extraction_finished_callback)
      override;
  bool IsSafeToFill(const FormFieldData& field,
                    FieldType filled_type,
                    const url::Origin& main_origin,
                    const url::Origin& trigger_origin) const override;

  void set_autofill_manager(std::unique_ptr<AutofillManager> autofill_manager) {
    autofill_manager_ = std::move(autofill_manager);
  }

  void set_frame_token(LocalFrameToken frame_token) {
    frame_token_ = frame_token;
  }

  void SetRemoteFrameToken(RemoteFrameToken remote_frame_token,
                           LocalFrameToken local_frame_token) {
    remote_frame_tokens_[remote_frame_token] = local_frame_token;
  }

  void SetParent(TestAutofillDriver* parent) { parent_ = parent; }

  void SetIsActive(bool is_active) { is_active_ = is_active; }

  void SetIsEmbedded(bool is_embedded) { is_embedded_ = is_embedded; }

  void SetPolicyControlledFeatureAutofillEnabled(bool enabled) {
    policy_controlled_feature_autofill_enabled_ = enabled;
  }

  void SetPolicyControlledFeatureManualTextEnabled(bool enabled) {
    policy_controlled_feature_manual_text_enabled_ = enabled;
  }

  void SetIsolationInfo(const net::IsolationInfo& isolation_info) {
    isolation_info_ = isolation_info;
  }

  // Initializes UKM source from `url()`. This needs to be called in unittests
  // after calling Purge for ukm recorder to re-initialize sources.
  void InitializeUKMSources();

  // The faked URL of the driver's frame.
  const GURL& url() { return url_; }
  void set_url(GURL url) { url_ = std::move(url); }

 private:
  LocalFrameToken frame_token_;
  std::map<RemoteFrameToken, LocalFrameToken> remote_frame_tokens_;
  raw_ptr<TestAutofillDriver> parent_ = nullptr;
  bool is_active_ = true;
  bool is_embedded_ = false;
  bool policy_controlled_feature_autofill_enabled_ = false;
  bool policy_controlled_feature_manual_text_enabled_ = false;
  net::IsolationInfo isolation_info_;
  raw_ref<TestAutofillClient> autofill_client_;
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;
  GURL url_{"https://example.test"};
  std::unique_ptr<AutofillManager> autofill_manager_ = nullptr;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_AUTOFILL_DRIVER_H_
