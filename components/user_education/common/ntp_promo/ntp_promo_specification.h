// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_SPECIFICATION_H_
#define COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_SPECIFICATION_H_

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/ntp_promo/ntp_promo_identifier.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_metadata.h"

namespace user_education {

// Visual content of the promo, for consumption by the UI.
class NtpPromoContent {
 public:
  NtpPromoContent() = delete;
  NtpPromoContent(const NtpPromoContent&);
  NtpPromoContent(NtpPromoContent&&) noexcept;
  ~NtpPromoContent();
  NtpPromoContent(std::string_view icon_name,
                  int body_text_string_id,
                  int action_button_text_string_id);

  std::string_view icon_name() const { return icon_name_; }
  int body_text_string_id() const { return body_text_string_id_; }
  int action_button_text_string_id() const {
    return action_button_text_string_id_;
  }

 private:
  std::string icon_name_;
  int body_text_string_id_;
  int action_button_text_string_id_;
};

// Describes a promo displayed on the New Tab Page, including the visual
// properties and ties to the features being promoted.
class NtpPromoSpecification {
 public:
  // Eligibility status returned by an EligibilityCallback.
  enum class Eligibility { kIneligible, kEligible, kCompleted };

  // Receives the profile to be evaluated for eligibility.
  using EligibilityCallback = base::RepeatingCallback<Eligibility(
      const user_education::UserEducationContextPtr&)>;

  using ShowCallback = base::RepeatingClosure;

  // Receives a browser in which the action can be taken, and an object
  // to be held by the invoked flow until termination.
  using ActionCallback = base::RepeatingCallback<void(
      const user_education::UserEducationContextPtr&)>;

  NtpPromoSpecification() = delete;
  NtpPromoSpecification(NtpPromoSpecification&&) noexcept;
  ~NtpPromoSpecification();
  NtpPromoSpecification(NtpPromoIdentifier id,
                        NtpPromoContent content,
                        EligibilityCallback eligibility_callback,
                        ShowCallback show_callback,
                        ActionCallback action_callback,
                        base::flat_set<NtpPromoIdentifier> show_after,
                        user_education::Metadata);

  const NtpPromoContent& content() const { return content_; }
  EligibilityCallback eligibility_callback() const {
    return eligibility_callback_;
  }
  ShowCallback show_callback() const { return show_callback_; }
  ActionCallback action_callback() const { return action_callback_; }
  const std::string& id() const { return id_; }
  const base::flat_set<NtpPromoIdentifier>& show_after() const {
    return show_after_;
  }
  const user_education::Metadata& metadata() const { return metadata_; }

 private:
  NtpPromoIdentifier id_;

  // Visual content of the promo.
  NtpPromoContent content_;

  // Called to test the eligibility of the promo (ie. can it be shown or not).
  EligibilityCallback eligibility_callback_;

  // Called when the promo is shown, for purposes of promo-specific metrics
  // collection.
  ShowCallback show_callback_;

  // Called to invoke the promoted action flow.
  ActionCallback action_callback_;

  // The set of other promos that must be listed before the current promo,
  // whenever promos are shown to the user.
  base::flat_set<NtpPromoIdentifier> show_after_;

  // Required in all User Education registries.
  user_education::Metadata metadata_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_NTP_PROMO_NTP_PROMO_SPECIFICATION_H_
