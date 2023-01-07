// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_MODEL_H_
#define COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_MODEL_H_

#include <string>

#include "base/memory/weak_ptr.h"

namespace payments {

// The data model for the secure payment confirmation 'no matching credentials'
// dialog.
class SecurePaymentConfirmationNoCredsModel {
 public:
  SecurePaymentConfirmationNoCredsModel();
  ~SecurePaymentConfirmationNoCredsModel();

  // Disallow copy and assign.
  SecurePaymentConfirmationNoCredsModel(
      const SecurePaymentConfirmationNoCredsModel& other) = delete;
  SecurePaymentConfirmationNoCredsModel& operator=(
      const SecurePaymentConfirmationNoCredsModel& other) = delete;

  const std::u16string& no_creds_text() const { return no_creds_text_; }
  void set_no_creds_text(const std::u16string& no_creds_text) {
    no_creds_text_ = no_creds_text;
  }

  // Opt Out text visibility and label.
  bool opt_out_visible() const { return opt_out_visible_; }
  void set_opt_out_visible(const bool opt_out_visible) {
    opt_out_visible_ = opt_out_visible;
  }
  const std::u16string& opt_out_label() const { return opt_out_label_; }
  void set_opt_out_label(const std::u16string& opt_out_label) {
    opt_out_label_ = opt_out_label;
  }
  const std::u16string& opt_out_link_label() const {
    return opt_out_link_label_;
  }
  void set_opt_out_link_label(const std::u16string& opt_out_link_label) {
    opt_out_link_label_ = opt_out_link_label;
  }

  // Relying Party id (origin); used in the opt out dialog.
  const std::u16string& relying_party_id() const { return relying_party_id_; }
  void set_relying_party_id(const std::u16string& relying_party_id) {
    relying_party_id_ = relying_party_id;
  }

  base::WeakPtr<SecurePaymentConfirmationNoCredsModel> GetWeakPtr();

 private:
  std::u16string no_creds_text_;

  bool opt_out_visible_ = false;
  std::u16string opt_out_label_;
  std::u16string opt_out_link_label_;

  std::u16string relying_party_id_;

  base::WeakPtrFactory<SecurePaymentConfirmationNoCredsModel> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_SECURE_PAYMENT_CONFIRMATION_NO_CREDS_MODEL_H_
