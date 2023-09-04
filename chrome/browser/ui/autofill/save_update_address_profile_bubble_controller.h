// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_

#include "components/autofill/core/browser/autofill_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/point.h"

namespace autofill {

// Interface that exposes controller functionality to SaveAddressProfileView
// bubble.
class SaveUpdateAddressProfileBubbleController {
 public:
  struct HeaderImages {
    ui::ImageModel light;
    ui::ImageModel dark;
  };

  virtual ~SaveUpdateAddressProfileBubbleController() = default;

  virtual std::u16string GetWindowTitle() const = 0;
  virtual absl::optional<HeaderImages> GetHeaderImages() const = 0;
  virtual std::u16string GetBodyText() const = 0;
  virtual std::u16string GetAddressSummary() const = 0;
  virtual std::u16string GetProfileEmail() const = 0;
  virtual std::u16string GetProfilePhone() const = 0;
  virtual std::u16string GetOkButtonLabel() const = 0;
  virtual AutofillClient::SaveAddressProfileOfferUserDecision
  GetCancelCallbackValue() const = 0;
  virtual std::u16string GetFooterMessage() const = 0;
  virtual const AutofillProfile& GetProfileToSave() const = 0;
  virtual const AutofillProfile* GetOriginalProfile() const = 0;
  virtual void OnUserDecision(
      AutofillClient::SaveAddressProfileOfferUserDecision decision,
      AutofillProfile profile) = 0;
  virtual void OnUserCanceledEditing() = 0;
  virtual void OnEditButtonClicked() = 0;
  virtual void OnBubbleClosed() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_SAVE_UPDATE_ADDRESS_PROFILE_BUBBLE_CONTROLLER_H_
