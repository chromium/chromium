// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_REGION_COMBOBOX_MODEL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_REGION_COMBOBOX_MODEL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/base/models/combobox_model.h"

namespace i18n {
namespace addressinput {
class RegionData;
}  // namespace addressinput
}  // namespace i18n

namespace autofill {

class RegionDataLoader;

// A model for country regions (aka state/provinces) to be used to enter
// addresses. Note that loading these regions can happen asynchronously so a
// ui::ComboboxModelObserver should be attached to this model to be updated when
// the regions load is completed.
class RegionComboboxModel : public ui::ComboboxModel {
 public:
  RegionComboboxModel();

  RegionComboboxModel(const RegionComboboxModel&) = delete;
  RegionComboboxModel& operator=(const RegionComboboxModel&) = delete;

  ~RegionComboboxModel() override;

  void LoadRegionData(const std::string& country_code,
                      RegionDataLoader* region_data_loader);

  bool IsPendingRegionDataLoad() const {
    return region_data_loader_ != nullptr;
  }

  bool failed_to_load_data() const { return failed_to_load_data_; }

  const std::vector<std::pair<std::string, std::string>>& GetRegions() const {
    return regions_;
  }

  // ui::ComboboxModel implementation:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  bool IsItemSeparatorAt(size_t index) const override;

 private:
  // Callback for the RegionDataLoader.
  void OnRegionDataLoaded(
      const std::vector<const ::i18n::addressinput::RegionData*>& regions);

  // Whether the region data load failed or not.
  bool failed_to_load_data_;

  // Lifespan not owned by RegionComboboxModel, but guaranteed to be alive up to
  // a call to OnRegionDataLoaded where soft ownership must be released.
  raw_ptr<RegionDataLoader> region_data_loader_;

  // List of <code, name> pairs for ADDRESS_HOME_STATE combobox values;
  std::vector<std::pair<std::string, std::string>> regions_;

  // Weak pointer factory.
  base::WeakPtrFactory<RegionComboboxModel> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_REGION_COMBOBOX_MODEL_H_
