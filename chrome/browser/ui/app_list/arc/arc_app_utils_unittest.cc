// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestPackage[] = "com.test.app";
constexpr char kTestActivity[] = "com.test.app.some.activity";
constexpr char kTestShelfGroupId[] = "some_shelf_group";
constexpr char kIntentWithShelfGroupId[] =
    "#Intent;launchFlags=0x18001000;"
    "component=com.test.app/com.test.app.some.activity;"
    "S.org.chromium.arc.shelf_group_id=some_shelf_group;end";

std::string GetPlayStoreInitialLaunchIntent() {
  return arc::GetLaunchIntent(arc::kPlayStorePackage, arc::kPlayStoreActivity,
                              {arc::kInitialStartParam});
}

void GetAndroidIdBlocking(bool* out_ok, int64_t* out_android_id) {
  base::RunLoop run_loop;
  arc::GetAndroidId(base::BindOnce(
      [](base::OnceClosure closure, bool* out_ok, int64_t* out_android_id,
         bool ok, int64_t result) {
        *out_ok = ok;
        *out_android_id = result;
        std::move(closure).Run();
      },
      run_loop.QuitClosure(), out_ok, out_android_id));
  run_loop.Run();
}

}  // namespace

using ArcAppUtilsTest = testing::Test;

TEST_F(ArcAppUtilsTest, LaunchIntent) {
  const std::string launch_intent = GetPlayStoreInitialLaunchIntent();

  arc::Intent intent1;
  EXPECT_TRUE(arc::ParseIntent(launch_intent, &intent1));
  EXPECT_EQ(intent1.action(), "android.intent.action.MAIN");
  EXPECT_EQ(intent1.category(), "android.intent.category.LAUNCHER");
  EXPECT_EQ(intent1.package_name(), arc::kPlayStorePackage);
  EXPECT_EQ(intent1.activity(), arc::kPlayStoreActivity);
  EXPECT_EQ(intent1.launch_flags(),
            arc::Intent::FLAG_ACTIVITY_NEW_TASK |
                arc::Intent::FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
  ASSERT_EQ(intent1.extra_params().size(), 1U);
  EXPECT_TRUE(intent1.HasExtraParam(arc::kInitialStartParam));

  arc::Intent intent2;
  EXPECT_TRUE(arc::ParseIntent(kIntentWithShelfGroupId, &intent2));
  EXPECT_EQ(intent2.action(), "");
  EXPECT_EQ(intent2.category(), "");
  EXPECT_EQ(intent2.package_name(), kTestPackage);
  EXPECT_EQ(intent2.activity(), kTestActivity);
  EXPECT_EQ(intent2.launch_flags(),
            arc::Intent::FLAG_ACTIVITY_NEW_TASK |
                arc::Intent::FLAG_RECEIVER_NO_ABORT |
                arc::Intent::FLAG_ACTIVITY_LAUNCH_ADJACENT);
  ASSERT_EQ(intent2.extra_params().size(), 1U);
  EXPECT_TRUE(intent2.HasExtraParam(
      "S.org.chromium.arc.shelf_group_id=some_shelf_group"));
}

TEST_F(ArcAppUtilsTest, ShelfGroupId) {
  const std::string intent_with_shelf_group_id(kIntentWithShelfGroupId);
  const std::string shelf_app_id =
      ArcAppListPrefs::GetAppId(kTestPackage, kTestActivity);
  arc::ArcAppShelfId shelf_id1 = arc::ArcAppShelfId::FromIntentAndAppId(
      intent_with_shelf_group_id, shelf_app_id);
  EXPECT_TRUE(shelf_id1.has_shelf_group_id());
  EXPECT_EQ(shelf_id1.shelf_group_id(), kTestShelfGroupId);
  EXPECT_EQ(shelf_id1.app_id(), shelf_app_id);

  arc::ArcAppShelfId shelf_id2 = arc::ArcAppShelfId::FromIntentAndAppId(
      GetPlayStoreInitialLaunchIntent(), arc::kPlayStoreAppId);
  EXPECT_FALSE(shelf_id2.has_shelf_group_id());
  EXPECT_EQ(shelf_id2.app_id(), arc::kPlayStoreAppId);
}

// Tests that IsArcItem does not crash or DCHECK with invalid crx file ids.
TEST_F(ArcAppUtilsTest, IsArcItemDoesNotCrashWithInvalidCrxFileIds) {
  // TestingProfile checks CurrentlyOn(cotnent::BrowserThread::UI).
  content::BrowserTaskEnvironment task_environment;
  TestingProfile testing_profile;
  EXPECT_FALSE(arc::IsArcItem(&testing_profile, std::string()));
  EXPECT_FALSE(arc::IsArcItem(&testing_profile, "ShelfWindowWatcher0"));
}

TEST_F(ArcAppUtilsTest, GetAndroidId) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile testing_profile;

  // ARC++ is not running.
  bool ok = true;
  int64_t android_id = -1;
  GetAndroidIdBlocking(&ok, &android_id);
  EXPECT_FALSE(ok);
  EXPECT_EQ(0, android_id);

  ArcAppTest arc_app_test_;
  arc_app_test_.SetUp(&testing_profile);

  constexpr int64_t kAndroidIdForTest = 1000;
  arc_app_test_.app_instance()->set_android_id(kAndroidIdForTest);
  GetAndroidIdBlocking(&ok, &android_id);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kAndroidIdForTest, android_id);

  arc_app_test_.TearDown();
}
