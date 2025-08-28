// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/icons/icon_masker.h"

#include <string>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/test/sk_gmock_support.h"

namespace web_app {

namespace {

const base::FilePath kInputIcon{
    FILE_PATH_LITERAL("chrome/test/data/web_apps/input_icon_for_masking.png")};
const base::FilePath kMaskedMacIcon{
    FILE_PATH_LITERAL("chrome/test/data/web_apps/golden_masked_icon_mac.png")};
const base::FilePath kMaskedChromeOsIcon{FILE_PATH_LITERAL(
    "chrome/test/data/web_apps/golden_masked_icon_chromeos.png")};

SkBitmap LoadTestPNG(const base::FilePath& path) {
  base::FilePath data_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_root);
  base::FilePath image_path = data_root.Append(path);
  std::optional<std::vector<uint8_t>> png_data =
      base::ReadFileToBytes(image_path);
  CHECK(png_data.has_value());
  return gfx::Image::CreateFrom1xPNGBytes(base::as_byte_span(*png_data))
      .AsBitmap();
}

base::FilePath GetExpectedIconsFilePath() {
#if BUILDFLAG(IS_MAC)
  return kMaskedMacIcon;
#elif BUILDFLAG(IS_CHROMEOS)
  return kMaskedChromeOsIcon;
#else
  return kInputIcon;
#endif
}

TEST(IconMaskingTest, Basic) {
  base::test::TaskEnvironment task_environment;
  const SkBitmap input_bitmap = LoadTestPNG(kInputIcon);

  base::test::TestFuture<SkBitmap> bitmap_future;
  MaskIconOnOs(input_bitmap, bitmap_future.GetCallback());
  EXPECT_TRUE(bitmap_future.Wait(base::RunLoop::Type::kNestableTasksAllowed));

  SkBitmap masked_bitmap = bitmap_future.Get();
  EXPECT_THAT(masked_bitmap, gfx::test::IsCloseToBitmap(
                                 LoadTestPNG(GetExpectedIconsFilePath()),
                                 /*max_per_channel_deviation=*/2));
}

}  // namespace

}  // namespace web_app
