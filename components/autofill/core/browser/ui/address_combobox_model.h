// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_COMBOBOX_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_COMBOBOX_MODEL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/observer_list.h"
#include "ui/base/models/combobox_model.h"

namespace autofill {

class AutofillProfile;
class PersonalDataManager;

// A combobox model for listing addresses by a generated user visible string and
// have a unique id to identify the one selected by the user.
class AddressComboboxModel : public ui::ComboboxModel {
 public:
  // Enumerate the profiles from |personal_data_manager| to expose them in a
  // combobox using |app_locale| for proper format. |default_selected_guid| is
  // an optional argument to specify which address should be selected by
  // default.
  AddressComboboxModel(const PersonalDataManager& personal_data_manager,
                       const std::string& app_locale,
                       const std::string& default_selected_guid);

  AddressComboboxModel(const AddressComboboxModel&) = delete;
  AddressComboboxModel& operator=(const AddressComboboxModel&) = delete;

  ~AddressComboboxModel() override;

  // ui::ComboboxModel implementation:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  bool IsItemSeparatorAt(size_t index) const override;
  std::optional<size_t> GetDefaultIndex() const override;

  // Adds |profile| to model and return its combobox index. The lifespan of
  // |profile| beyond this call is undefined so a copy must be made.
  size_t AddNewProfile(const AutofillProfile& profile);

  // Returns the unique identifier of the profile at |index|, unless |index|
  // refers to a special entry, in which case an empty string is returned.
  std::string GetItemIdentifierAt(size_t index);

  // Returns the combobox index of the item with the given id or nullopt if it's
  // not found.
  std::optional<size_t> GetIndexOfIdentifier(
      const std::string& identifier) const;

 private:
  // Update |addresses_| based on |profiles_cache_| and notify observers.
  void UpdateAddresses();

  // List of <id, user visible string> pairs for the addresses extracted from
  // the |personal_data_manager| passed in the constructor.
  std::vector<std::pair<std::string, std::u16string>> addresses_;

  // A cached copy of all profiles to allow rebuilding the differentiating
  // labels when new profiles are added.
  std::vector<std::unique_ptr<AutofillProfile>> profiles_cache_;

  // Application locale, also needed when a new profile is added.
  std::string app_locale_;

  // If non empty, the guid of the address that should be selected by default.
  std::string default_selected_guid_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_COMBOBOX_MODEL_H_
