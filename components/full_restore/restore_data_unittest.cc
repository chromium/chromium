// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/restore_data.h"

#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/app_restore_data.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace full_restore {

namespace {

const char kAppId1[] = "aaa";
const char kAppId2[] = "bbb";

const int32_t kId1 = 100;
const int32_t kId2 = 200;
const int32_t kId3 = 300;

const int64_t kDisplayId1 = 22000000;
const int64_t kDisplayId2 = 11000000;

const char kFilePath1[] = "path1";
const char kFilePath2[] = "path2";

const char kIntentActionView[] = "view";
const char kIntentActionSend[] = "send";

const char kMimeType[] = "text/plain";

const char kShareText1[] = "text1";
const char kShareText2[] = "text2";

}  // namespace

// Unit tests for restore data.
class RestoreDataTest : public testing::Test {
 public:
  RestoreDataTest() = default;
  ~RestoreDataTest() override = default;

  RestoreDataTest(const RestoreDataTest&) = delete;
  RestoreDataTest& operator=(const RestoreDataTest&) = delete;

  apps::mojom::IntentPtr CreateIntent(const std::string& action,
                                      const std::string& mime_type,
                                      const std::string& share_text) {
    auto intent = apps::mojom::Intent::New();
    intent->action = action;
    intent->mime_type = mime_type;
    intent->share_text = share_text;
    return intent;
  }

  void VerifyAppRestoreData(const std::unique_ptr<AppRestoreData>& data,
                            apps::mojom::LaunchContainer container,
                            WindowOpenDisposition disposition,
                            int64_t display_id,
                            std::vector<base::FilePath> file_paths,
                            apps::mojom::IntentPtr intent) {
    EXPECT_TRUE(data->container.has_value());
    EXPECT_EQ(static_cast<int>(container), data->container.value());

    EXPECT_TRUE(data->disposition.has_value());
    EXPECT_EQ(static_cast<int>(disposition), data->disposition.value());

    EXPECT_TRUE(data->display_id.has_value());
    EXPECT_EQ(display_id, data->display_id.value());

    EXPECT_TRUE(data->file_paths.has_value());
    EXPECT_EQ(file_paths.size(), data->file_paths.value().size());
    for (size_t i = 0; i < file_paths.size(); i++)
      EXPECT_EQ(file_paths[i], data->file_paths.value()[i]);

    EXPECT_TRUE(data->intent.has_value());
    EXPECT_EQ(intent->action, data->intent.value()->action);
    EXPECT_EQ(intent->mime_type, data->intent.value()->mime_type);
    EXPECT_EQ(intent->share_text, data->intent.value()->share_text);
  }

  RestoreData& restore_data() { return restore_data_; }

  const RestoreData::AppIdToLaunchList& app_id_to_launch_list() {
    return restore_data_.app_id_to_launch_list();
  }

 private:
  RestoreData restore_data_;
};

TEST_F(RestoreDataTest, AddNullAppLaunchInfo) {
  restore_data().AddAppLaunchInfo(nullptr);
  EXPECT_TRUE(app_id_to_launch_list().empty());
}

TEST_F(RestoreDataTest, AddAppLaunchInfos) {
  std::unique_ptr<AppLaunchInfo> app_launch_info1 =
      std::make_unique<AppLaunchInfo>(
          kAppId1, kId1, apps::mojom::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
          std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                      base::FilePath(kFilePath2)},
          CreateIntent(kIntentActionSend, kMimeType, kShareText1));

  std::unique_ptr<AppLaunchInfo> app_launch_info2 =
      std::make_unique<AppLaunchInfo>(
          kAppId1, kId2, apps::mojom::LaunchContainer::kLaunchContainerTab,
          WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId2,
          std::vector<base::FilePath>{base::FilePath(kFilePath2)},
          CreateIntent(kIntentActionView, kMimeType, kShareText2));

  std::unique_ptr<AppLaunchInfo> app_launch_info3 =
      std::make_unique<AppLaunchInfo>(
          kAppId2, kId3, apps::mojom::LaunchContainer::kLaunchContainerNone,
          WindowOpenDisposition::NEW_POPUP, kDisplayId2,
          std::vector<base::FilePath>{base::FilePath(kFilePath1)},
          CreateIntent(kIntentActionView, kMimeType, kShareText1));

  restore_data().AddAppLaunchInfo(std::move(app_launch_info1));
  restore_data().AddAppLaunchInfo(std::move(app_launch_info2));
  restore_data().AddAppLaunchInfo(std::move(app_launch_info3));

  EXPECT_EQ(2u, app_id_to_launch_list().size());

  // Verify for |kAppId1|
  const auto launch_list_it1 = app_id_to_launch_list().find(kAppId1);
  EXPECT_TRUE(launch_list_it1 != app_id_to_launch_list().end());
  EXPECT_EQ(2u, launch_list_it1->second.size());

  const auto app_restore_data_it1 = launch_list_it1->second.find(kId1);
  EXPECT_TRUE(app_restore_data_it1 != launch_list_it1->second.end());

  VerifyAppRestoreData(app_restore_data_it1->second,
                       apps::mojom::LaunchContainer::kLaunchContainerWindow,
                       WindowOpenDisposition::NEW_WINDOW, kDisplayId1,
                       std::vector<base::FilePath>{base::FilePath(kFilePath1),
                                                   base::FilePath(kFilePath2)},
                       CreateIntent(kIntentActionSend, kMimeType, kShareText1));

  const auto app_restore_data_it2 = launch_list_it1->second.find(kId2);
  EXPECT_TRUE(app_restore_data_it2 != launch_list_it1->second.end());
  VerifyAppRestoreData(app_restore_data_it2->second,
                       apps::mojom::LaunchContainer::kLaunchContainerTab,
                       WindowOpenDisposition::NEW_FOREGROUND_TAB, kDisplayId2,
                       std::vector<base::FilePath>{base::FilePath(kFilePath2)},
                       CreateIntent(kIntentActionView, kMimeType, kShareText2));

  // Verify for |kAppId2|
  const auto launch_list_it2 = app_id_to_launch_list().find(kAppId2);
  EXPECT_TRUE(launch_list_it2 != app_id_to_launch_list().end());
  EXPECT_EQ(1u, launch_list_it2->second.size());

  EXPECT_EQ(kId3, launch_list_it2->second.begin()->first);
  VerifyAppRestoreData(launch_list_it2->second.begin()->second,
                       apps::mojom::LaunchContainer::kLaunchContainerNone,
                       WindowOpenDisposition::NEW_POPUP, kDisplayId2,
                       std::vector<base::FilePath>{base::FilePath(kFilePath1)},
                       CreateIntent(kIntentActionView, kMimeType, kShareText1));
}

}  // namespace full_restore
