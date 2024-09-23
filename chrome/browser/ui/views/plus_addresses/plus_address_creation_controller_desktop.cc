// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"
#include "chrome/browser/ui/views/plus_addresses/plus_address_creation_dialog_delegate.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/plus_address_setting_service.h"

namespace plus_addresses {
// static
PlusAddressCreationController* PlusAddressCreationController::GetOrCreate(
    content::WebContents* web_contents) {
  PlusAddressCreationControllerDesktop::CreateForWebContents(web_contents);
  return PlusAddressCreationControllerDesktop::FromWebContents(web_contents);
}
PlusAddressCreationControllerDesktop::PlusAddressCreationControllerDesktop(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PlusAddressCreationControllerDesktop>(
          *web_contents) {}

PlusAddressCreationControllerDesktop::~PlusAddressCreationControllerDesktop() {
  // If the dialog is still open, ensure it gets cleaned up.
  if (dialog_delegate_ && dialog_delegate_->GetWidget()) {
    dialog_delegate_->GetWidget()->CloseNow();
  }
}

void PlusAddressCreationControllerDesktop::TryAgainToReservePlusAddress() {
  NOTIMPLEMENTED() << "Retrying to reserve a plus address is only supported on "
                      "mobile platforms.";
}

void PlusAddressCreationControllerDesktop::OnRefreshClicked() {
  PlusAddressService* plus_address_service = GetPlusAddressService();
  if (!plus_address_service) {
    return;
  }
  plus_address_service->RefreshPlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerDesktop::OnPlusAddressReserved,
          GetWeakPtr()));
}

PlusAddressService*
PlusAddressCreationControllerDesktop::GetPlusAddressService() {
  return PlusAddressServiceFactory::GetForBrowserContext(
      GetWebContents().GetBrowserContext());
}

PlusAddressSettingService*
PlusAddressCreationControllerDesktop::GetPlusAddressSettingService() {
  return PlusAddressSettingServiceFactory::GetForBrowserContext(
      GetWebContents().GetBrowserContext());
}

void PlusAddressCreationControllerDesktop::OfferCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {
  if (dialog_delegate_) {
    return;
  }
  PlusAddressService* plus_address_service = GetPlusAddressService();
  if (!plus_address_service) {
    return;
  }
  std::optional<std::string> maybe_email =
      plus_address_service->GetPrimaryEmail();
  if (maybe_email == std::nullopt) {
    return;
  }

  relevant_origin_ = main_frame_origin;
  callback_ = std::move(callback);

  const bool should_show_notice = ShouldShowNotice();
  modal_shown_time_ = base::TimeTicks::Now();
  metrics::RecordModalEvent(metrics::PlusAddressModalEvent::kModalShown,
                            /*is_notice_screen=*/should_show_notice);
  if (!suppress_ui_for_testing_) {
    dialog_delegate_ = std::make_unique<PlusAddressCreationDialogDelegate>(
        GetWeakPtr(), &GetWebContents(), maybe_email.value(),
        should_show_notice);
    constrained_window::ShowWebModalDialogViews(dialog_delegate_.get(),
                                                &GetWebContents());
  }

  plus_address_service->ReservePlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerDesktop::OnPlusAddressReserved,
          GetWeakPtr()));
}

void PlusAddressCreationControllerDesktop::OnConfirmed() {
  // The UI prevents any attempt to Confirm if Reserve() had failed.
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
            &PlusAddressCreationControllerDesktop::OnPlusAddressConfirmed,
            GetWeakPtr()));
  }
}
void PlusAddressCreationControllerDesktop::OnCanceled() {
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
void PlusAddressCreationControllerDesktop::OnDialogDestroyed() {
  dialog_delegate_.reset();
  plus_profile_.reset();
}

PlusAddressCreationView*
PlusAddressCreationControllerDesktop::get_view_for_testing() {
  return dialog_delegate_.get();
}

void PlusAddressCreationControllerDesktop::RecordModalShownOutcome(
    metrics::PlusAddressModalCompletionStatus status,
    bool was_notice_shown) {
  if (modal_shown_time_.has_value()) {
    // The number of refreshes is equal to the number of `reserve` responses
    // minus 1, since the first displayed plus address also calls `reserve`.
    metrics::RecordModalShownOutcome(
        status, base::TimeTicks::Now() - modal_shown_time_.value(),
        std::max(0, reserve_response_count_ - 1), was_notice_shown);
    modal_shown_time_.reset();
    reserve_response_count_ = 0;
  }
}

void PlusAddressCreationControllerDesktop::set_suppress_ui_for_testing(
    bool should_suppress) {
  suppress_ui_for_testing_ = should_suppress;
}

std::optional<PlusProfile>
PlusAddressCreationControllerDesktop::get_plus_profile_for_testing() {
  return plus_profile_;
}

base::WeakPtr<PlusAddressCreationControllerDesktop>
PlusAddressCreationControllerDesktop::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PlusAddressCreationControllerDesktop::OnPlusAddressReserved(
    const PlusProfileOrError& maybe_plus_profile) {
  if (maybe_plus_profile.has_value()) {
    plus_profile_ = maybe_plus_profile.value();
    ++reserve_response_count_;
  } else {
    modal_error_status_ =
        metrics::PlusAddressModalCompletionStatus::kReservePlusAddressError;
  }
  // Display result on UI only after setting `plus_profile_` to prevent
  // premature confirm without `plus_profile_` value.
  if (dialog_delegate_) {
    PlusAddressService* service = GetPlusAddressService();
    const bool show_refresh =
        service && service->IsRefreshingSupported(relevant_origin_);
    dialog_delegate_->ShowReserveResult(maybe_plus_profile, show_refresh);
  }
}

void PlusAddressCreationControllerDesktop::OnPlusAddressConfirmed(
    const PlusProfileOrError& maybe_plus_profile) {
  if (maybe_plus_profile.has_value()) {
    // Autofill the plus address.
    std::move(callback_).Run(*maybe_plus_profile->plus_address);

    // If this was a first run dialog, record that the user has accepted the
    // notice.
    const bool was_notice_shown = ShouldShowNotice();
    if (was_notice_shown) {
      GetPlusAddressSettingService()->SetHasAcceptedNotice();
    }

    RecordModalShownOutcome(
        metrics::PlusAddressModalCompletionStatus::kModalConfirmed,
        was_notice_shown);
  } else {
    modal_error_status_ =
        metrics::PlusAddressModalCompletionStatus::kConfirmPlusAddressError;
  }

  // Display result on UI after setting `modal_error_status_` to ensure correct
  // metric is recorded.
  if (dialog_delegate_) {
    dialog_delegate_->ShowConfirmResult(maybe_plus_profile);
  }
}

bool PlusAddressCreationControllerDesktop::ShouldShowNotice() const {
  // `this` is never created as a `const` member - therefore the cast is safe.
  const PlusAddressSettingService* setting_service =
      const_cast<PlusAddressCreationControllerDesktop*>(this)
          ->GetPlusAddressSettingService();

  return setting_service && !setting_service->GetHasAcceptedNotice() &&
         base::FeatureList::IsEnabled(
             features::kPlusAddressUserOnboardingEnabled);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerDesktop);
}  // namespace plus_addresses
