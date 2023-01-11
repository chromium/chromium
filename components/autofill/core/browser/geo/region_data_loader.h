// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_REGION_DATA_LOADER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_REGION_DATA_LOADER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace i18n {
namespace addressinput {
class RegionData;
}  // namespace addressinput
}  // namespace i18n

namespace autofill {

// An interface to wrap the loading of region data so it can be mocked in tests.
class RegionDataLoader {
 public:
  // The signature of the function to be called when the region data is loaded.
  // When the loading request times out or other failure occure, |regions| is
  // empty.
  typedef base::RepeatingCallback<void(
      const std::vector<const ::i18n::addressinput::RegionData*>& regions)>
      RegionDataLoaded;

  virtual ~RegionDataLoader() = default;
  // Calls |loaded_callback| when the region data for |country_code| is ready.
  // This may happen synchronously.
  virtual void LoadRegionData(const std::string& country_code,
                              RegionDataLoaded callback) = 0;
  // To forget about the |callback| givent to LoadRegionData, in cases where
  // callback owner is destroyed before loader.
  virtual void ClearCallback() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_REGION_DATA_LOADER_H_
