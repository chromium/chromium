// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_combobox_model.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model_observer.h"
#include "ui/gfx/text_elider.h"

namespace autofill {

namespace {
// There's one header entry to prompt the user to select an address, and a
// separator.
int kNbHeaderEntries = 2;
}  // namespace

AddressComboboxModel::AddressComboboxModel(
    const PersonalDataManager& personal_data_manager,
    const std::string& app_locale,
    const std::string& default_selected_guid)
    : app_locale_(app_locale), default_selected_guid_(default_selected_guid) {
  for (const auto* profile : personal_data_manager.GetProfilesToSuggest()) {
    profiles_cache_.push_back(std::make_unique<AutofillProfile>(*profile));
  }
  UpdateAddresses();
}

AddressComboboxModel::~AddressComboboxModel() {}

int AddressComboboxModel::GetItemCount() const {
  // When there are not addresses, a special entry is shown to prompt the user
  // to add addresses, but nothing else is shown, since there are no address to
  // select from, and no need for a separator.
  if (addresses_.size() == 0)
    return 1;
  // If there are addresses to choose from, but none is selected, add extra
  // items for the "Select" entry and a separator.
  return addresses_.size() + kNbHeaderEntries;
}

base::string16 AddressComboboxModel::GetItemAt(int index) {
  DCHECK_GE(index, 0);
  // A special entry is always added at index 0 and a separator at index 1.
  DCHECK_LT(static_cast<size_t>(index), addresses_.size() + kNbHeaderEntries);

  // Special entry when no profiles have been created yet.
  if (addresses_.empty())
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_NO_SAVED_ADDRESS);

  // Always show the "Select" entry at the top, default selection position.
  if (index == 0)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_SELECT);

  // Always show the "Select" entry at the top, default selection position.
  if (index == 1)
    return base::ASCIIToUTF16("---");

  return addresses_[index - kNbHeaderEntries].second;
}

bool AddressComboboxModel::IsItemSeparatorAt(int index) {
  // The only separator is between the "Select" entry at 0 and the first address
  // at index 2. So there must be at least one address for a separator to be
  // shown.
  DCHECK(index <= kNbHeaderEntries || !addresses_.empty());
  return index == 1;
}

int AddressComboboxModel::GetDefaultIndex() const {
  if (!default_selected_guid_.empty()) {
    int address_index = GetIndexOfIdentifier(default_selected_guid_);
    if (address_index != -1)
      return address_index;
  }
  return ui::ComboboxModel::GetDefaultIndex();
}

void AddressComboboxModel::AddObserver(ui::ComboboxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AddressComboboxModel::RemoveObserver(ui::ComboboxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

int AddressComboboxModel::AddNewProfile(const AutofillProfile& profile) {
  profiles_cache_.push_back(std::make_unique<AutofillProfile>(profile));
  UpdateAddresses();
  DCHECK_GT(addresses_.size(), 0UL);
  return addresses_.size() + kNbHeaderEntries - 1;
}

std::string AddressComboboxModel::GetItemIdentifierAt(int index) {
  // The first two indices are special entries, with no addresses.
  if (index < kNbHeaderEntries)
    return std::string();
  DCHECK_LT(static_cast<size_t>(index), addresses_.size() + kNbHeaderEntries);
  return addresses_[index - kNbHeaderEntries].first;
}

int AddressComboboxModel::GetIndexOfIdentifier(
    const std::string& identifier) const {
  for (size_t i = 0; i < addresses_.size(); ++i) {
    if (addresses_[i].first == identifier)
      return i + kNbHeaderEntries;
  }
  return -1;
}

void AddressComboboxModel::UpdateAddresses() {
  addresses_.clear();
  std::vector<base::string16> labels;
  // CreateDifferentiatingLabels is expecting a pointer vector and we keep
  // profiles as unique_ptr.
  std::vector<AutofillProfile*> profiles;
  for (const auto& profile : profiles_cache_) {
    profiles.push_back(profile.get());
  }
  AutofillProfile::CreateDifferentiatingLabels(profiles, app_locale_, &labels);
  DCHECK_EQ(labels.size(), profiles_cache_.size());

  for (size_t i = 0; i < profiles_cache_.size(); ++i)
    addresses_.emplace_back(profiles_cache_[i]->guid(), labels[i]);

  for (auto& observer : observers_) {
    observer.OnComboboxModelChanged(this);
  }
}
}  // namespace autofill
