// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_STATE_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_STATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/payments/content/initialization_task.h"
#include "components/payments/content/payment_app_factory.h"
#include "components/payments/content/payment_app_service.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_response_helper.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "components/payments/content/service_worker_payment_app_factory.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payments_profile_comparator.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "url/origin.h"

namespace autofill {
class AddressNormalizer;
class AutofillProfile;
class PersonalDataManager;
class RegionDataLoader;
}  // namespace autofill

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments {

class ContentPaymentRequestDelegate;
class CSPChecker;
class PaymentApp;

// Keeps track of the information currently selected by the user and whether the
// user is ready to pay. Uses information from the PaymentRequestSpec, which is
// what the merchant has specified, as input into the "is ready to pay"
// computation.
//
// The initialization state is observed by PaymentRequestDialogView for showing
// a "Loading..." spinner.
class PaymentRequestState : public PaymentAppFactory::Delegate,
                            public PaymentResponseHelper::Delegate,
                            public PaymentRequestSpec::Observer,
                            public InitializationTask {
 public:
  // Any class call add itself as Observer via AddObserver() and receive
  // notification about the state changing.
  class Observer {
   public:
    // Called when finished getting all available payment apps.
    virtual void OnGetAllPaymentAppsFinished() = 0;

    // Called when the information (payment method, address/contact info,
    // shipping option) changes.
    virtual void OnSelectedInformationChanged() = 0;

   protected:
    virtual ~Observer() = default;
  };

  class Delegate {
   public:
    // Called when the PaymentResponse is available.
    virtual void OnPaymentResponseAvailable(
        mojom::PaymentResponsePtr response) = 0;

    // Called when the invoked payment app failed.
    virtual void OnPaymentResponseError(const std::string& error_message) = 0;

    // Called when the shipping option has changed to |shipping_option_id|.
    virtual void OnShippingOptionIdSelected(std::string shipping_option_id) = 0;

    // Called when the shipping address has changed to |address|.
    virtual void OnShippingAddressSelected(
        mojom::PaymentAddressPtr address) = 0;

    virtual void OnPayerInfoSelected(mojom::PayerDetailPtr payer_info) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  using StatusCallback = base::OnceCallback<void(bool)>;
  using MethodsSupportedCallback =
      base::OnceCallback<void(bool methods_supported,
                              const std::string& error_message,
                              AppCreationFailureReason error_reason)>;

  PaymentRequestState(
      std::unique_ptr<PaymentAppService> payment_app_service,
      content::RenderFrameHost* initiator_render_frame_host,
      const GURL& top_level_origin,
      const GURL& frame_origin,
      const url::Origin& frame_security_origin,
      base::WeakPtr<PaymentRequestSpec> spec,
      base::WeakPtr<Delegate> delegate,
      const std::string& app_locale,
      autofill::PersonalDataManager* personal_data_manager,
      base::WeakPtr<ContentPaymentRequestDelegate> payment_request_delegate,
      base::WeakPtr<JourneyLogger> journey_logger,
      base::WeakPtr<CSPChecker> csp_checker);

  PaymentRequestState(const PaymentRequestState&) = delete;
  PaymentRequestState& operator=(const PaymentRequestState&) = delete;

  ~PaymentRequestState() override;

  // PaymentAppFactory::Delegate
  content::WebContents* GetWebContents() override;
  base::WeakPtr<ContentPaymentRequestDelegate> GetPaymentRequestDelegate()
      const override;
  void ShowProcessingSpinner() override;
  base::WeakPtr<PaymentRequestSpec> GetSpec() const override;
  void GetTwaPackageName(GetTwaPackageNameCallback callback) override;
  const GURL& GetTopOrigin() override;
  const GURL& GetFrameOrigin() override;
  const url::Origin& GetFrameSecurityOrigin() override;
  content::RenderFrameHost* GetInitiatorRenderFrameHost() const override;
  content::GlobalRenderFrameHostId GetInitiatorRenderFrameHostId()
      const override;
  const std::vector<mojom::PaymentMethodDataPtr>& GetMethodData()
      const override;
  std::unique_ptr<webauthn::InternalAuthenticator> CreateInternalAuthenticator()
      const override;
  scoped_refptr<PaymentManifestWebDataService>
  GetPaymentManifestWebDataService() const override;
  bool IsOffTheRecord() const override;
  void OnPaymentAppCreated(std::unique_ptr<PaymentApp> app) override;
  void OnPaymentAppCreationError(
      const std::string& error_message,
      AppCreationFailureReason reason =
          AppCreationFailureReason::UNKNOWN) override;
  void OnDoneCreatingPaymentApps() override;
  void SetCanMakePaymentEvenWithoutApps() override;
  base::WeakPtr<CSPChecker> GetCSPChecker() override;
  void SetOptOutOffered() override;
  std::optional<base::UnguessableToken> GetChromeOSTWAInstanceId()
      const override;

  // PaymentResponseHelper::Delegate
  void OnPaymentResponseReady(
      mojom::PaymentResponsePtr payment_response) override;
  void OnPaymentResponseError(const std::string& error_message) override;

  // PaymentRequestSpec::Observer
  void OnStartUpdating(PaymentRequestSpec::UpdateReason reason) override {}
  void OnSpecUpdated() override;

  // Checks whether support for the specified payment methods exist, either
  // because the user has a registered payment handler or because the browser
  // can do just-in-time registration for a suitable payment handler.
  void CanMakePayment(StatusCallback callback);

  // Checks whether the user has at least one instrument that satisfies the
  // specified supported payment methods asynchronously.
  void HasEnrolledInstrument(StatusCallback callback);

  // Checks if the payment methods that the merchant website have
  // requested are supported asynchronously. For example, may return true for
  // "basic-card", but false for "https://bobpay.com".
  void AreRequestedMethodsSupported(MethodsSupportedCallback callback);

  // Resets pending MethodsSupportedCallback after abort.
  void OnAbort();

  // Returns authenticated user email, or empty string.
  std::string GetAuthenticatedEmail() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Initiates the generation of the PaymentResponse. Callers should check
  // |is_ready_to_pay|, which is inexpensive.
  void GeneratePaymentResponse();

  // Cancels the generation of the PaymentResponse.
  void OnPaymentAppWindowClosed();

  // Record the use of the data models that were used in the Payment Request.
  void RecordUseStats();

  // Sets selected app as the only available app for retry.
  void SetAvailablePaymentAppForRetry();

  // Gets the Autofill Profile representing the shipping address or contact
  // information currently selected for this PaymentRequest flow. Can return
  // null.
  autofill::AutofillProfile* selected_shipping_profile() const {
    return selected_shipping_profile_;
  }
  // If |spec()->selected_shipping_option_error()| is not empty, this contains
  // the profile for which the error is about.
  autofill::AutofillProfile* selected_shipping_option_error_profile() const {
    return selected_shipping_option_error_profile_;
  }
  autofill::AutofillProfile* selected_contact_profile() const {
    return selected_contact_profile_;
  }
  autofill::AutofillProfile* invalid_shipping_profile() const {
    return invalid_shipping_profile_;
  }
  autofill::AutofillProfile* invalid_contact_profile() const {
    return invalid_contact_profile_;
  }
  // Returns the currently selected app for this PaymentRequest flow. It's not
  // guaranteed to be complete. Returns nullptr if there is no selected app.
  PaymentApp* selected_app() const { return selected_app_.get(); }

  // Returns the appropriate Autofill Profiles for this user. The profiles
  // returned are owned by the PaymentRequestState.
  const std::vector<raw_ptr<autofill::AutofillProfile, VectorExperimental>>&
  shipping_profiles() {
    return shipping_profiles_;
  }
  const std::vector<raw_ptr<autofill::AutofillProfile, VectorExperimental>>&
  contact_profiles() {
    return contact_profiles_;
  }
  const std::vector<std::unique_ptr<PaymentApp>>& available_apps() {
    return available_apps_;
  }

  // Creates and adds an AutofillProfile as a shipping profile, which makes a
  // copy of |profile|. |selected| indicates if the newly-created shipping
  // profile should be selected, after which observers will be notified.
  void AddAutofillShippingProfile(bool selected,
                                  const autofill::AutofillProfile& profile);

  // Creates and adds an AutofillProfile as a contact profile, which makes a
  // copy of |profile|. |selected| indicates if the newly-created shipping
  // profile should be selected, after which observers will be notified.
  void AddAutofillContactProfile(bool selected,
                                 const autofill::AutofillProfile& profile);

  // Setters to change the selected information. Will have the side effect of
  // recomputing "is ready to pay" and notify observers.
  void SetSelectedShippingOption(const std::string& shipping_option_id);
  void SetSelectedShippingProfile(autofill::AutofillProfile* profile);
  void SetSelectedContactProfile(autofill::AutofillProfile* profile);
  void SetSelectedApp(base::WeakPtr<PaymentApp> app);

  bool is_ready_to_pay() { return is_ready_to_pay_; }

  // Checks whether getting all available apps is finished.
  bool is_get_all_apps_finished() { return get_all_apps_finished_; }

  // Returns true after is_get_all_apps_finished() is true and supported payment
  // method are found. Should not be called before is_get_all_apps_finished() is
  // true.
  bool are_requested_methods_supported() const {
    return are_requested_methods_supported_;
  }

  bool is_retry_called() const { return is_retry_called_; }

  const std::string& GetApplicationLocale();
  autofill::PersonalDataManager* GetPersonalDataManager();
  autofill::RegionDataLoader* GetRegionDataLoader();

  PaymentsProfileComparator* profile_comparator() {
    return &profile_comparator_;
  }

  // Returns true if the payment app has been invoked and the payment response
  // generation has begun. False otherwise.
  bool IsPaymentAppInvoked() const;

  autofill::AddressNormalizer* GetAddressNormalizer();

  // InitializationTask:
  bool IsInitialized() const override;

  // Selects the default shipping address.
  void SelectDefaultShippingAddressAndNotifyObservers();

  // Returns true when shipping address is required and the selected app (if
  // any) does not support shipping address delegation.
  bool ShouldShowShippingSection() const;

  // Returns true when payer name/phone/email is required and the selected app
  // (if any) does not support required contact info delegation.
  bool ShouldShowContactSection() const;

  base::WeakPtr<PaymentRequestState> AsWeakPtr();

 private:
  // Fetches the Autofill Profiles for this user from the PersonalDataManager,
  // and stores copies of them, owned by this PaymentRequestState, in
  // profile_cache_.
  void PopulateProfileCache();

  // Sets the initial selections for apps and profiles, and notifies observers.
  void SetDefaultProfileSelections();

  // Uses the user-selected information as well as the merchant spec to update
  // |is_ready_to_pay_| with the current state, by validating that all the
  // required information is available. Will notify observers.
  void UpdateIsReadyToPayAndNotifyObservers();

  // Notifies all observers that getting all payment apps is finished.
  void NotifyOnGetAllPaymentAppsFinished();

  // Notifies all observers that selected information has changed.
  void NotifyOnSelectedInformationChanged();

  // Returns whether the selected data satisfies the PaymentDetails requirements
  // (payment methods).
  bool ArePaymentDetailsSatisfied();

  // Returns whether the selected data satisfies the PaymentOptions requirements
  // (contact info, shipping address).
  bool ArePaymentOptionsSatisfied();

  // Checks if the payment methods that the merchant website have
  // requested are supported and call the |callback| to return the result.
  void CheckRequestedMethodsSupported(MethodsSupportedCallback callback);

  void OnAddressNormalized(bool success,
                           const autofill::AutofillProfile& normalized_profile);

  void OnGetTwaPackageName(GetTwaPackageNameCallback callback,
                           const std::string& twa_package_name);

  // Returns whether the browser is currently in a TWA.
  bool IsInTwa() const;

  bool GetCanMakePaymentValue() const;
  bool GetHasEnrolledInstrumentValue() const;

  const std::unique_ptr<PaymentAppService> payment_app_service_;

  content::GlobalRenderFrameHostId frame_routing_id_;
  const GURL top_origin_;
  const GURL frame_origin_;
  const url::Origin frame_security_origin_;
  size_t number_of_payment_app_factories_ = 0;

  // True when the requested autofill data (shipping address and/or contact
  // information) and payment data (either autofill or service worker) are
  // complete, valid, and selected.
  bool is_ready_to_pay_ = false;

  // Whether getting all available apps is finished.
  bool get_all_apps_finished_ = false;

  // The Android package name of the Trusted Web Activity that invoked this
  // browser, if any.
  std::string twa_package_name_;

  // The value returned by hasEnrolledInstrument(). Can be used only after
  // |get_all_apps_finished_| is true.
  bool has_enrolled_instrument_ = false;

  // Whether the data is currently being validated by the merchant.
  bool is_waiting_for_merchant_validation_ = false;

  // Whether retry() has been called by the merchant.
  bool is_retry_called_ = false;

  const std::string app_locale_;

  // These WeakPtrs can be null when the webpage closes or the iframe refreshes
  // or navigates.
  base::WeakPtr<PaymentRequestSpec> spec_;
  base::WeakPtr<Delegate> delegate_;
  base::WeakPtr<JourneyLogger> journey_logger_;
  base::WeakPtr<CSPChecker> csp_checker_;

  // Not owned. Never null. Must outlive this object.
  raw_ptr<autofill::PersonalDataManager, DanglingUntriaged>
      personal_data_manager_;

  StatusCallback can_make_payment_callback_;
  StatusCallback has_enrolled_instrument_callback_;
  MethodsSupportedCallback are_requested_methods_supported_callback_;
  bool are_requested_methods_supported_ = false;
  std::string get_all_payment_apps_error_;
  AppCreationFailureReason get_all_payment_apps_error_reason_ =
      AppCreationFailureReason::UNKNOWN;

  raw_ptr<autofill::AutofillProfile, DanglingUntriaged>
      selected_shipping_profile_ = nullptr;
  raw_ptr<autofill::AutofillProfile, DanglingUntriaged>
      selected_shipping_option_error_profile_ = nullptr;
  raw_ptr<autofill::AutofillProfile, DanglingUntriaged>
      selected_contact_profile_ = nullptr;
  raw_ptr<autofill::AutofillProfile, DanglingUntriaged>
      invalid_shipping_profile_ = nullptr;
  raw_ptr<autofill::AutofillProfile, DanglingUntriaged>
      invalid_contact_profile_ = nullptr;
  base::WeakPtr<PaymentApp> selected_app_;

  // Profiles may change due to (e.g.) sync events, so profiles are cached after
  // loading and owned here. They are populated once only, and ordered by
  // frecency.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profile_cache_;
  std::vector<raw_ptr<autofill::AutofillProfile, VectorExperimental>>
      shipping_profiles_;
  std::vector<raw_ptr<autofill::AutofillProfile, VectorExperimental>>
      contact_profiles_;

  std::vector<std::unique_ptr<PaymentApp>> available_apps_;

  base::WeakPtr<ContentPaymentRequestDelegate> payment_request_delegate_;

  std::unique_ptr<PaymentResponseHelper> response_helper_;

  PaymentsProfileComparator profile_comparator_;

  base::ObserverList<Observer>::Unchecked observers_;

  // If set to true, then both GetCanMakePaymentValue() and
  // GetHasEnrolledInstrumentValue() will return true, regardless of presence of
  // payment apps. This is used by secure payment confirmation, where
  // PaymentRequest.canMakePayment() and PaymentRequesthasEnrolledInstrument()
  // calls in JavaScript both return true without querying the SQLite database
  // for instrument information and without querying the authenticator for
  // credentials.
  bool can_make_payment_even_without_apps_ = false;

  base::WeakPtrFactory<PaymentRequestState> weak_ptr_factory_{this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_STATE_H_
