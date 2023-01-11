// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/region_combobox_model.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/geo/region_data_loader.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model_observer.h"

namespace autofill {

RegionComboboxModel::RegionComboboxModel()
    : failed_to_load_data_(false), region_data_loader_(nullptr) {}

RegionComboboxModel::~RegionComboboxModel() {
  if (region_data_loader_)
    region_data_loader_->ClearCallback();
}

void RegionComboboxModel::LoadRegionData(const std::string& country_code,
                                         RegionDataLoader* region_data_loader) {
  DCHECK(region_data_loader);
  DCHECK(!region_data_loader_);
  region_data_loader_ = region_data_loader;
  region_data_loader_->LoadRegionData(
      country_code,
      base::BindRepeating(&RegionComboboxModel::OnRegionDataLoaded,
                          weak_factory_.GetWeakPtr()));
}

size_t RegionComboboxModel::GetItemCount() const {
  // The combobox view needs to always have at least one item. If the regions
  // have not been completely loaded yet, we display a single "loading" item.
  return std::max(regions_.size(), size_t{1});
}

std::u16string RegionComboboxModel::GetItemAt(size_t index) const {
  // This might happen because of the asynchronous nature of the data.
  if (index >= regions_.size())
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_LOADING_REGIONS);

  if (!regions_[index].second.empty())
    return base::UTF8ToUTF16(regions_[index].second);

  // The separator item. Implemented for platforms that don't yet support
  // IsItemSeparatorAt().
  return u"---";
}

bool RegionComboboxModel::IsItemSeparatorAt(size_t index) const {
  return index < regions_.size() && regions_[index].first.empty();
}

void RegionComboboxModel::OnRegionDataLoaded(
    const std::vector<const ::i18n::addressinput::RegionData*>& regions) {
  // The RegionDataLoader will eventually self destruct after this call.
  DCHECK(region_data_loader_);
  region_data_loader_ = nullptr;
  regions_.clear();

  // Some countries expose a state field but have no region names available.
  if (regions.size() > 0) {
    failed_to_load_data_ = false;
    regions_.emplace_back("", "---");
    for (auto* const region : regions) {
      regions_.emplace_back(region->key(), region->name());
    }
  } else {
    // TODO(mad): Maybe use a static list as is done for countries in
    // components\autofill\core\browser\country_data.cc
    failed_to_load_data_ = true;
  }

  for (auto& observer : observers()) {
    observer.OnComboboxModelChanged(this);
  }
}

}  // namespace autofill
