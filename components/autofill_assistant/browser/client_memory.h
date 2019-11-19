// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_

#include <map>
#include <string>

#include "base/optional.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/website_login_fetcher.h"

namespace autofill_assistant {
// Data shared between scripts and actions.
class ClientMemory {
 public:
  ClientMemory();
  ~ClientMemory();

  // Selected credit card, if any. It will be a nullptr if didn't select
  // anything or if selected 'Fill manually'.
  const autofill::CreditCard* selected_card() const;

  // Return true if card has been selected, otherwise return false.
  // Note that selected_card() might return nullptr when has_selected_card() is
  // true because fill manually was chosen.
  bool has_selected_card() const;

  // Selected address for |name|. It will be a nullptr if didn't select anything
  // or if selected 'Fill manually'.
  const autofill::AutofillProfile* selected_address(
      const std::string& name) const;

  // Return true if address has been selected, otherwise return false.
  // Note that selected_address() might return nullptr when
  // has_selected_address() is true because fill manually was chosen.
  bool has_selected_address(const std::string& name) const;

  // Set the selected card.
  void set_selected_card(std::unique_ptr<autofill::CreditCard> card);

  // Set the selected address for |name|.
  void set_selected_address(const std::string& name,
                            std::unique_ptr<autofill::AutofillProfile> address);

  // Set the selected login.
  void set_selected_login(const WebsiteLoginFetcher::Login& login);

  // Return true if a login has been selected, otherwise false.
  bool has_selected_login() const;

  // The selected login or nullptr if no login was selected.
  const WebsiteLoginFetcher::Login* selected_login() const;

  // The additional value for |key|, or nullptr if it does not exist.
  const std::string* additional_value(const std::string& key);

  // Returns true if an additional value is stored for |key|.
  bool has_additional_value(const std::string& key);

  // Sets the additional value for |key|.
  void set_additional_value(const std::string& key, const std::string& value);

  std::string GetAllAddressKeyNames() const;

 private:
  base::Optional<std::unique_ptr<autofill::CreditCard>> selected_card_;
  base::Optional<WebsiteLoginFetcher::Login> selected_login_;

  // The address key requested by the autofill action.
  std::map<std::string, std::unique_ptr<autofill::AutofillProfile>>
      selected_addresses_;
  // Maps keys to additional values.
  std::map<std::string, std::string> additional_values_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_MEMORY_H_
