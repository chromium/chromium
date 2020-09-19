// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"

#include <memory>

#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace settings {

namespace {

const char kWebCallbackFunctionName[] = "cr.webUIListenerCallback";

class TestAmbientModeHandler : public AmbientModeHandler {
 public:
  TestAmbientModeHandler() = default;
  ~TestAmbientModeHandler() override = default;

  // Make public for testing.
  using AmbientModeHandler::AllowJavascript;
  using AmbientModeHandler::RegisterMessages;
  using AmbientModeHandler::set_web_ui;
};

}  // namespace

class AmbientModeHandlerTest : public testing::Test {
 public:
  AmbientModeHandlerTest() = default;
  ~AmbientModeHandlerTest() override = default;

  void SetUp() override {
    web_ui_ = std::make_unique<content::TestWebUI>();
    handler_ = std::make_unique<TestAmbientModeHandler>();
    handler_->set_web_ui(web_ui_.get());
    handler_->RegisterMessages();
    handler_->AllowJavascript();
    fake_backend_controller_ =
        std::make_unique<ash::FakeAmbientBackendControllerImpl>();
    image_downloader_ = std::make_unique<ash::TestImageDownloader>();
  }

  void RequestSettings() {
    base::ListValue args;
    handler_->HandleRequestSettings(&args);
  }

  void RequestAlbums(ash::AmbientModeTopicSource topic_source) {
    base::ListValue args;
    args.Append(static_cast<int>(topic_source));
    handler_->HandleRequestAlbums(&args);
  }

  void FetchSettings() {
    handler_->RequestSettingsAndAlbums(/*topic_source=*/base::nullopt);
  }

  void UpdateSettings() {
    handler_->settings_ = ash::AmbientSettings();
    handler_->UpdateSettings();
  }

  bool IsUpdateSettingsPendingAtHandler() const {
    return handler_->is_updating_backend_;
  }

  bool HasPendingUpdatesForTesting() const {
    return handler_->has_pending_updates_for_backend_;
  }

  base::TimeDelta GetFetchSettingsDelay() {
    return handler_->fetch_settings_retry_backoff_.GetTimeUntilRelease();
  }

  base::TimeDelta GetUpdateSettingsDelay() {
    return handler_->update_settings_retry_backoff_.GetTimeUntilRelease();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  bool IsFetchSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsFetchSettingsAndAlbumsPending();
  }

  void ReplyFetchSettingsAndAlbums(bool success) {
    fake_backend_controller_->ReplyFetchSettingsAndAlbums(success);
  }

  bool IsUpdateSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsUpdateSettingsPending();
  }

  void ReplyUpdateSettings(bool success) {
    fake_backend_controller_->ReplyUpdateSettings(success);
  }

  std::string BoolToString(bool x) { return x ? "true" : "false"; }

  void VerifySettingsSent() {
    EXPECT_EQ(2U, web_ui_->call_data().size());

    // The call is structured such that the function name is the "web callback"
    // name and the first argument is the name of the message being sent.
    const auto& topic_source_call_data = *web_ui_->call_data().front();
    const auto& temperature_unit_call_data = *web_ui_->call_data().back();

    // Topic Source
    EXPECT_EQ(kWebCallbackFunctionName, topic_source_call_data.function_name());
    EXPECT_EQ("topic-source-changed",
              topic_source_call_data.arg1()->GetString());
    // In FakeAmbientBackendControllerImpl, the |topic_source| is
    // kGooglePhotos.
    const base::DictionaryValue* dictionary = nullptr;
    topic_source_call_data.arg2()->GetAsDictionary(&dictionary);
    const base::Value* topic_source_value = dictionary->FindKey("topicSource");
    EXPECT_EQ(0, topic_source_value->GetInt());

    // Temperature Unit
    EXPECT_EQ(kWebCallbackFunctionName,
              temperature_unit_call_data.function_name());
    EXPECT_EQ("temperature-unit-changed",
              temperature_unit_call_data.arg1()->GetString());
    // In FakeAmbientBackendControllerImpl, the |temperature_unit| is kCelsius.
    EXPECT_EQ("celsius", temperature_unit_call_data.arg2()->GetString());
  }

  void VerifyAlbumsSent(ash::AmbientModeTopicSource topic_source) {
    // Art gallery has an extra call to update the topic source to Art gallery.
    std::vector<std::unique_ptr<content::TestWebUI::CallData>>::size_type call_size =
        topic_source == ash::AmbientModeTopicSource::kGooglePhotos ? 1U : 2U;
    EXPECT_EQ(call_size, web_ui_->call_data().size());

    if (topic_source == ash::AmbientModeTopicSource::kArtGallery) {
      const auto& topic_source_call_data = *web_ui_->call_data().front();
      const base::DictionaryValue* dictionary = nullptr;
      topic_source_call_data.arg2()->GetAsDictionary(&dictionary);
      const base::Value* topic_source_value =
          dictionary->FindKey("topicSource");
      EXPECT_EQ(static_cast<int>(topic_source), topic_source_value->GetInt());
    }

    const content::TestWebUI::CallData& call_data =
        *web_ui_->call_data().back();

    // The call is structured such that the function name is the "web callback"
    // name and the first argument is the name of the message being sent.
    EXPECT_EQ(kWebCallbackFunctionName, call_data.function_name());
    EXPECT_EQ("albums-changed", call_data.arg1()->GetString());

    // The test data is set in FakeAmbientBackendControllerImpl.
    const base::DictionaryValue* dictionary = nullptr;
    call_data.arg2()->GetAsDictionary(&dictionary);

    const base::Value* topic_source_value = dictionary->FindKey("topicSource");
    EXPECT_EQ(static_cast<int>(topic_source), topic_source_value->GetInt());

    const base::Value* albums = dictionary->FindKey("albums");
    EXPECT_EQ(2U, albums->GetList().size());

    const base::DictionaryValue* album0;
    albums->GetList()[0].GetAsDictionary(&album0);
    EXPECT_EQ("0", album0->FindKey("albumId")->GetString());

    const base::DictionaryValue* album1;
    albums->GetList()[1].GetAsDictionary(&album1);
    EXPECT_EQ("1", album1->FindKey("albumId")->GetString());

    if (topic_source == ash::AmbientModeTopicSource::kGooglePhotos) {
      EXPECT_EQ(false, album0->FindKey("checked")->GetBool());
      EXPECT_EQ("album0", album0->FindKey("title")->GetString());

      EXPECT_EQ(true, album1->FindKey("checked")->GetBool());
      EXPECT_EQ("album1", album1->FindKey("title")->GetString());
    } else {
      EXPECT_EQ(true, album0->FindKey("checked")->GetBool());
      EXPECT_EQ("art0", album0->FindKey("title")->GetString());

      EXPECT_EQ(false, album1->FindKey("checked")->GetBool());
      EXPECT_EQ("art1", album1->FindKey("title")->GetString());
    }
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<ash::FakeAmbientBackendControllerImpl>
      fake_backend_controller_;
  std::unique_ptr<ash::TestImageDownloader> image_downloader_;
  std::unique_ptr<TestAmbientModeHandler> handler_;
};

TEST_F(AmbientModeHandlerTest, TestSendTemperatureUnitAndTopicSource) {
  RequestSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  VerifySettingsSent();
}

TEST_F(AmbientModeHandlerTest, TestSendAlbumsForGooglePhotos) {
  ash::AmbientModeTopicSource topic_source =
      ash::AmbientModeTopicSource::kGooglePhotos;
  RequestAlbums(topic_source);
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  VerifyAlbumsSent(topic_source);
}

TEST_F(AmbientModeHandlerTest, TestSendAlbumsForArtGallery) {
  ash::AmbientModeTopicSource topic_source =
      ash::AmbientModeTopicSource::kArtGallery;
  RequestAlbums(topic_source);
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  VerifyAlbumsSent(topic_source);
}

TEST_F(AmbientModeHandlerTest, TestFetchSettings) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(AmbientModeHandlerTest, TestFetchSettingsFailedWillRetry) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
}

TEST_F(AmbientModeHandlerTest, TestFetchSettingsSecondRetryWillBackoff) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  base::TimeDelta delay1 = GetFetchSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  base::TimeDelta delay2 = GetFetchSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
}

TEST_F(AmbientModeHandlerTest,
       TestFetchSettingsWillNotRetryMoreThanThreeTimes) {
  FetchSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 1st retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 2nd retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // 3rd retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/false);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  // Will not retry.
  FastForwardBy(GetFetchSettingsDelay() * 1.5);
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(AmbientModeHandlerTest, TestUpdateSettings) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());
}

TEST_F(AmbientModeHandlerTest, TestUpdateSettingsTwice) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(HasPendingUpdatesForTesting());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(HasPendingUpdatesForTesting());
}

TEST_F(AmbientModeHandlerTest, TestUpdateSettingsFailedWillRetry) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());
}

TEST_F(AmbientModeHandlerTest, TestUpdateSettingsSecondRetryWillBackoff) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  base::TimeDelta delay1 = GetUpdateSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  base::TimeDelta delay2 = GetUpdateSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());
}

TEST_F(AmbientModeHandlerTest,
       TestUpdateSettingsWillNotRetryMoreThanThreeTimes) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());

  // Will not retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesForTesting());
}

}  // namespace settings
}  // namespace chromeos
