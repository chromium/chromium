// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/icons/icon_masker.h"

#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
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
  const SkBitmap input_bitmap =
      web_app::test::LoadTestImageFromDisk(kInputIcon).AsBitmap();

  base::test::TestFuture<SkBitmap> bitmap_future;
  MaskIconOnOs(input_bitmap, bitmap_future.GetCallback());
  EXPECT_TRUE(bitmap_future.Wait(base::RunLoop::Type::kNestableTasksAllowed));

  SkBitmap masked_bitmap = bitmap_future.Get();
  EXPECT_THAT(masked_bitmap,
              gfx::test::IsCloseToBitmap(web_app::test::LoadTestImageFromDisk(
                                             GetExpectedIconsFilePath())
                                             .AsBitmap(),
                                         /*max_per_channel_deviation=*/2));
}

}  // namespace

}  // namespace web_app
