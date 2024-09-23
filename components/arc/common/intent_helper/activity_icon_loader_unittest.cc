// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/intent_helper/activity_icon_loader.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/arc/common/intent_helper/adaptive_icon_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace arc {
namespace internal {
namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
using RawIconPngDataPtr = mojom::RawIconPngDataPtr;
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
using RawIconPngDataPtr = crosapi::mojom::RawIconPngDataPtr;
#endif

void OnIconsReady0(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons) {
  EXPECT_EQ(3U, activity_to_icons->size());
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p0", "a0")));
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p1", "a0")));
}

void OnIconsReady1(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons) {
  EXPECT_EQ(1U, activity_to_icons->size());
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
}

void OnIconsReady2(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons) {
  EXPECT_TRUE(activity_to_icons->empty());
}

void OnIconsReady3(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons) {
  EXPECT_EQ(2U, activity_to_icons->size());
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
  EXPECT_EQ(1U, activity_to_icons->count(
                    ActivityIconLoader::ActivityName("p2", "a2")));
}

class FakeAdaptiveIconDelegate : public AdaptiveIconDelegate {
 public:
  FakeAdaptiveIconDelegate() = default;
  ~FakeAdaptiveIconDelegate() override = default;

  FakeAdaptiveIconDelegate(const FakeAdaptiveIconDelegate&) = delete;
  FakeAdaptiveIconDelegate& operator=(const FakeAdaptiveIconDelegate&) = delete;

  void GenerateAdaptiveIcons(
      const std::vector<ActivityIconLoader::ActivityIconPtr>& icons,
      AdaptiveIconDelegateCallback callback) override {
    ++count_;
    std::vector<gfx::ImageSkia> result;
    for (const auto& icon : icons) {
      if (icon && icon->icon_png_data &&
          icon->icon_png_data->is_adaptive_icon &&
          icon->icon_png_data->foreground_icon_png_data) {
        auto png_data(icon->icon_png_data->foreground_icon_png_data.value());
        png_data_.emplace_back(std::string(png_data.begin(), png_data.end()));
        result.emplace_back(
            gfx::ImageSkia(gfx::ImageSkiaRep(gfx::Size(20, 20), 0.0f)));
      }
    }
    std::move(callback).Run(std::move(result));
  }

  std::vector<std::string> png_data() { return png_data_; }
  int count() { return count_; }

 private:
  int count_ = 0;
  std::vector<std::string> png_data_;
};

// Tests if InvalidateIcons properly cleans up the cache.
TEST(ActivityIconLoaderTest, TestInvalidateIcons) {
  ActivityIconLoader loader;
  loader.AddCacheEntryForTesting(ActivityIconLoader::ActivityName("p0", "a0"));
  loader.AddCacheEntryForTesting(ActivityIconLoader::ActivityName("p0", "a1"));
  loader.AddCacheEntryForTesting(ActivityIconLoader::ActivityName("p1", "a0"));
  EXPECT_EQ(3U, loader.cached_icons_for_testing().size());

  loader.InvalidateIcons("p2");  // delete none.
  EXPECT_EQ(3U, loader.cached_icons_for_testing().size());

  loader.InvalidateIcons("p0");  // delete 2 entries.
  EXPECT_EQ(1U, loader.cached_icons_for_testing().size());

  loader.InvalidateIcons("p2");  // delete none.
  EXPECT_EQ(1U, loader.cached_icons_for_testing().size());

  loader.InvalidateIcons("p1");  // delete 1 entry.
  EXPECT_EQ(0U, loader.cached_icons_for_testing().size());
}

// Tests if GetActivityIcons immediately returns cached icons.
TEST(ActivityIconLoaderTest, TestGetActivityIcons) {
  ActivityIconLoader loader;
  loader.AddCacheEntryForTesting(ActivityIconLoader::ActivityName("p0", "a0"));
  loader.AddCacheEntryForTesting(ActivityIconLoader::ActivityName("p1", "a1"));
  loader.AddCacheEntryForTesting(ActivityIconLoader::ActivityName("p1", "a0"));

  // Check that GetActivityIcons() immediately calls OnIconsReady0() with all
  // locally cached icons.
  std::vector<ActivityIconLoader::ActivityName> activities;
  activities.emplace_back("p0", "a0");
  activities.emplace_back("p1", "a1");
  activities.emplace_back("p1", "a0");
  EXPECT_EQ(
      ActivityIconLoader::GetResult::SUCCEEDED_SYNC,
      loader.GetActivityIcons(activities, base::BindOnce(&OnIconsReady0)));

  // Test with different |activities|.
  activities.clear();
  activities.emplace_back("p1", "a1");
  EXPECT_EQ(
      ActivityIconLoader::GetResult::SUCCEEDED_SYNC,
      loader.GetActivityIcons(activities, base::BindOnce(&OnIconsReady1)));
  activities.clear();
  EXPECT_EQ(
      ActivityIconLoader::GetResult::SUCCEEDED_SYNC,
      loader.GetActivityIcons(activities, base::BindOnce(&OnIconsReady2)));
  activities.emplace_back("p1", "a_unknown");
  EXPECT_EQ(
      ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED,
      loader.GetActivityIcons(activities, base::BindOnce(&OnIconsReady2)));
}

// Tests if OnIconsResized updates the cache.
TEST(ActivityIconLoaderTest, TestOnIconsResized) {
  std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons(
      new ActivityIconLoader::ActivityToIconsMap);
  activity_to_icons->insert(std::make_pair(
      ActivityIconLoader::ActivityName("p0", "a0"),
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));
  activity_to_icons->insert(std::make_pair(
      ActivityIconLoader::ActivityName("p1", "a1"),
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));
  activity_to_icons->insert(std::make_pair(
      ActivityIconLoader::ActivityName("p1", "a0"),
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));
  // Duplicated entey which should be ignored.
  activity_to_icons->insert(std::make_pair(
      ActivityIconLoader::ActivityName("p0", "a0"),
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));

  ActivityIconLoader loader;

  // Call OnIconsResized() and check that the cache is properly updated.
  loader.OnIconsResizedForTesting(base::BindOnce(&OnIconsReady0),
                                  std::move(activity_to_icons));
  EXPECT_EQ(3U, loader.cached_icons_for_testing().size());
  EXPECT_EQ(1U, loader.cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p0", "a0")));
  EXPECT_EQ(1U, loader.cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
  EXPECT_EQ(1U, loader.cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p1", "a0")));

  // Call OnIconsResized() again to make sure that the second call does not
  // remove the cache the previous call added.
  activity_to_icons =
      std::make_unique<ActivityIconLoader::ActivityToIconsMap>();
  // Duplicated entry.
  activity_to_icons->insert(std::make_pair(
      ActivityIconLoader::ActivityName("p1", "a1"),
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));
  // New entry.
  activity_to_icons->insert(std::make_pair(
      ActivityIconLoader::ActivityName("p2", "a2"),
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));
  loader.OnIconsResizedForTesting(base::BindOnce(&OnIconsReady3),
                                  std::move(activity_to_icons));
  EXPECT_EQ(4U, loader.cached_icons_for_testing().size());
  EXPECT_EQ(1U, loader.cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p0", "a0")));
  EXPECT_EQ(1U, loader.cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p1", "a1")));
  EXPECT_EQ(1U, loader.cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p1", "a0")));
  EXPECT_EQ(1U, loader.cached_icons_for_testing().count(
                    ActivityIconLoader::ActivityName("p2", "a2")));
}

class ActivityIconLoaderOnIconsReadyTest : public ::testing::Test {
 public:
  ActivityIconLoaderOnIconsReadyTest() = default;
  ~ActivityIconLoaderOnIconsReadyTest() override = default;

  ActivityIconLoaderOnIconsReadyTest(
      const ActivityIconLoaderOnIconsReadyTest&) = delete;
  ActivityIconLoaderOnIconsReadyTest& operator=(
      const ActivityIconLoaderOnIconsReadyTest&) = delete;

  void OnIconsReady(std::unique_ptr<ActivityIconLoader::ActivityToIconsMap>
                        activity_to_icons) {
    EXPECT_EQ(4U, activity_to_icons->size());
    EXPECT_EQ(1U, activity_to_icons->count(
                      ActivityIconLoader::ActivityName("p0", "a0")));
    EXPECT_EQ(1U, activity_to_icons->count(
                      ActivityIconLoader::ActivityName("p1", "a1")));
    EXPECT_EQ(1U, activity_to_icons->count(
                      ActivityIconLoader::ActivityName("p2", "a2")));
    EXPECT_EQ(1U, activity_to_icons->count(
                      ActivityIconLoader::ActivityName("p3", "a3")));
    if (!on_icon_ready_callback_.is_null()) {
      std::move(on_icon_ready_callback_).Run();
    }
  }

  void WaitForIconReady() {
    base::RunLoop run_loop;
    on_icon_ready_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::OnceClosure on_icon_ready_callback_;
  base::test::TaskEnvironment task_environment_;

  base::WeakPtrFactory<ActivityIconLoaderOnIconsReadyTest> weak_ptr_factory_{
      this};
};

// Tests OnIconsReady with a delegate.
TEST_F(ActivityIconLoaderOnIconsReadyTest, TestWithDelegate) {
  ActivityIconLoader loader;
  std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons(
      new ActivityIconLoader::ActivityToIconsMap);
  auto activity_name0 = ActivityIconLoader::ActivityName("p0", "a0");
  auto activity_name1 = ActivityIconLoader::ActivityName("p1", "a1");
  loader.AddCacheEntryForTesting(activity_name0);
  loader.AddCacheEntryForTesting(activity_name1);
  activity_to_icons->insert(std::make_pair(
      activity_name0,
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));
  activity_to_icons->insert(std::make_pair(
      activity_name1,
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));

  std::vector<ActivityIconLoader::ActivityIconPtr> icons;
  std::string foreground_png_data_as_string0 = "FOREGROUND_ICON_CONTENT_0";
  std::string foreground_png_data_as_string1 = "FOREGROUND_ICON_CONTENT_1";
  icons.emplace_back(ActivityIconLoader::ActivityIconPtr::Struct::New(
      ActivityIconLoader::ActivityNamePtr::Struct::New("p2", "a2"), 32, 32,
      std::vector<uint8_t>(),
      RawIconPngDataPtr::Struct::New(
          true, std::vector<uint8_t>(),
          std::vector<uint8_t>(foreground_png_data_as_string0.begin(),
                               foreground_png_data_as_string0.end()),
          std::vector<uint8_t>())));
  icons.emplace_back(ActivityIconLoader::ActivityIconPtr::Struct::New(
      ActivityIconLoader::ActivityNamePtr::Struct::New("p3", "a3"), 32, 32,
      std::vector<uint8_t>(),
      RawIconPngDataPtr::Struct::New(
          true, std::vector<uint8_t>(),
          std::vector<uint8_t>(foreground_png_data_as_string1.begin(),
                               foreground_png_data_as_string1.end()),
          std::vector<uint8_t>())));

  FakeAdaptiveIconDelegate delegate;
  loader.SetAdaptiveIconDelegate(&delegate);

  // Call OnIconsReady() and check that the cache is properly updated.
  loader.OnIconsReadyForTesting(
      std::move(activity_to_icons),
      base::BindOnce(&ActivityIconLoaderOnIconsReadyTest::OnIconsReady,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(icons));

  EXPECT_EQ(1, delegate.count());
  EXPECT_EQ(2U, delegate.png_data().size());
  EXPECT_EQ(foreground_png_data_as_string0, delegate.png_data()[0]);
  EXPECT_EQ(foreground_png_data_as_string1, delegate.png_data()[1]);
  WaitForIconReady();
}

// Tests OnIconsReady without a delegate.
TEST_F(ActivityIconLoaderOnIconsReadyTest, TestWithoutDelegate) {
  ActivityIconLoader loader;
  std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> activity_to_icons(
      new ActivityIconLoader::ActivityToIconsMap);
  auto activity_name0 = ActivityIconLoader::ActivityName("p0", "a0");
  auto activity_name1 = ActivityIconLoader::ActivityName("p1", "a1");
  loader.AddCacheEntryForTesting(activity_name0);
  loader.AddCacheEntryForTesting(activity_name1);
  activity_to_icons->insert(std::make_pair(
      activity_name0,
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));
  activity_to_icons->insert(std::make_pair(
      activity_name1,
      ActivityIconLoader::Icons(gfx::Image(), gfx::Image(), nullptr)));

  std::vector<ActivityIconLoader::ActivityIconPtr> icons;
  icons.emplace_back(ActivityIconLoader::ActivityIconPtr::Struct::New(
      ActivityIconLoader::ActivityNamePtr::Struct::New("p2", "a2"), 1, 1,
      std::vector<uint8_t>(4), RawIconPngDataPtr::Struct::New()));
  icons.emplace_back(ActivityIconLoader::ActivityIconPtr::Struct::New(
      ActivityIconLoader::ActivityNamePtr::Struct::New("p3", "a3"), 1, 1,
      std::vector<uint8_t>(4), RawIconPngDataPtr::Struct::New()));

  // Call OnIconsReady() and check that the cache is properly updated.
  loader.OnIconsReadyForTesting(
      std::move(activity_to_icons),
      base::BindOnce(&ActivityIconLoaderOnIconsReadyTest::OnIconsReady,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(icons));

  WaitForIconReady();
}

}  // namespace
}  // namespace internal
}  // namespace arc
