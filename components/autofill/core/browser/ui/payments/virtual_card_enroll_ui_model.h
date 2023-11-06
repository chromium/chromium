// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_

#include <string>
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

namespace autofill {

// Container for Virtual Card Enrollment UI resources.
struct VirtualCardEnrollUiModel {
  // Title displayed in the view.
  std::u16string window_title;
  // The main text displayed in the view.
  std::u16string explanatory_message;
  // The label text for virtual card enroll action (usually a button).
  std::u16string accept_action_text;
  // The label text for cancel action (usually a button).
  std::u16string cancel_action_text;
  // The text used in the learn more link.
  std::u16string learn_more_link_text;
  // The enrollment fields for the virtual card.
  VirtualCardEnrollmentFields enrollment_fields;

  VirtualCardEnrollUiModel();
  ~VirtualCardEnrollUiModel();

  bool operator==(const VirtualCardEnrollUiModel& other) const;

  // Copying is allowed for value-semantics.
  //
  // For performance sensitive code, be aware this could be a large copy: Many
  // fields are contained in `enrollment_fields`.
  VirtualCardEnrollUiModel(const VirtualCardEnrollUiModel& other);
  VirtualCardEnrollUiModel& operator=(
      const VirtualCardEnrollUiModel& other) noexcept;

  VirtualCardEnrollUiModel(VirtualCardEnrollUiModel&& other);
  VirtualCardEnrollUiModel& operator=(
      VirtualCardEnrollUiModel&& other) noexcept;

  // Create a UI model given the `enrollment_fields`.
  static VirtualCardEnrollUiModel Create(
      const VirtualCardEnrollmentFields& enrollment_fields);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_
