// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager.h"
#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace settings {

class ChangePictureHandlerTest : public testing::Test {
 public:
  ChangePictureHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        user_manager_enabler_(std::make_unique<ash::FakeChromeUserManager>()) {}
  ~ChangePictureHandlerTest() override = default;

  void SetUp() override {
    audio::SoundsManager::Create(content::GetAudioServiceStreamFactoryBinder());

    ASSERT_TRUE(profile_manager_.SetUp());
    account_id_ = AccountId::FromUserEmail("lala@example.com");

    user_manager::User* user = GetFakeUserManager()->AddUser(account_id_);

    testing_profile_ =
        profile_manager_.CreateTestingProfile(account_id_.GetUserEmail());
    ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                            testing_profile_);

    // Note that user profiles are created after user login in reality.
    GetFakeUserManager()->LoginUser(account_id_);
    GetFakeUserManager()->UserLoggedIn(account_id_, user->username_hash(),
                                       /*browser_restart=*/false,
                                       /*is_child=*/false);

    web_ui_ = std::make_unique<content::TestWebUI>();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(testing_profile_));
    web_ui_->set_web_contents(web_contents_.get());

    handler_ = std::make_unique<ChangePictureHandler>();
    handler_->set_web_ui(web_ui_.get());
    handler_->AllowJavascript();
    handler_->RegisterMessages();

    request_ = handler_.get();
  }

  void TearDown() override {
    request_ = nullptr;
    handler_.reset();
    web_contents_.reset();
    web_ui_.reset();
    GetFakeUserManager()->Shutdown();
    testing_profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
    audio::SoundsManager::Shutdown();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  void SelectNewDefaultImage(int default_image_index) {
    base::ListValue args;
    args.Append(
        default_user_image::GetDefaultImageUrl(default_image_index).spec());
    args.Append("default");

    web_ui_->HandleReceivedMessage("selectImage", &args);
  }

  void SelectProfileImage() {
    base::ListValue args;
    args.Append("empty url");
    args.Append("profile");

    web_ui_->HandleReceivedMessage("selectImage", &args);
  }

  void SelectImageFromFile(const base::FilePath& path) {
    handler_->FileSelected(path);
  }

  void CancelFileSelection() { handler_->FileSelectionCanceled(); }

  void OnCameraImageDecoded() {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);

    std::vector<unsigned char> data;
    data.push_back('a');
    handler_->user_photo_data_ = base::RefCountedBytes::TakeVector(&data);

    request_->OnImageDecoded(bitmap);
  }

  ash::UserImageManager* GetUserImageManager() {
    return GetFakeUserManager()->GetUserImageManager(account_id_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ChangePictureHandler> handler_;
  base::HistogramTester histogram_tester_;
  AccountId account_id_;
  TestingProfile* testing_profile_;
  TestingProfileManager profile_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  ImageDecoder::ImageRequest* request_;
};

TEST_F(ChangePictureHandlerTest,
       ShouldSendUmaMetricWhenNewDefaultImageIsSelected) {
  const int default_image_index =
      default_user_image::GetRandomDefaultImageIndex();
  SelectNewDefaultImage(default_image_index);

  auto* user_image_manager = GetUserImageManager();

  histogram_tester().ExpectBucketCount(
      ash::UserImageManager::kUserImageChangedHistogramName,
      user_image_manager->ImageIndexToHistogramIndex(default_image_index), 1);
}

TEST_F(ChangePictureHandlerTest,
       ShouldNotSendUmaMetricWhenDefaultImageIsReselected) {
  const int default_image_index =
      default_user_image::GetRandomDefaultImageIndex();
  auto* user_image_manager = GetUserImageManager();

  SelectNewDefaultImage(default_image_index);
  histogram_tester().ExpectBucketCount(
      ash::UserImageManager::kUserImageChangedHistogramName,
      user_image_manager->ImageIndexToHistogramIndex(default_image_index), 1);

  // Selecting the same default image should not log another impression.
  SelectNewDefaultImage(default_image_index);
  histogram_tester().ExpectBucketCount(
      ash::UserImageManager::kUserImageChangedHistogramName,
      user_image_manager->ImageIndexToHistogramIndex(default_image_index), 1);
}

TEST_F(ChangePictureHandlerTest, ShoulSendUmaMetricWhenProfileImageIsSelected) {
  const int default_image_index =
      default_user_image::GetRandomDefaultImageIndex();
  auto* user_image_manager = GetUserImageManager();

  // User selects a new default image.
  SelectNewDefaultImage(default_image_index);
  histogram_tester().ExpectBucketCount(
      ash::UserImageManager::kUserImageChangedHistogramName,
      user_image_manager->ImageIndexToHistogramIndex(default_image_index), 1);

  // User selects the profile image.
  SelectProfileImage();
  histogram_tester().ExpectBucketCount(
      ash::UserImageManager::kUserImageChangedHistogramName,
      user_image_manager->ImageIndexToHistogramIndex(
          user_manager::User::USER_IMAGE_PROFILE),
      1);
}

TEST_F(ChangePictureHandlerTest,
       ShoulNotSendUmaMetricWhenProfileImageIsReselected) {
  auto* user_image_manager = GetUserImageManager();
  // User has profile image by default, thus reselecting profile does not log an
  // impression
  SelectProfileImage();
  histogram_tester().ExpectBucketCount(
      ash::UserImageManager::kUserImageChangedHistogramName,
      user_image_manager->ImageIndexToHistogramIndex(
          user_manager::User::USER_IMAGE_PROFILE),
      0);
}

TEST_F(ChangePictureHandlerTest,
       ShouldSendUmaMetricWhenImageIsSelectedFromFile) {
  auto* user_image_manager = GetUserImageManager();

  const base::FilePath base_file_path("/this/is/a/test/directory/Base Name");
  const base::FilePath dir_path = base_file_path.AppendASCII("dir1");
  const base::FilePath file_path = dir_path.AppendASCII("file1.txt");
  SelectImageFromFile(file_path);

  histogram_tester().ExpectBucketCount(
      ash::UserImageManager::kUserImageChangedHistogramName,
      user_image_manager->ImageIndexToHistogramIndex(
          user_manager::User::USER_IMAGE_EXTERNAL),
      1);
}

TEST_F(ChangePictureHandlerTest, ShouldSendUmaMetricWhenCameraImageIsDecoded) {
  // Camera image is decoded
  OnCameraImageDecoded();
  histogram_tester().ExpectBucketCount(
      ash::UserImageManager::kUserImageChangedHistogramName,
      default_user_image::kHistogramImageFromCamera, 1);
}

TEST_F(ChangePictureHandlerTest,
       ShouldSelectTheCurrentUserImageIfFileSelectionIsCanceled) {
  // keep the current call size so we can check what happened after our test
  // method call.
  auto number_of_calls_before_cancel = web_ui()->call_data().size();
  CancelFileSelection();
  // reset back to previous profile image.
  EXPECT_EQ(web_ui()
                ->call_data()
                .at(number_of_calls_before_cancel)
                ->arg1()
                ->GetString(),
            "profile-image-changed");
}

}  // namespace settings
}  // namespace chromeos
