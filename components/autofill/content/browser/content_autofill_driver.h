// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/key_press_handler_manager.h"
#include "components/autofill/content/browser/webauthn/internal_authenticator_impl.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace autofill {

class AutofillClient;
class AutofillProvider;
class LogManager;

// Use <Phone><WebOTP><OTC> as the bit pattern to identify the metrics state.
enum class PhoneCollectionMetricState {
  kNone = 0,    // Site did not collect phone, not use OTC, not use WebOTP
  kOTC = 1,     // Site used OTC only
  kWebOTP = 2,  // Site used WebOTP only
  kWebOTPPlusOTC = 3,  // Site used WebOTP and OTC
  kPhone = 4,          // Site collected phone, not used neither WebOTP nor OTC
  kPhonePlusOTC = 5,   // Site collected phone number and used OTC
  kPhonePlusWebOTP = 6,         // Site collected phone number and used WebOTP
  kPhonePlusWebOTPPlusOTC = 7,  // Site collected phone number and used both
  kMaxValue = kPhonePlusWebOTPPlusOTC,
};

namespace phone_collection_metric {
constexpr uint32_t kOTCUsed = 1 << 0;
constexpr uint32_t kWebOTPUsed = 1 << 1;
constexpr uint32_t kPhoneCollected = 1 << 2;
}  // namespace phone_collection_metric

// Class that drives autofill flow in the browser process based on
// communication from the renderer and from the external world. There is one
// instance per RenderFrameHost.
class ContentAutofillDriver : public AutofillDriver,
                              public mojom::AutofillDriver,
                              public KeyPressHandlerManager::Delegate {
 public:
  ContentAutofillDriver(
      content::RenderFrameHost* render_frame_host,
      AutofillClient* client,
      const std::string& app_locale,
      AutofillManager::AutofillDownloadManagerState enable_download_manager,
      AutofillProvider* provider);
  ~ContentAutofillDriver() override;

  // Gets the driver for |render_frame_host|.
  static ContentAutofillDriver* GetForRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver);

  // AutofillDriver:
  bool IsIncognito() const override;
  bool IsInMainFrame() const override;
  bool CanShowAutofillUi() const override;
  ui::AXTreeID GetAxTreeId() const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool RendererIsAvailable() override;
  InternalAuthenticator* GetOrCreateCreditCardInternalAuthenticator() override;
  void SendFormDataToRenderer(int query_id,
                              RendererFormDataAction action,
                              const FormData& data) override;
  void PropagateAutofillPredictions(
      const std::vector<autofill::FormStructure*>& forms) override;
  void HandleParsedForms(const std::vector<const FormData*>& forms) override;
  void SendAutofillTypePredictionsToRenderer(
      const std::vector<FormStructure*>& forms) override;
  void RendererShouldAcceptDataListSuggestion(
      const base::string16& value) override;
  void RendererShouldClearFilledSection() override;
  void RendererShouldClearPreviewedForm() override;
  void RendererShouldFillFieldWithValue(const base::string16& value) override;
  void RendererShouldPreviewFieldWithValue(
      const base::string16& value) override;
  void RendererShouldSetSuggestionAvailability(
      const mojom::AutofillState state) override;
  void PopupHidden() override;
  gfx::RectF TransformBoundingBoxToViewportCoordinates(
      const gfx::RectF& bounding_box) override;
  net::IsolationInfo IsolationInfo() override;

  // mojom::AutofillDriver:
  void SetFormToBeProbablySubmitted(
      const base::Optional<FormData>& form) override;
  void FormsSeen(const std::vector<FormData>& forms,
                 base::TimeTicks timestamp) override;
  void FormSubmitted(const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource source) override;
  void TextFieldDidChange(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box,
                          base::TimeTicks timestamp) override;
  void TextFieldDidScroll(const FormData& form,
                          const FormFieldData& field,
                          const gfx::RectF& bounding_box) override;
  void SelectControlDidChange(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;
  void QueryFormFieldAutofill(int32_t id,
                              const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box,
                              bool autoselect_first_suggestion) override;
  void HidePopup() override;
  void FocusNoLongerOnForm(bool had_interacted_form) override;
  void FocusOnFormField(const FormData& form,
                        const FormFieldData& field,
                        const gfx::RectF& bounding_box) override;
  void DidFillAutofillFormData(const FormData& form,
                               base::TimeTicks timestamp) override;
  void DidPreviewAutofillFormData() override;
  void DidEndTextFieldEditing() override;
  void SelectFieldOptionsDidChange(const FormData& form) override;

  void ProbablyFormSubmitted();

  // DidNavigateFrame() is called on the frame's driver, respectively, when a
  // navigation occurs in that specific frame.
  void DidNavigateFrame(content::NavigationHandle* navigation_handle);

  AutofillManager* autofill_manager() { return autofill_manager_; }
  AutofillHandler* autofill_handler() { return autofill_handler_.get(); }
  content::RenderFrameHost* render_frame_host() { return render_frame_host_; }

  const mojo::AssociatedRemote<mojom::AutofillAgent>& GetAutofillAgent();

  // Methods forwarded to key_press_handler_manager_.
  void RegisterKeyPressHandler(
      const content::RenderWidgetHost::KeyPressEventCallback& handler);
  void RemoveKeyPressHandler();

  void SetAutofillProviderForTesting(AutofillProvider* provider);

  // Sets the manager to |manager| and sets |manager|'s external delegate
  // to |autofill_external_delegate_|. Takes ownership of |manager|.
  void SetAutofillManager(std::unique_ptr<AutofillManager> manager);

  // Reports whether a document collects phone numbers, uses one time code, uses
  // WebOTP. There are cases that the reporting is not expected:
  //   1. some unit tests do not set necessary members, |autofill_manager_|
  //   2. there is no form and WebOTP is not used
  // |MaybeReportAutofillWebOTPMetrics| is to exclude the cases above.
  // |ReportAutofillWebOTPMetrics| is visible for unit tests where the
  // |render_frame_host_| is not set.
  void MaybeReportAutofillWebOTPMetrics();
  void ReportAutofillWebOTPMetrics(bool document_used_webotp);

 protected:
  // Constructor for tests.
  ContentAutofillDriver();

 private:
  // KeyPressHandlerManager::Delegate:
  void AddHandler(
      const content::RenderWidgetHost::KeyPressEventCallback& handler) override;
  void RemoveHandler(
      const content::RenderWidgetHost::KeyPressEventCallback& handler) override;

  void SetAutofillProvider(AutofillProvider* provider);

  // Returns whether navigator.credentials.get({otp: {transport:"sms"}}) has
  // been used.
  bool DocumentUsedWebOTP() const;

  // Weak ref to the RenderFrameHost the driver is associated with. Should
  // always be non-NULL and valid for lifetime of |this|.
  content::RenderFrameHost* const render_frame_host_;

  // The form pushed from the AutofillAgent to the AutofillDriver. When the
  // ProbablyFormSubmitted() event is fired, this form is considered the
  // submitted one.
  base::Optional<FormData> potentially_submitted_form_;

  // Keeps track of the forms for which FormSubmitted() event has been triggered
  // to avoid duplicates fired by AutofillAgent.
  std::set<FormRendererId> submitted_forms_;

  // AutofillHandler instance via which this object drives the shared Autofill
  // code.
  std::unique_ptr<AutofillHandler> autofill_handler_;

  // The pointer to autofill_handler_ if it is AutofillManager instance.
  // TODO: unify autofill_handler_ and autofill_manager_ to a single pointer to
  // a common root.
  AutofillManager* autofill_manager_;

  // Pointer to an implementation of InternalAuthenticator.
  std::unique_ptr<InternalAuthenticator> authenticator_impl_;

  // AutofillExternalDelegate instance that this object instantiates in the
  // case where the Autofill native UI is enabled.
  std::unique_ptr<AutofillExternalDelegate> autofill_external_delegate_;

  KeyPressHandlerManager key_press_handler_manager_;

  LogManager* const log_manager_;

  mojo::AssociatedReceiver<mojom::AutofillDriver> receiver_{this};

  mojo::AssociatedRemote<mojom::AutofillAgent> autofill_agent_;

  // Helps with measuring whether phone number is collected and whether it is in
  // conjunction with WebOTP or OneTimeCode (OTC).
  // value="0" label="Phone Not Collected, WebOTP Not Used, OTC Not Used"
  // value="1" label="Phone Not Collected, WebOTP Not Used, OTC Used"
  // value="2" label="Phone Not Collected, WebOTP Used, OTC Not Used"
  // value="3" label="Phone Not Collected, WebOTP Used, OTC Used"
  // value="4" label="Phone Collected, WebOTP Not Used, OTC Not Used"
  // value="5" label="Phone Collected, WebOTP Not Used, OTC Used"
  // value="6" label="Phone Collected, WebOTP Used, OTC Not Used"
  // value="7" label="Phone Collected, WebOTP Used, OTC Used"
  uint32_t phone_collection_metric_state_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_AUTOFILL_DRIVER_H_
