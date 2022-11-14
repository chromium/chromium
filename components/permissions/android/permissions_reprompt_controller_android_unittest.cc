// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permissions_reprompt_controller_android.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/test_renderer_host.h"

namespace permissions {

class PermissionsRepromptControllerAndroidTest
    : public content::RenderViewHostTestHarness {
 public:
  PermissionsRepromptControllerAndroidTest() = default;
  ~PermissionsRepromptControllerAndroidTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    PermissionsRepromptControllerAndroid::CreateForWebContents(web_contents());
    controller_ =
        PermissionsRepromptControllerAndroid::FromWebContents(web_contents());
    client_ = std::make_unique<RepromptTestPermissionsClient>(controller_);
  }

  void RepromtMissingLocation(base::OnceCallback<void(bool)> callback) {
    controller_->RepromptPermissionRequestInternal(
        {ContentSettingsType::GEOLOCATION}, {ContentSettingsType::GEOLOCATION},
        ContentSettingsType::GEOLOCATION, std::move(callback));
  }

  void RepromtMissingCameraMediaStream(
      base::OnceCallback<void(bool)> callback) {
    controller_->RepromptPermissionRequestInternal(
        {ContentSettingsType::MEDIASTREAM_CAMERA},
        {ContentSettingsType::MEDIASTREAM_CAMERA},
        ContentSettingsType::MEDIASTREAM_CAMERA, std::move(callback));
  }

  void RepromtMissingCameraAR(base::OnceCallback<void(bool)> callback) {
    controller_->RepromptPermissionRequestInternal(
        {ContentSettingsType::MEDIASTREAM_CAMERA},
        {ContentSettingsType::MEDIASTREAM_CAMERA}, ContentSettingsType::AR,
        std::move(callback));
  }

  size_t GetPendingCallbackCount() const {
    size_t callback_count = 0;
    for (const auto& it : controller_->pending_callbacks_) {
      callback_count += it.second.second.size();
    }

    return callback_count;
  }

  size_t GetRepromptCount() const { return client_->reprompt_count(); }

  void WaitForNextReprompting() { client_->WaitForNextReprompting(); }

 private:
  class RepromptTestPermissionsClient : public TestPermissionsClient {
   public:
    explicit RepromptTestPermissionsClient(
        PermissionsRepromptControllerAndroid* controller)
        : controller_(controller) {}
    ~RepromptTestPermissionsClient() override = default;

    RepromptTestPermissionsClient(const RepromptTestPermissionsClient&) =
        delete;
    RepromptTestPermissionsClient& operator=(
        const RepromptTestPermissionsClient&) = delete;

    // Getters
    size_t reprompt_count() const { return reprompt_count_; }

    void WaitForNextReprompting() {
      base::RunLoop run_loop;
      run_loop_ = &run_loop;
      run_loop_->Run();
      run_loop_ = nullptr;
    }

   private:
    void RepromptForAndroidPermissions(
        content::WebContents* web_contents,
        const std::vector<ContentSettingsType>& content_settings_types,
        const std::vector<ContentSettingsType>& filtered_content_settings_types,
        const std::vector<std::string>& required_permissions,
        const std::vector<std::string>& optional_permissions,
        PermissionsUpdatedCallback callback) override {
      ++reprompt_count_;

      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &RepromptTestPermissionsClient::OnRepromptPermissionDone,
              base::Unretained(this), std::move(callback)));
    }

    void OnRepromptPermissionDone(PermissionsUpdatedCallback callback) {
      if (run_loop_)
        run_loop_->Quit();
      std::move(callback).Run(false);
    }

    size_t reprompt_count_ = 0;
    raw_ptr<PermissionsRepromptControllerAndroid> controller_ = nullptr;
    raw_ptr<base::RunLoop> run_loop_ = nullptr;
  };

  raw_ptr<PermissionsRepromptControllerAndroid> controller_;
  std::unique_ptr<RepromptTestPermissionsClient> client_;
};

// Duplicated requests from same contexts. Callback from same context will be
// ignored and no new prompt will be shown
TEST_F(PermissionsRepromptControllerAndroidTest, DuplicatedRequestSameContext) {
  base::MockOnceCallback<void(bool)> mock_callback1;
  base::MockOnceCallback<void(bool)> mock_callback2;
  RepromtMissingLocation(mock_callback1.Get());
  EXPECT_EQ(1u, GetPendingCallbackCount());
  RepromtMissingLocation(mock_callback2.Get());
  EXPECT_EQ(1u, GetPendingCallbackCount());
  EXPECT_CALL(mock_callback1, Run(false));
  EXPECT_CALL(mock_callback2, Run).Times(0);
  WaitForNextReprompting();
  EXPECT_EQ(1u, GetRepromptCount());
  EXPECT_EQ(0u, GetPendingCallbackCount());
}

// Duplicated requests from different contexts. All callbacks are expected to be
// called but only 1 prompt should be shown.
TEST_F(PermissionsRepromptControllerAndroidTest,
       DuplicatedRequestsDifferentContexts) {
  base::MockOnceCallback<void(bool)> mock_callback1;
  base::MockOnceCallback<void(bool)> mock_callback2;
  RepromtMissingCameraMediaStream(mock_callback1.Get());
  EXPECT_EQ(1u, GetPendingCallbackCount());
  RepromtMissingCameraAR(mock_callback2.Get());
  EXPECT_EQ(2u, GetPendingCallbackCount());
  EXPECT_CALL(mock_callback1, Run(false));
  EXPECT_CALL(mock_callback2, Run(false));
  WaitForNextReprompting();

  EXPECT_EQ(1u, GetRepromptCount());
  EXPECT_EQ(0u, GetPendingCallbackCount());
}

// Different requests, all callbacks are expected to be called, and new prompt
// should be shown
TEST_F(PermissionsRepromptControllerAndroidTest, DifferentRequests) {
  base::MockOnceCallback<void(bool)> mock_callback1;
  base::MockOnceCallback<void(bool)> mock_callback2;
  RepromtMissingLocation(mock_callback1.Get());
  EXPECT_EQ(1u, GetPendingCallbackCount());
  RepromtMissingCameraAR(mock_callback2.Get());
  EXPECT_EQ(2u, GetPendingCallbackCount());
  EXPECT_CALL(mock_callback1, Run(false));
  WaitForNextReprompting();
  EXPECT_CALL(mock_callback2, Run(false));
  WaitForNextReprompting();
  EXPECT_EQ(2u, GetRepromptCount());
  EXPECT_EQ(0u, GetPendingCallbackCount());
}

// Mixed requests including both duplicated and different request.
TEST_F(PermissionsRepromptControllerAndroidTest, MixedRequests) {
  base::MockOnceCallback<void(bool)> mock_callback1;
  base::MockOnceCallback<void(bool)> mock_callback2;
  base::MockOnceCallback<void(bool)> mock_callback3;
  base::MockOnceCallback<void(bool)> mock_callback4;
  RepromtMissingLocation(mock_callback1.Get());
  EXPECT_EQ(1u, GetPendingCallbackCount());
  RepromtMissingCameraMediaStream(mock_callback2.Get());
  EXPECT_EQ(2u, GetPendingCallbackCount());
  RepromtMissingLocation(mock_callback3.Get());
  EXPECT_EQ(2u, GetPendingCallbackCount());
  RepromtMissingCameraAR(mock_callback4.Get());
  EXPECT_EQ(3u, GetPendingCallbackCount());
  EXPECT_CALL(mock_callback1, Run(false));
  EXPECT_CALL(mock_callback2, Run(false));
  WaitForNextReprompting();
  EXPECT_CALL(mock_callback4, Run(false));
  EXPECT_CALL(mock_callback3, Run).Times(0);
  WaitForNextReprompting();
  EXPECT_EQ(2u, GetRepromptCount());
  EXPECT_EQ(0u, GetPendingCallbackCount());
}

// Reprompt missing permission again after finished the last ones.
TEST_F(PermissionsRepromptControllerAndroidTest, OnRepromptPermissionDone) {
  base::MockOnceCallback<void(bool)> mock_callback1;
  base::MockOnceCallback<void(bool)> mock_callback2;
  base::MockOnceCallback<void(bool)> mock_callback3;
  base::MockOnceCallback<void(bool)> mock_callback4;
  RepromtMissingLocation(mock_callback1.Get());
  EXPECT_EQ(1u, GetPendingCallbackCount());
  RepromtMissingCameraMediaStream(mock_callback2.Get());
  EXPECT_EQ(2u, GetPendingCallbackCount());
  EXPECT_CALL(mock_callback1, Run(false));
  WaitForNextReprompting();
  EXPECT_CALL(mock_callback2, Run(false));
  WaitForNextReprompting();
  EXPECT_EQ(0u, GetPendingCallbackCount());
  RepromtMissingLocation(mock_callback3.Get());
  EXPECT_EQ(1u, GetPendingCallbackCount());
  EXPECT_CALL(mock_callback3, Run(false));
  WaitForNextReprompting();
  EXPECT_EQ(0u, GetPendingCallbackCount());
  RepromtMissingCameraAR(mock_callback4.Get());
  EXPECT_EQ(1u, GetPendingCallbackCount());
  EXPECT_CALL(mock_callback4, Run(false));
  WaitForNextReprompting();
  EXPECT_EQ(4u, GetRepromptCount());
  EXPECT_EQ(0u, GetPendingCallbackCount());
}

}  // namespace permissions
