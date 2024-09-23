// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_ANDROID_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_view_android.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/plus_addresses/metrics/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace plus_addresses {

class PlusAddressService;
class PlusAddressSettingService;

// A (hopefully) temporary Android-specific implementation of
// `PlusAddressCreationController`. The hope is to converge this back to a
// single implementation for desktop and android, but while both are in
// development, a split is convenient.
class PlusAddressCreationControllerAndroid
    : public PlusAddressCreationController,
      public content::WebContentsUserData<
          PlusAddressCreationControllerAndroid> {
 public:
  ~PlusAddressCreationControllerAndroid() override;

  // PlusAddressCreationController implementation:
  void OfferCreation(const url::Origin& main_frame_origin,
                     PlusAddressCallback callback) override;
  void TryAgainToReservePlusAddress() override;
  void OnRefreshClicked() override;
  void OnConfirmed() override;
  void OnCanceled() override;
  void OnDialogDestroyed() override;

  // A mechanism to avoid view entanglements, reducing the need for JNI mocking,
  // etc., while still allowing tests of specific business logic.
  // TODO(crbug.com/40276862): Add end-to-end coverage as the modal behavior
  // comes fully online.
  void set_suppress_ui_for_testing(bool should_suppress);

  // Validate storage and clearing of `plus_profile_`.
  std::optional<PlusProfile> get_plus_profile_for_testing();

 private:
  // WebContentsUserData:
  explicit PlusAddressCreationControllerAndroid(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      PlusAddressCreationControllerAndroid>;

  // Updates the dialog using `maybe_plus_profile` by either showing a plus
  // address or an error message.
  void OnPlusAddressReserved(const PlusProfileOrError& maybe_plus_profile);
  // If `maybe_plus_profile` is the expected value, autofills the targeted field
  // and closes the dialog. Otherwise shows an error message on the dialog.
  void OnPlusAddressConfirmed(const PlusProfileOrError& maybe_plus_profile);

  // Records the modal outcome and the time between `modal_shown_time_` and now
  // as modal shown duration and clear `modal_shown_time_`.
  void RecordModalShownOutcome(metrics::PlusAddressModalCompletionStatus status,
                               bool was_notice_shown);

  // Returns whether the onboarding screen with the notice should be shown.
  bool ShouldShowNotice() const;

  PlusAddressService* GetPlusAddressService();
  PlusAddressSettingService* GetPlusAddressSettingService();

  base::WeakPtr<PlusAddressCreationControllerAndroid> GetWeakPtr();

  std::unique_ptr<PlusAddressCreationViewAndroid> view_;
  url::Origin relevant_origin_;
  PlusAddressCallback callback_;
  bool suppress_ui_for_testing_ = false;
  // This is set by OnPlusAddressReserved and cleared when it's confirmed or
  // when the dialog is closed or cancelled.
  std::optional<PlusProfile> plus_profile_;

  // This is set on `OfferCreation`.
  std::optional<base::TimeTicks> modal_shown_time_;
  std::optional<metrics::PlusAddressModalCompletionStatus> modal_error_status_;
  // The number of responses from calls to reserve a plus address that a user
  // has made. This equals 1 + number of refreshes.
  int reserve_response_count_ = 0;

  base::WeakPtrFactory<PlusAddressCreationControllerAndroid> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_ANDROID_H_
