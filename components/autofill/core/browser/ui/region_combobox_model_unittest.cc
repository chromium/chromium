// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/region_combobox_model.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/geo/test_region_data_loader.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"

namespace autofill {

// Strings used in more than one place and must be the same everywhere.
const char kQuebecCode[] = "QC";
const char kQuebecName[] = "Quebec";
const char16_t kQuebecName16[] = u"Quebec";
const char kOntarioCode[] = "ON";
const char kOntarioName[] = "Ontario";
const char16_t kOntarioName16[] = u"Ontario";

// Make sure the two regions returned by the source are properly set in the
// model.
TEST(RegionComboboxModelTest, QuebecOntarioRegions) {
  TestRegionDataLoader test_region_data_loader;
  RegionComboboxModel model;
  model.LoadRegionData("", &test_region_data_loader);

  std::vector<std::pair<std::string, std::string>> regions;
  regions.emplace_back(kQuebecCode, kQuebecName);
  regions.emplace_back(kOntarioCode, kOntarioName);

  test_region_data_loader.SendAsynchronousData(regions);

  EXPECT_EQ(3u, model.GetItemCount());
  EXPECT_EQ(u"---", model.GetItemAt(0));
  EXPECT_EQ(kQuebecName16, model.GetItemAt(1));
  EXPECT_EQ(kOntarioName16, model.GetItemAt(2));
  EXPECT_FALSE(model.failed_to_load_data());
}

// Make sure the combo box properly support source emptyness/failures.
TEST(RegionComboboxModelTest, FailingSource) {
  TestRegionDataLoader test_region_data_loader;
  RegionComboboxModel model;
  model.LoadRegionData("", &test_region_data_loader);
  test_region_data_loader.SendAsynchronousData(
      std::vector<std::pair<std::string, std::string>>());

  // There's always 1 item, even in failure cases.
  EXPECT_EQ(1u, model.GetItemCount());
  EXPECT_TRUE(model.failed_to_load_data());
}

}  // namespace autofill
