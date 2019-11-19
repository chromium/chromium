// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_TEST_REGION_DATA_LOADER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_TEST_REGION_DATA_LOADER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "components/autofill/core/browser/geo/region_data_loader.h"

namespace autofill {

class TestRegionDataLoader : public RegionDataLoader {
 public:
  TestRegionDataLoader();
  ~TestRegionDataLoader() override;

  // RegionDataLoader.
  void LoadRegionData(const std::string& country_code,
                      autofill::RegionDataLoader::RegionDataLoaded callback,
                      int64_t timeout_ms) override;
  void ClearCallback() override;

  std::string country_code() { return country_code_; }
  const autofill::RegionDataLoader::RegionDataLoaded& callback() {
    return callback_;
  }
  void set_synchronous_callback(bool synchronous_callback) {
    synchronous_callback_ = synchronous_callback;
  }

  void SetRegionData(
      std::vector<std::pair<std::string, std::string>>& regions) {
    regions_ = regions;
  }

  void SendAsynchronousData(
      const std::vector<std::pair<std::string, std::string>>& regions);

 private:
  void SendRegionData(
      const std::vector<std::pair<std::string, std::string>>& regions,
      autofill::RegionDataLoader::RegionDataLoaded callback);

  std::vector<std::pair<std::string, std::string>> regions_;
  std::string country_code_;
  autofill::RegionDataLoader::RegionDataLoaded callback_;
  bool synchronous_callback_{false};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_GEO_TEST_REGION_DATA_LOADER_H_
