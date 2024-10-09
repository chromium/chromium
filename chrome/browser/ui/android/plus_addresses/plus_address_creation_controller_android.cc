// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"

#include <optional>

#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"
#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_view_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/plus_address_ui_utils.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

namespace plus_addresses {

namespace {
PlusAddressCreationErrorStateInfo GetReserveErrorStateInfo(
    PlusAddressRequestError error) {
  if (error.IsTimeoutError()) {
    return PlusAddressCreationErrorStateInfo(
        /*error_type=*/PlusAddressCreationBottomSheetErrorType::kReserveTimeout,
        /*title=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_RESERVE_TIMEOUT_ERROR_TITLE_ANDROID),
        /*description=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_RESERVE_TIMEOUT_ERROR_DESCRIPTION_ANDROID),
        /*ok_text=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_TRY_AGAIN_BUTTON_TEXT_ANDROID),
        /*cancel_text=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_CANCEL_BUTTON_TEXT_ANDROID));
  }
  if (error.IsQuotaError()) {
    // Cancel text is empty in this case.
    return PlusAddressCreationErrorStateInfo(
        /*error_type=*/PlusAddressCreationBottomSheetErrorType::kReserveQuota,
        /*title=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_RESERVE_QUOTA_ERROR_TITLE_ANDROID),
        /*description=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_RESERVE_QUOTA_ERROR_DESCRIPTION_ANDROID),
        /*ok_text=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_OK_BUTTON_TEXT_ANDROID),
        /*cancel_text=*/u"");
  }
  return PlusAddressCreationErrorStateInfo(
      /*error_type=*/PlusAddressCreationBottomSheetErrorType::kReserveGeneric,
      /*title=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_RESERVE_GENERIC_ERROR_TITLE_ANDROID),
      /*description=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_RESERVE_GENERIC_ERROR_DESCRIPTION_ANDROID),
      /*ok_text=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_TRY_AGAIN_BUTTON_TEXT_ANDROID),
      /*cancel_text=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_CANCEL_BUTTON_TEXT_ANDROID));
}

PlusAddressCreationErrorStateInfo GetCreateErrorStateInfo(
    PlusAddressRequestError error) {
  if (error.IsTimeoutError()) {
    return PlusAddressCreationErrorStateInfo(
        /*error_type=*/PlusAddressCreationBottomSheetErrorType::kCreateTimeout,
        /*title=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_CREATE_TIMEOUT_ERROR_TITLE_ANDROID),
        /*description=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_CREATE_TIMEOUT_ERROR_DESCRIPTION_ANDROID),
        /*ok_text=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_TRY_AGAIN_BUTTON_TEXT_ANDROID),
        /*cancel_text=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_CANCEL_BUTTON_TEXT_ANDROID));
  }
  if (error.IsQuotaError()) {
    // Cancel text is empty in this case.
    return PlusAddressCreationErrorStateInfo(
        /*error_type=*/PlusAddressCreationBottomSheetErrorType::kCreateQuota,
        /*title=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_CREATE_QUOTA_ERROR_TITLE_ANDROID),
        /*description=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_CREATE_QUOTA_ERROR_DESCRIPTION_ANDROID),
        /*ok_text=*/
        l10n_util::GetStringUTF16(
            IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_OK_BUTTON_TEXT_ANDROID),
        /*cancel_text=*/u"");
  }
  return PlusAddressCreationErrorStateInfo(
      /*error_type=*/PlusAddressCreationBottomSheetErrorType::kCreateGeneric,
      /*title=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_CREATE_GENERIC_ERROR_TITLE_ANDROID),
      /*description=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_CREATE_GENERIC_ERROR_DESCRIPTION_ANDROID),
      /*ok_text=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_TRY_AGAIN_BUTTON_TEXT_ANDROID),
      /*cancel_text=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_CANCEL_BUTTON_TEXT_ANDROID));
}

PlusAddressCreationErrorStateInfo GetAffiliationErrorStateInfo(
    PlusProfile existing_profile) {
  return PlusAddressCreationErrorStateInfo(
      /*error_type=*/PlusAddressCreationBottomSheetErrorType::
          kCreateAffiliation,
      /*title=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_CREATE_AFFILIATION_ERROR_TITLE_ANDROID),
      /*description=*/
      l10n_util::GetStringFUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_CREATE_AFFILIATION_ERROR_DESCRIPTION_ANDROID,
          GetOriginForDisplay(existing_profile),
          base::UTF8ToUTF16(*existing_profile.plus_address)),
      /*ok_text=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_USE_EXISTING_ADDRESS_BUTTON_TEXT_ANDROID),
      /*cancel_text=*/
      l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_BOTTOMSHEET_ERROR_CANCEL_BUTTON_TEXT_ANDROID));
}
}  // namespace

// static
PlusAddressCreationController* PlusAddressCreationController::GetOrCreate(
    content::WebContents* web_contents) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(web_contents);
  return PlusAddressCreationControllerAndroid::FromWebContents(web_contents);
}

PlusAddressCreationControllerAndroid::PlusAddressCreationControllerAndroid(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PlusAddressCreationControllerAndroid>(
          *web_contents) {}
PlusAddressCreationControllerAndroid::~PlusAddressCreationControllerAndroid() =
    default;
void PlusAddressCreationControllerAndroid::OfferCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {
  if (view_) {
    return;
  }

  PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext());
  if (!plus_address_service) {
    // TODO(crbug.com/40276862): Verify expected behavior in this case and the
    // missing email case below.
    return;
  }
  std::optional<std::string> maybe_email =
      plus_address_service->GetPrimaryEmail();
  if (maybe_email == std::nullopt) {
    return;
  }

  callback_ = std::move(callback);
  relevant_origin_ = main_frame_origin;
  const bool should_show_notice = ShouldShowNotice();
  metrics::RecordModalEvent(metrics::PlusAddressModalEvent::kModalShown,
                            should_show_notice);
  modal_shown_time_ = base::TimeTicks::Now();
  if (!suppress_ui_for_testing_) {
    view_ = std::make_unique<PlusAddressCreationViewAndroid>(GetWeakPtr());
    view_->ShowInit(
        GetWebContents().GetNativeView(),
        TabModelList::GetTabModelForWebContents(&GetWebContents()),
        maybe_email.value(),
        plus_address_service->IsRefreshingSupported(relevant_origin_),
        /*has_accepted_notice=*/!should_show_notice);
  }
  plus_address_service->ReservePlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerAndroid::OnPlusAddressReserved,
          GetWeakPtr()));
}

void PlusAddressCreationControllerAndroid::TryAgainToReservePlusAddress() {
  PlusAddressService* plus_address_service = GetPlusAddressService();
  if (!plus_address_service) {
    return;
  }
  plus_address_service->ReservePlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerAndroid::OnPlusAddressReserved,
          GetWeakPtr()));
}

void PlusAddressCreationControllerAndroid::OnRefreshClicked() {
  PlusAddressService* plus_address_service = GetPlusAddressService();
  if (!plus_address_service) {
    return;
  }
  plus_address_service->RefreshPlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerAndroid::OnPlusAddressReserved,
          GetWeakPtr()));
}

void PlusAddressCreationControllerAndroid::OnConfirmed() {
  CHECK(plus_profile_.has_value());
  metrics::RecordModalEvent(metrics::PlusAddressModalEvent::kModalConfirmed,
                            ShouldShowNotice());
  if (plus_profile_->is_confirmed) {
    OnPlusAddressConfirmed(plus_profile_.value());
    return;
  }
  if (PlusAddressService* plus_address_service = GetPlusAddressService()) {
    // Note: this call may fail if this modal is confirmed on the same
    // `relevant_origin_` from another device.
    plus_address_service->ConfirmPlusAddress(
        relevant_origin_, plus_profile_->plus_address,
        base::BindOnce(
            &PlusAddressCreationControllerAndroid::OnPlusAddressConfirmed,
            GetWeakPtr()));
  }
}

void PlusAddressCreationControllerAndroid::OnCanceled() {
  // TODO(b/320541525) ModalEvent is in sync with actual user action. May
  // re-evaluate the use of this metric when modal becomes more complex.
  const bool was_notice_shown = ShouldShowNotice();
  metrics::RecordModalEvent(metrics::PlusAddressModalEvent::kModalCanceled,
                            was_notice_shown);
  if (modal_error_status_.has_value()) {
    RecordModalShownOutcome(modal_error_status_.value(), was_notice_shown);
    modal_error_status_.reset();
  } else {
    RecordModalShownOutcome(
        metrics::PlusAddressModalCompletionStatus::kModalCanceled,
        was_notice_shown);
  }
}

void PlusAddressCreationControllerAndroid::OnDialogDestroyed() {
  view_.reset();
  plus_profile_.reset();
}

void PlusAddressCreationControllerAndroid::set_suppress_ui_for_testing(
    bool should_suppress) {
  suppress_ui_for_testing_ = should_suppress;
}

std::optional<PlusProfile>
PlusAddressCreationControllerAndroid::get_plus_profile_for_testing() {
  return plus_profile_;
}

void PlusAddressCreationControllerAndroid::OnPlusAddressReserved(
    const PlusProfileOrError& maybe_plus_profile) {
  // Note that in case of `suppress_ui_for_testing_` or bottom sheet dismissal
  // prior to service response, `view_` will be null.
  if (PlusAddressService* service = GetPlusAddressService();
      view_ && service && !service->IsRefreshingSupported(relevant_origin_)) {
    view_->HideRefreshButton();
  }
  if (maybe_plus_profile.has_value()) {
    plus_profile_ = maybe_plus_profile.value();
    ++reserve_response_count_;
    if (view_) {
      view_->ShowReservedProfile(maybe_plus_profile.value());
    }
  } else {
    modal_error_status_ =
        metrics::PlusAddressModalCompletionStatus::kReservePlusAddressError;
    if (view_) {
      view_->ShowError(GetReserveErrorStateInfo(maybe_plus_profile.error()));
    }
  }
}

void PlusAddressCreationControllerAndroid::OnPlusAddressConfirmed(
    const PlusProfileOrError& maybe_plus_profile) {
  CHECK(plus_profile_.has_value());
  if (maybe_plus_profile.has_value()) {
    if (maybe_plus_profile->plus_address == plus_profile_->plus_address) {
      if (view_) {
        view_->FinishConfirm();
      }
      const bool was_notice_shown = ShouldShowNotice();
      if (was_notice_shown) {
        GetPlusAddressSettingService()->SetHasAcceptedNotice();
      }
      std::move(callback_).Run(*maybe_plus_profile->plus_address);
      RecordModalShownOutcome(
          metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
          was_notice_shown);
    } else {
      // Persist the confirmed profile if it's different from the reserved one.
      plus_profile_ = maybe_plus_profile.value();
      modal_error_status_ =
          metrics::PlusAddressModalCompletionStatus::kConfirmPlusAddressError;
      if (view_) {
        view_->ShowError(
            GetAffiliationErrorStateInfo(maybe_plus_profile.value()));
      }
    }
  } else {
    modal_error_status_ =
        metrics::PlusAddressModalCompletionStatus::kConfirmPlusAddressError;
    // Note that in case of `suppress_ui_for_testing_` or bottom sheet dismissal
    // prior to service response, `view_` will be null.
    if (view_) {
      view_->ShowError(GetCreateErrorStateInfo(maybe_plus_profile.error()));
    }
  }
}

void PlusAddressCreationControllerAndroid::RecordModalShownOutcome(
    metrics::PlusAddressModalCompletionStatus status,
    bool was_notice_shown) {
  if (modal_shown_time_.has_value()) {
    metrics::RecordModalShownOutcome(
        status, base::TimeTicks::Now() - *modal_shown_time_,
        std::max(reserve_response_count_ - 1, 0), was_notice_shown);
    modal_shown_time_.reset();
    reserve_response_count_ = 0;
  }
}

bool PlusAddressCreationControllerAndroid::ShouldShowNotice() const {
  // `this` is never created as a `const` member - therefore the cast is safe.
  const PlusAddressSettingService* setting_service =
      const_cast<PlusAddressCreationControllerAndroid*>(this)
          ->GetPlusAddressSettingService();

  return setting_service && !setting_service->GetHasAcceptedNotice() &&
         base::FeatureList::IsEnabled(
             features::kPlusAddressUserOnboardingEnabled);
}

PlusAddressService*
PlusAddressCreationControllerAndroid::GetPlusAddressService() {
  return PlusAddressServiceFactory::GetForBrowserContext(
      GetWebContents().GetBrowserContext());
}

PlusAddressSettingService*
PlusAddressCreationControllerAndroid::GetPlusAddressSettingService() {
  return PlusAddressSettingServiceFactory::GetForBrowserContext(
      GetWebContents().GetBrowserContext());
}

base::WeakPtr<PlusAddressCreationControllerAndroid>
PlusAddressCreationControllerAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerAndroid);
}  // namespace plus_addresses
