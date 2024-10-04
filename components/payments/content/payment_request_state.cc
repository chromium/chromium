// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_state.h"

#include <set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/payments/content/content_payment_request_delegate.h"
#include "components/payments/content/payment_app.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_response_helper.h"
#include "components/payments/content/service_worker_payment_app.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_experimental_features.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"

namespace payments {
namespace {

// Invokes the |callback| with |status|.
void CallStatusCallback(PaymentRequestState::StatusCallback callback,
                        bool status) {
  std::move(callback).Run(status);
}

// Posts the |callback| to be invoked with |status| asynchronously.
void PostStatusCallback(PaymentRequestState::StatusCallback callback,
                        bool status) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&CallStatusCallback, std::move(callback), status));
}

}  // namespace

PaymentRequestState::PaymentRequestState(
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
    base::WeakPtr<CSPChecker> csp_checker)
    : payment_app_service_(std::move(payment_app_service)),
      frame_routing_id_(initiator_render_frame_host->GetGlobalId()),
      top_origin_(top_level_origin),
      frame_origin_(frame_origin),
      frame_security_origin_(frame_security_origin),
      app_locale_(app_locale),
      spec_(spec),
      delegate_(delegate),
      journey_logger_(journey_logger),
      csp_checker_(csp_checker),
      personal_data_manager_(personal_data_manager),
      payment_request_delegate_(payment_request_delegate),
      profile_comparator_(app_locale, *spec) {
  PopulateProfileCache();

  number_of_payment_app_factories_ =
      payment_app_service_->GetNumberOfFactories();
  payment_app_service_->Create(weak_ptr_factory_.GetWeakPtr());

  spec_->AddObserver(this);
}

PaymentRequestState::~PaymentRequestState() = default;

content::WebContents* PaymentRequestState::GetWebContents() {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  return rfh && rfh->IsActive() ? content::WebContents::FromRenderFrameHost(rfh)
                                : nullptr;
}

base::WeakPtr<ContentPaymentRequestDelegate>
PaymentRequestState::GetPaymentRequestDelegate() const {
  return payment_request_delegate_;
}

void PaymentRequestState::ShowProcessingSpinner() {
  GetPaymentRequestDelegate()->ShowProcessingSpinner();
}

base::WeakPtr<PaymentRequestSpec> PaymentRequestState::GetSpec() const {
  return spec_;
}

void PaymentRequestState::GetTwaPackageName(
    GetTwaPackageNameCallback callback) {
  GetPaymentRequestDelegate()->GetTwaPackageName(
      base::BindOnce(&PaymentRequestState::OnGetTwaPackageName,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

const GURL& PaymentRequestState::GetTopOrigin() {
  return top_origin_;
}

const GURL& PaymentRequestState::GetFrameOrigin() {
  return frame_origin_;
}

const url::Origin& PaymentRequestState::GetFrameSecurityOrigin() {
  return frame_security_origin_;
}

content::RenderFrameHost* PaymentRequestState::GetInitiatorRenderFrameHost()
    const {
  return content::RenderFrameHost::FromID(frame_routing_id_);
}

content::GlobalRenderFrameHostId
PaymentRequestState::GetInitiatorRenderFrameHostId() const {
  return frame_routing_id_;
}

const std::vector<mojom::PaymentMethodDataPtr>&
PaymentRequestState::GetMethodData() const {
  DCHECK(GetSpec());
  return GetSpec()->method_data();
}

std::unique_ptr<webauthn::InternalAuthenticator>
PaymentRequestState::CreateInternalAuthenticator() const {
  return GetPaymentRequestDelegate()->CreateInternalAuthenticator();
}

scoped_refptr<PaymentManifestWebDataService>
PaymentRequestState::GetPaymentManifestWebDataService() const {
  return GetPaymentRequestDelegate()->GetPaymentManifestWebDataService();
}

bool PaymentRequestState::IsOffTheRecord() const {
  return GetPaymentRequestDelegate()->IsOffTheRecord();
}

void PaymentRequestState::OnPaymentAppCreated(std::unique_ptr<PaymentApp> app) {
  available_apps_.emplace_back(std::move(app));
}

void PaymentRequestState::OnPaymentAppCreationError(
    const std::string& error_message,
    AppCreationFailureReason reason) {
  get_all_payment_apps_error_ = error_message;
  get_all_payment_apps_error_reason_ = reason;
}

void PaymentRequestState::OnDoneCreatingPaymentApps() {
  DCHECK_NE(0U, number_of_payment_app_factories_);
  if (--number_of_payment_app_factories_ > 0U)
    return;

  if (IsInTwa()) {
    // If a preferred payment app is present (e.g. Play Billing within a TWA),
    // all other payment apps are ignored.
    bool has_preferred_app = base::ranges::any_of(
        available_apps_, [](const auto& app) { return app->IsPreferred(); });
    if (has_preferred_app) {
      std::erase_if(available_apps_,
                    [](const auto& app) { return !app->IsPreferred(); });

      // By design, only one payment app can be preferred.
      DCHECK_EQ(available_apps_.size(), 1u);
      if (available_apps_.size() > 1)
        available_apps_.resize(1);
    }
  }

  SetDefaultProfileSelections();

  get_all_apps_finished_ = true;
  has_enrolled_instrument_ = base::ranges::any_of(
      available_apps_,
      [](const auto& app) { return app->HasEnrolledInstrument(); });
  are_requested_methods_supported_ |= !available_apps_.empty();
  NotifyOnGetAllPaymentAppsFinished();
  NotifyInitialized();

  // Fulfill the pending CanMakePayment call.
  if (can_make_payment_callback_)
    std::move(can_make_payment_callback_).Run(GetCanMakePaymentValue());

  // Fulfill the pending HasEnrolledInstrument call.
  if (has_enrolled_instrument_callback_)
    std::move(has_enrolled_instrument_callback_)
        .Run(GetHasEnrolledInstrumentValue());

  // Fulfill the pending AreRequestedMethodsSupported call.
  if (are_requested_methods_supported_callback_)
    CheckRequestedMethodsSupported(
        std::move(are_requested_methods_supported_callback_));
}

void PaymentRequestState::SetCanMakePaymentEvenWithoutApps() {
  can_make_payment_even_without_apps_ = true;
}

base::WeakPtr<CSPChecker> PaymentRequestState::GetCSPChecker() {
  return csp_checker_;
}

void PaymentRequestState::SetOptOutOffered() {
  if (journey_logger_)
    journey_logger_->SetOptOutOffered();
}

std::optional<base::UnguessableToken>
PaymentRequestState::GetChromeOSTWAInstanceId() const {
  if (!payment_request_delegate_) {
    return std::nullopt;
  }

  return payment_request_delegate_->GetChromeOSTWAInstanceId();
}

void PaymentRequestState::OnPaymentResponseReady(
    mojom::PaymentResponsePtr payment_response) {
  if (!delegate_)
    return;

  delegate_->OnPaymentResponseAvailable(std::move(payment_response));
}

void PaymentRequestState::OnPaymentResponseError(
    const std::string& error_message) {
  if (!delegate_)
    return;

  delegate_->OnPaymentResponseError(error_message);
}

void PaymentRequestState::OnSpecUpdated() {
  if (!spec_)
    return;

  autofill::AutofillProfile* selected_shipping_profile =
      selected_shipping_profile_;
  autofill::AutofillProfile* selected_contact_profile =
      selected_contact_profile_;

  if (spec_->current_update_reason() ==
      PaymentRequestSpec::UpdateReason::RETRY) {
    if (spec_->has_shipping_address_error() && selected_shipping_profile) {
      invalid_shipping_profile_ = selected_shipping_profile;
      selected_shipping_profile_ = nullptr;
    }

    if (spec_->has_payer_error() && selected_contact_profile) {
      invalid_contact_profile_ = selected_contact_profile;
      selected_contact_profile_ = nullptr;
    }
  }

  if (spec_->selected_shipping_option_error().empty()) {
    selected_shipping_option_error_profile_ = nullptr;
  } else {
    selected_shipping_option_error_profile_ = selected_shipping_profile;
    selected_shipping_profile_ = nullptr;
    if (spec_->has_shipping_address_error() && selected_shipping_profile) {
      invalid_shipping_profile_ = selected_shipping_profile;
    }
  }

  is_waiting_for_merchant_validation_ = false;
  UpdateIsReadyToPayAndNotifyObservers();
}

void PaymentRequestState::CanMakePayment(StatusCallback callback) {
  if (!get_all_apps_finished_) {
    DCHECK(!can_make_payment_callback_);
    can_make_payment_callback_ = std::move(callback);
    return;
  }

  PostStatusCallback(std::move(callback), GetCanMakePaymentValue());
}

void PaymentRequestState::HasEnrolledInstrument(StatusCallback callback) {
  if (!get_all_apps_finished_) {
    DCHECK(!has_enrolled_instrument_callback_);
    has_enrolled_instrument_callback_ = std::move(callback);
    return;
  }

  PostStatusCallback(std::move(callback), GetHasEnrolledInstrumentValue());
}

void PaymentRequestState::AreRequestedMethodsSupported(
    MethodsSupportedCallback callback) {
  if (!get_all_apps_finished_) {
    are_requested_methods_supported_callback_ = std::move(callback);
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&PaymentRequestState::CheckRequestedMethodsSupported,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentRequestState::OnAbort() {
  // Reset supported method callback when the merchant calls abort before
  // OnDoneCreatingPaymentApps().
  if (are_requested_methods_supported_callback_)
    are_requested_methods_supported_callback_.Reset();
}

void PaymentRequestState::CheckRequestedMethodsSupported(
    MethodsSupportedCallback callback) {
  DCHECK(get_all_apps_finished_);

  if (!spec_)
    return;

  if (!are_requested_methods_supported_ &&
      get_all_payment_apps_error_.empty() &&
      base::Contains(spec_->payment_method_identifiers_set(),
                     methods::kGooglePlayBilling) &&
      !IsInTwa()) {
    get_all_payment_apps_error_ = errors::kAppStoreMethodOnlySupportedInTwa;
  }

  std::move(callback).Run(are_requested_methods_supported_,
                          get_all_payment_apps_error_,
                          get_all_payment_apps_error_reason_);
}

std::string PaymentRequestState::GetAuthenticatedEmail() const {
  return payment_request_delegate_
             ? payment_request_delegate_->GetAuthenticatedEmail()
             : std::string();
}

void PaymentRequestState::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PaymentRequestState::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PaymentRequestState::GeneratePaymentResponse() {
  DCHECK(is_ready_to_pay());

  if (!spec_)
    return;

  // Once the response is ready, will call back into OnPaymentResponseReady.
  response_helper_ = std::make_unique<PaymentResponseHelper>(
      app_locale_, spec_, selected_app_, payment_request_delegate_,
      selected_shipping_profile_, selected_contact_profile_,
      weak_ptr_factory_.GetWeakPtr());
}

void PaymentRequestState::OnPaymentAppWindowClosed() {
  if (!selected_app_)
    return;
  response_helper_.reset();
  selected_app_->OnPaymentAppWindowClosed();
}

void PaymentRequestState::RecordUseStats() {
  if (ShouldShowShippingSection()) {
    DCHECK(selected_shipping_profile_);
    personal_data_manager_->address_data_manager().RecordUseOf(
        *selected_shipping_profile_);
  }

  if (ShouldShowContactSection()) {
    DCHECK(selected_contact_profile_);

    // If the same address was used for both contact and shipping, the stats
    // should only be updated once.
    if (!ShouldShowShippingSection() || (selected_shipping_profile_->guid() !=
                                         selected_contact_profile_->guid())) {
      personal_data_manager_->address_data_manager().RecordUseOf(
          *selected_contact_profile_);
    }
  }

  if (selected_app_)
    selected_app_->RecordUse();
}

void PaymentRequestState::SetAvailablePaymentAppForRetry() {
  if (!selected_app_)
    return;

  std::erase_if(available_apps_, [this](const auto& payment_app) {
    // Remove the app if it is not selected.
    return payment_app.get() != selected_app_.get();
  });
  is_retry_called_ = true;
}

void PaymentRequestState::AddAutofillShippingProfile(
    bool selected,
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      std::make_unique<autofill::AutofillProfile>(profile));
  // TODO(tmartino): Implement deduplication rules specific to shipping
  // profiles.
  autofill::AutofillProfile* new_cached_profile = profile_cache_.back().get();
  shipping_profiles_.push_back(new_cached_profile);

  if (selected) {
    SetSelectedShippingProfile(new_cached_profile);
  }
}

void PaymentRequestState::AddAutofillContactProfile(
    bool selected,
    const autofill::AutofillProfile& profile) {
  profile_cache_.push_back(
      std::make_unique<autofill::AutofillProfile>(profile));
  autofill::AutofillProfile* new_cached_profile = profile_cache_.back().get();
  contact_profiles_.push_back(new_cached_profile);

  if (selected) {
    SetSelectedContactProfile(new_cached_profile);
  }
}

void PaymentRequestState::SetSelectedShippingOption(
    const std::string& shipping_option_id) {
  if (!spec_)
    return;

  spec_->StartWaitingForUpdateWith(
      PaymentRequestSpec::UpdateReason::SHIPPING_OPTION);
  if (delegate_) {
    // This will inform the merchant and will lead to them calling updateWith
    // with new PaymentDetails.
    delegate_->OnShippingOptionIdSelected(shipping_option_id);
  }
}

void PaymentRequestState::SetSelectedShippingProfile(
    autofill::AutofillProfile* profile) {
  if (!spec_)
    return;

  spec_->StartWaitingForUpdateWith(
      PaymentRequestSpec::UpdateReason::SHIPPING_ADDRESS);
  selected_shipping_profile_ = profile;

  // Changing the shipping address clears shipping address validation errors
  // from retry().
  invalid_shipping_profile_ = nullptr;

  // The user should not be able to click on pay until the callback from the
  // merchant.
  is_waiting_for_merchant_validation_ = true;

  if (payment_request_delegate_) {
    // Start the normalization of the shipping address.
    payment_request_delegate_->GetAddressNormalizer()->NormalizeAddressAsync(
        *selected_shipping_profile_, /*timeout_seconds=*/2,
        base::BindOnce(&PaymentRequestState::OnAddressNormalized,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void PaymentRequestState::SetSelectedContactProfile(
    autofill::AutofillProfile* profile) {
  selected_contact_profile_ = profile;

  // Changing the contact information clears contact information validation
  // errors from retry().
  invalid_contact_profile_ = nullptr;

  UpdateIsReadyToPayAndNotifyObservers();

  if (IsPaymentAppInvoked() && delegate_) {
    delegate_->OnPayerInfoSelected(
        response_helper_->GeneratePayerDetail(profile));
  }
}

void PaymentRequestState::SetSelectedApp(base::WeakPtr<PaymentApp> app) {
  selected_app_ = app;
  UpdateIsReadyToPayAndNotifyObservers();
}

const std::string& PaymentRequestState::GetApplicationLocale() {
  return app_locale_;
}

autofill::PersonalDataManager* PaymentRequestState::GetPersonalDataManager() {
  return personal_data_manager_;
}

autofill::RegionDataLoader* PaymentRequestState::GetRegionDataLoader() {
  return payment_request_delegate_
             ? payment_request_delegate_->GetRegionDataLoader()
             : nullptr;
}

bool PaymentRequestState::IsPaymentAppInvoked() const {
  return !!response_helper_;
}

autofill::AddressNormalizer* PaymentRequestState::GetAddressNormalizer() {
  return payment_request_delegate_
             ? payment_request_delegate_->GetAddressNormalizer()
             : nullptr;
}

bool PaymentRequestState::IsInitialized() const {
  return get_all_apps_finished_;
}

void PaymentRequestState::SelectDefaultShippingAddressAndNotifyObservers() {
  // Only pre-select an address if the merchant provided at least one selected
  // shipping option, and the top profile is complete. Assumes that profiles
  // have already been sorted for completeness and frecency.
  if (!shipping_profiles().empty() && spec_ &&
      spec_->selected_shipping_option() &&
      profile_comparator()->IsShippingComplete(shipping_profiles_[0])) {
    selected_shipping_profile_ = shipping_profiles()[0].get();
  }
  UpdateIsReadyToPayAndNotifyObservers();
}

bool PaymentRequestState::ShouldShowShippingSection() const {
  if (!spec_ || !spec_->request_shipping())
    return false;

  return selected_app_ ? !selected_app_->HandlesShippingAddress() : true;
}

bool PaymentRequestState::ShouldShowContactSection() const {
  if (!spec_)
    return false;

  if (spec_->request_payer_name() &&
      (!selected_app_ || !selected_app_->HandlesPayerName())) {
    return true;
  }
  if (spec_->request_payer_email() &&
      (!selected_app_ || !selected_app_->HandlesPayerEmail())) {
    return true;
  }
  if (spec_->request_payer_phone() &&
      (!selected_app_ || !selected_app_->HandlesPayerPhone())) {
    return true;
  }

  return false;
}

base::WeakPtr<PaymentRequestState> PaymentRequestState::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PaymentRequestState::PopulateProfileCache() {
  std::vector<const autofill::AutofillProfile*> profiles =
      personal_data_manager_->address_data_manager().GetProfilesToSuggest();

  std::vector<raw_ptr<autofill::AutofillProfile, VectorExperimental>>
      raw_profiles_for_filtering;
  raw_profiles_for_filtering.reserve(profiles.size());

  // PaymentRequest may outlive the Profiles returned by the Data Manager.
  // Thus, we store copies, and return a vector of pointers to these copies
  // whenever Profiles are requested.
  for (size_t i = 0; i < profiles.size(); i++) {
    profile_cache_.push_back(
        std::make_unique<autofill::AutofillProfile>(*profiles[i]));
    raw_profiles_for_filtering.push_back(profile_cache_.back().get());
  }

  contact_profiles_ = profile_comparator()->FilterProfilesForContact(
      raw_profiles_for_filtering);
  shipping_profiles_ = profile_comparator()->FilterProfilesForShipping(
      raw_profiles_for_filtering);

  // Set the number of suggestions shown for the sections requested by the
  // merchant.
  if (ShouldShowContactSection()) {
    bool has_complete_contact =
        contact_profiles_.empty()
            ? false
            : profile_comparator()->IsContactInfoComplete(contact_profiles_[0]);
    if (journey_logger_) {
      journey_logger_->SetNumberOfSuggestionsShown(
          JourneyLogger::Section::SECTION_CONTACT_INFO,
          contact_profiles_.size(), has_complete_contact);
    }
  }
  if (ShouldShowShippingSection()) {
    bool has_complete_shipping =
        shipping_profiles_.empty()
            ? false
            : profile_comparator()->IsShippingComplete(shipping_profiles_[0]);
    if (journey_logger_) {
      journey_logger_->SetNumberOfSuggestionsShown(
          JourneyLogger::Section::SECTION_SHIPPING_ADDRESS,
          shipping_profiles_.size(), has_complete_shipping);
    }
  }
}

void PaymentRequestState::SetDefaultProfileSelections() {
  // Contact profiles were ordered by completeness in addition to frecency;
  // the first one is the best default selection.
  if (!contact_profiles().empty() &&
      profile_comparator()->IsContactInfoComplete(contact_profiles_[0]))
    selected_contact_profile_ = contact_profiles()[0].get();

  // Sort apps.
  PaymentApp::SortApps(&available_apps_);

  selected_app_ = nullptr;
  if (!available_apps_.empty() && available_apps_[0]->CanPreselect()) {
    selected_app_ = available_apps_[0]->AsWeakPtr();
    UpdateIsReadyToPayAndNotifyObservers();
  }

  SelectDefaultShippingAddressAndNotifyObservers();

  if (journey_logger_) {
    journey_logger_->SetNumberOfSuggestionsShown(
        JourneyLogger::Section::SECTION_PAYMENT_METHOD, available_apps().size(),
        selected_app_.get());
  }
}

void PaymentRequestState::UpdateIsReadyToPayAndNotifyObservers() {
  is_ready_to_pay_ =
      ArePaymentDetailsSatisfied() && ArePaymentOptionsSatisfied();
  NotifyOnSelectedInformationChanged();
}

void PaymentRequestState::NotifyOnGetAllPaymentAppsFinished() {
  for (auto& observer : observers_)
    observer.OnGetAllPaymentAppsFinished();
}

void PaymentRequestState::NotifyOnSelectedInformationChanged() {
  for (auto& observer : observers_)
    observer.OnSelectedInformationChanged();
}

bool PaymentRequestState::ArePaymentDetailsSatisfied() {
  // There is no need to check for supported networks, because only supported
  // apps are listed/created in the flow.
  return selected_app_ && selected_app_->IsCompleteForPayment();
}

bool PaymentRequestState::ArePaymentOptionsSatisfied() {
  if (is_waiting_for_merchant_validation_ || !spec_)
    return false;

  if (ShouldShowShippingSection() &&
      (!spec_->selected_shipping_option() ||
       !profile_comparator()->IsShippingComplete(selected_shipping_profile_))) {
    return false;
  }

  if (ShouldShowContactSection() &&
      !profile_comparator()->IsContactInfoComplete(selected_contact_profile_)) {
    return false;
  }

  return true;
}

void PaymentRequestState::OnAddressNormalized(
    bool success,
    const autofill::AutofillProfile& normalized_profile) {
  if (!delegate_)
    return;

  delegate_->OnShippingAddressSelected(
      data_util::GetPaymentAddressFromAutofillProfile(normalized_profile,
                                                      app_locale_));
}

void PaymentRequestState::OnGetTwaPackageName(
    GetTwaPackageNameCallback callback,
    const std::string& twa_package_name) {
  DCHECK(!get_all_apps_finished_);
  twa_package_name_ = twa_package_name;
  std::move(callback).Run(twa_package_name);
}

bool PaymentRequestState::IsInTwa() const {
  return !twa_package_name_.empty();
}

bool PaymentRequestState::GetCanMakePaymentValue() const {
  return are_requested_methods_supported_ ||
         can_make_payment_even_without_apps_;
}

bool PaymentRequestState::GetHasEnrolledInstrumentValue() const {
  return has_enrolled_instrument_ || can_make_payment_even_without_apps_;
}

}  // namespace payments
