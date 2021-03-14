// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_MODEL_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_MODEL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace payments {

// The data model for PaymentCredentialEnrollmentView. Owned by the
// PaymentCredentialEnrollmentController.
class PaymentCredentialEnrollmentModel {
 public:
  PaymentCredentialEnrollmentModel();
  ~PaymentCredentialEnrollmentModel();

  PaymentCredentialEnrollmentModel(
      const PaymentCredentialEnrollmentModel& other) = delete;
  PaymentCredentialEnrollmentModel& operator=(
      const PaymentCredentialEnrollmentModel& other) = delete;

  // Title, e.g. "Faster card verification in Chrome using Touch ID"
  const std::u16string& title() const { return title_; }
  void set_title(const std::u16string& title) { title_ = title; }

  // Description.
  const std::u16string& description() const { return description_; }
  void set_description(const std::u16string& description) {
    description_ = description;
  }

  // Payment instrument name.
  const std::u16string& instrument_name() const { return instrument_name_; }
  void set_instrument_name(const std::u16string& instrument_name) {
    instrument_name_ = instrument_name;
  }

  // Extra description text, used for additional incognito information.
  const std::u16string& extra_description() const { return extra_description_; }
  void set_extra_description(const std::u16string& extra_description) {
    extra_description_ = extra_description;
  }

  // Instrument icon.
  const SkBitmap* instrument_icon() const { return instrument_icon_.get(); }
  void set_instrument_icon(std::unique_ptr<SkBitmap> instrument_icon) {
    instrument_icon_ = std::move(instrument_icon);
  }

  // Label for the accept button, e.g. "Use Touch ID".
  const std::u16string& accept_button_label() const {
    return accept_button_label_;
  }
  void set_accept_button_label(const std::u16string& accept_button_label) {
    accept_button_label_ = accept_button_label;
  }

  // Label for the cancel button, e.g. "Cancel".
  const std::u16string& cancel_button_label() const {
    return cancel_button_label_;
  }
  void set_cancel_button_label(const std::u16string& cancel_button_label) {
    cancel_button_label_ = cancel_button_label;
  }

  // Progress bar visibility.
  bool progress_bar_visible() const { return progress_bar_visible_; }
  void set_progress_bar_visible(bool progress_bar_visible) {
    progress_bar_visible_ = progress_bar_visible;
  }

  // Accept button enabled state.
  bool accept_button_enabled() const { return accept_button_enabled_; }
  void set_accept_button_enabled(bool accept_button_enabled) {
    accept_button_enabled_ = accept_button_enabled;
  }

  // Accept button visibility.
  bool accept_button_visible() const { return accept_button_visible_; }
  void set_accept_button_visible(bool accept_button_visible) {
    accept_button_visible_ = accept_button_visible;
  }

  // Cancel button enabled state.
  bool cancel_button_enabled() const { return cancel_button_enabled_; }
  void set_cancel_button_enabled(bool cancel_button_enabled) {
    cancel_button_enabled_ = cancel_button_enabled;
  }

  // Cancel button visibility.
  bool cancel_button_visible() const { return cancel_button_visible_; }
  void set_cancel_button_visible(bool cancel_button_visible) {
    cancel_button_visible_ = cancel_button_visible;
  }

  base::WeakPtr<PaymentCredentialEnrollmentModel> GetWeakPtr();

 private:
  std::u16string title_;
  std::u16string description_;
  std::u16string instrument_name_;
  std::u16string extra_description_;

  std::unique_ptr<SkBitmap> instrument_icon_;

  std::u16string accept_button_label_;
  std::u16string cancel_button_label_;

  bool progress_bar_visible_ = false;

  bool accept_button_enabled_ = true;
  bool accept_button_visible_ = true;

  bool cancel_button_enabled_ = true;
  bool cancel_button_visible_ = true;

  base::WeakPtrFactory<PaymentCredentialEnrollmentModel> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_CREDENTIAL_ENROLLMENT_MODEL_H_
