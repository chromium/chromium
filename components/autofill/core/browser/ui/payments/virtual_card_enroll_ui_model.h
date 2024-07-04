// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_

#include <base/memory/weak_ptr.h>
#include <base/observer_list.h>
#include <base/observer_list_types.h>

#include <string>

#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

namespace autofill {

class VirtualCardEnrollUiModelTestApi;

// Container for Virtual Card Enrollment UI resources.
class VirtualCardEnrollUiModel final {
 public:
  // States that the enrollment progress can be in.
  enum class EnrollmentProgress {
    // The enrollment is offered to the user.
    kOffered,
    // The enrollment has succeeded.
    kEnrolled,
    // The enrollment has failed.
    kFailed,
    kMaxValue = kFailed,
  };

  // Interface for observers of this model.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    // Called when the `enrollment_progress` value changes.
    virtual void OnEnrollmentProgressChanged(
        EnrollmentProgress enrollment_progress) = 0;
  };

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

  void AddObserver(Observer* observer) { observer_list_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observer_list_.RemoveObserver(observer);
  }

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

  EnrollmentProgress enrollment_progress() const {
    return enrollment_progress_;
  }

  void SetEnrollmentProgress(EnrollmentProgress enrollment_progress) {
    if (enrollment_progress_ != enrollment_progress) {
      enrollment_progress_ = enrollment_progress;
      for (auto& observer : observer_list_) {
        observer.OnEnrollmentProgressChanged(enrollment_progress_);
      }
    }
  }

  base::WeakPtr<VirtualCardEnrollUiModel> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class VirtualCardEnrollUiModelTestApi;
  std::u16string window_title_;
  std::u16string explanatory_message_;
  std::u16string accept_action_text_;
  std::u16string cancel_action_text_;
  std::u16string learn_more_link_text_;
  VirtualCardEnrollmentFields enrollment_fields_;
  EnrollmentProgress enrollment_progress_{EnrollmentProgress::kOffered};

  base::ObserverList<Observer> observer_list_;
  base::WeakPtrFactory<VirtualCardEnrollUiModel> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_PAYMENTS_VIRTUAL_CARD_ENROLL_UI_MODEL_H_
