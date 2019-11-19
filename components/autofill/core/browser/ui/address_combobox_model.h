// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_COMBOBOX_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_COMBOBOX_MODEL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
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
  ~AddressComboboxModel() override;

  // ui::ComboboxModel implementation:
  int GetItemCount() const override;
  base::string16 GetItemAt(int index) override;
  bool IsItemSeparatorAt(int index) override;
  int GetDefaultIndex() const override;
  void AddObserver(ui::ComboboxModelObserver* observer) override;
  void RemoveObserver(ui::ComboboxModelObserver* observer) override;

  // Adds |profile| to model and return its combobox index. The lifespan of
  // |profile| beyond this call is undefined so a copy must be made.
  int AddNewProfile(const AutofillProfile& profile);

  // Returns the unique identifier of the profile at |index|, unless |index|
  // refers to a special entry, in which case an empty string is returned.
  std::string GetItemIdentifierAt(int index);

  // Returns the combobox index of the item with the given id or -1 if it's not
  // found.
  int GetIndexOfIdentifier(const std::string& identifier) const;

 private:
  // Update |addresses_| based on |profiles_cache_| and notify observers.
  void UpdateAddresses();

  // List of <id, user visible string> pairs for the addresses extracted from
  // the |personal_data_manager| passed in the constructor.
  std::vector<std::pair<std::string, base::string16>> addresses_;

  // A cached copy of all profiles to allow rebuilding the differentiating
  // labels when new profiles are added.
  std::vector<std::unique_ptr<AutofillProfile>> profiles_cache_;

  // Application locale, also needed when a new profile is added.
  std::string app_locale_;

  // If non empty, the guid of the address that should be selected by default.
  std::string default_selected_guid_;

  // To be called when the data for the given country code was loaded.
  base::ObserverList<ui::ComboboxModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AddressComboboxModel);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESS_COMBOBOX_MODEL_H_
