// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_

#include <string>
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

namespace autofill {

class VirtualCardEnrollUiModelTestApi;

// Container for Virtual Card Enrollment UI resources.
class VirtualCardEnrollUiModel final {
 public:
  // Create a UI model given the `enrollment_fields`.
  explicit VirtualCardEnrollUiModel(
      const VirtualCardEnrollmentFields& enrollment_fields);

  VirtualCardEnrollUiModel(const VirtualCardEnrollUiModel& other) = delete;
  VirtualCardEnrollUiModel& operator=(
      const VirtualCardEnrollUiModel& other) noexcept = delete;
  VirtualCardEnrollUiModel(VirtualCardEnrollUiModel&& other) = delete;
  VirtualCardEnrollUiModel& operator=(
      VirtualCardEnrollUiModel&& other) noexcept = delete;
  ~VirtualCardEnrollUiModel();

  bool operator==(const VirtualCardEnrollUiModel& other) const;

  // Title displayed in the view.
  const std::u16string& window_title() const { return window_title_; }
  // The main text displayed in the view.
  const std::u16string& explanatory_message() const {
    return explanatory_message_;
  }
  // The label text for virtual card enroll action (usually a button).
  const std::u16string& accept_action_text() const {
    return accept_action_text_;
  }
  // The label text for cancel action (usually a button).
  const std::u16string& cancel_action_text() const {
    return cancel_action_text_;
  }
  // The text used in the learn more link.
  const std::u16string& learn_more_link_text() const {
    return learn_more_link_text_;
  }
  // The enrollment fields for the virtual card.
  const VirtualCardEnrollmentFields& enrollment_fields() const {
    return enrollment_fields_;
  }

 private:
  friend class VirtualCardEnrollUiModelTestApi;
  std::u16string window_title_;
  std::u16string explanatory_message_;
  std::u16string accept_action_text_;
  std::u16string cancel_action_text_;
  std::u16string learn_more_link_text_;
  VirtualCardEnrollmentFields enrollment_fields_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_
