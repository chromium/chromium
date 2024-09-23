// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_combobox_model.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
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
constexpr size_t kNbHeaderEntries = 2;
}  // namespace

AddressComboboxModel::AddressComboboxModel(
    const PersonalDataManager& personal_data_manager,
    const std::string& app_locale,
    const std::string& default_selected_guid)
    : app_locale_(app_locale), default_selected_guid_(default_selected_guid) {
  for (const auto* profile :
       personal_data_manager.address_data_manager().GetProfilesToSuggest()) {
    profiles_cache_.push_back(std::make_unique<AutofillProfile>(*profile));
  }
  UpdateAddresses();
}

AddressComboboxModel::~AddressComboboxModel() = default;

size_t AddressComboboxModel::GetItemCount() const {
  // When there are not addresses, a special entry is shown to prompt the user
  // to add addresses, but nothing else is shown, since there are no address to
  // select from, and no need for a separator.
  if (addresses_.size() == 0)
    return 1;
  // If there are addresses to choose from, but none is selected, add extra
  // items for the "Select" entry and a separator.
  return addresses_.size() + kNbHeaderEntries;
}

std::u16string AddressComboboxModel::GetItemAt(size_t index) const {
  // A special entry is always added at index 0 and a separator at index 1.
  DCHECK_LT(index, addresses_.size() + kNbHeaderEntries);

  // Special entry when no profiles have been created yet.
  if (addresses_.empty())
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_NO_SAVED_ADDRESS);

  // Always show the "Select" entry at the top, default selection position.
  if (index == 0)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_SELECT);
  if (index == 1)
    return u"---";

  return addresses_[index - kNbHeaderEntries].second;
}

bool AddressComboboxModel::IsItemSeparatorAt(size_t index) const {
  // The only separator is between the "Select" entry at 0 and the first address
  // at index 2. So there must be at least one address for a separator to be
  // shown.
  DCHECK(index <= kNbHeaderEntries || !addresses_.empty());
  return index == 1;
}

std::optional<size_t> AddressComboboxModel::GetDefaultIndex() const {
  if (!default_selected_guid_.empty()) {
    const auto index = GetIndexOfIdentifier(default_selected_guid_);
    if (index.has_value())
      return index;
  }
  return ui::ComboboxModel::GetDefaultIndex();
}

size_t AddressComboboxModel::AddNewProfile(const AutofillProfile& profile) {
  profiles_cache_.push_back(std::make_unique<AutofillProfile>(profile));
  UpdateAddresses();
  DCHECK(!addresses_.empty());
  return addresses_.size() + kNbHeaderEntries - 1;
}

std::string AddressComboboxModel::GetItemIdentifierAt(size_t index) {
  // The first two indices are special entries, with no addresses.
  if (index < kNbHeaderEntries)
    return std::string();
  DCHECK_LT(index, addresses_.size() + kNbHeaderEntries);
  return addresses_[index - kNbHeaderEntries].first;
}

std::optional<size_t> AddressComboboxModel::GetIndexOfIdentifier(
    const std::string& identifier) const {
  for (size_t i = 0; i < addresses_.size(); ++i) {
    if (addresses_[i].first == identifier)
      return i + kNbHeaderEntries;
  }
  return std::nullopt;
}

void AddressComboboxModel::UpdateAddresses() {
  addresses_.clear();
  std::vector<std::u16string> labels;
  // CreateDifferentiatingLabels is expecting a pointer vector and we keep
  // profiles as unique_ptr.
  std::vector<raw_ptr<const AutofillProfile, VectorExperimental>> profiles;
  for (const auto& profile : profiles_cache_) {
    profiles.push_back(profile.get());
  }
  AutofillProfile::CreateDifferentiatingLabels(profiles, app_locale_, &labels);
  DCHECK_EQ(labels.size(), profiles_cache_.size());

  for (size_t i = 0; i < profiles_cache_.size(); ++i)
    addresses_.emplace_back(profiles_cache_[i]->guid(), labels[i]);

  for (auto& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}
}  // namespace autofill
