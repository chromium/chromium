// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font_data/font_data_service_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace font_data_service {

namespace {

class FontDataServiceImplUnitTest : public testing::Test {
 protected:
  FontDataServiceImplUnitTest()
      : receiver_(&impl_, font_service_.BindNewPipeAndPassReceiver()) {}
  ~FontDataServiceImplUnitTest() override = default;

  base::test::SingleThreadTaskEnvironment environment_;
  mojo::Remote<mojom::FontDataService> font_service_;
  FontDataServiceImpl impl_;
  mojo::Receiver<mojom::FontDataService> receiver_;
};

mojom::TypefaceStylePtr CreateTypefaceStyle(int weight,
                                            int width,
                                            mojom::TypefaceSlant slant) {
  mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
  style->weight = weight;
  style->width = width;
  style->slant = slant;
  return style;
}

TEST_F(FontDataServiceImplUnitTest, MatchFamilyName) {
  mojom::MatchFamilyNameResultPtr out_result;
  std::string family_name = "Segoe UI";
  EXPECT_TRUE(impl_.GetCacheSizeForTesting() == 0);

  // There should be one entry added to the cache.
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_TRUE(impl_.GetCacheSizeForTesting() == 1);

  // Call with the same family name and style. Cache should stay the same
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_TRUE(impl_.GetCacheSizeForTesting() == 1);

  // Call with a different family name. Cache should increase.
  font_service_->MatchFamilyName(
      "Bahnschrift", CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_TRUE(impl_.GetCacheSizeForTesting() == 2);

  // Call with a different font style. Cache should increase.
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(600, 5, mojom::TypefaceSlant::kOblique),
      &out_result);
  EXPECT_TRUE(impl_.GetCacheSizeForTesting() == 3);
}

}  // namespace

}  // namespace font_data_service
