// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/ambient_mode_handler.h"

#include <memory>

#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ambient/fake_ambient_backend_controller_impl.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace settings {

namespace {

const char kWebCallbackFunctionName[] = "cr.webUIListenerCallback";

class TestAmbientModeHandler : public AmbientModeHandler {
 public:
  explicit TestAmbientModeHandler(PrefService* pref_service)
      : AmbientModeHandler(pref_service) {}
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
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    test_pref_service_->registry()->RegisterBooleanPref(
        ash::ambient::prefs::kAmbientModeEnabled, true);

    handler_ =
        std::make_unique<TestAmbientModeHandler>(test_pref_service_.get());
    handler_->set_web_ui(web_ui_.get());
    handler_->RegisterMessages();
    handler_->AllowJavascript();
    fake_backend_controller_ =
        std::make_unique<ash::FakeAmbientBackendControllerImpl>();
    image_downloader_ = std::make_unique<ash::TestImageDownloader>();
  }

  void TearDown() override { handler_->DisallowJavascript(); }

  content::TestWebUI* web_ui() { return web_ui_.get(); }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  base::Optional<ash::AmbientSettings>& settings() {
    return handler_->settings_;
  }

  void SetEnabledPref(bool enabled) {
    test_pref_service_->SetBoolean(ash::ambient::prefs::kAmbientModeEnabled,
                                   enabled);
  }

  void SetTopicSource(ash::AmbientModeTopicSource topic_source) {
    if (!handler_->settings_)
      handler_->settings_ = ash::AmbientSettings();

    handler_->settings_->topic_source = topic_source;
  }

  void SetTemperatureUnit(ash::AmbientModeTemperatureUnit temperature_unit) {
    if (!handler_->settings_)
      handler_->settings_ = ash::AmbientSettings();

    handler_->settings_->temperature_unit = temperature_unit;
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

  void HandleSetSelectedTemperatureUnit(const base::ListValue* args) {
    handler_->HandleSetSelectedTemperatureUnit(args);
  }

  void HandleSetSelectedAlbums(const base::ListValue* args) {
    handler_->HandleSetSelectedAlbums(args);
  }

  void FetchSettings() {
    handler_->RequestSettingsAndAlbums(/*topic_source=*/base::nullopt);
  }

  void UpdateSettings() {
    if (!handler_->settings_)
      handler_->settings_ = ash::AmbientSettings();

    handler_->UpdateSettings();
  }

  bool HasPendingFetchRequestAtHandler() const {
    return handler_->has_pending_fetch_request_;
  }

  bool IsUpdateSettingsPendingAtHandler() const {
    return handler_->is_updating_backend_;
  }

  bool HasPendingUpdatesAtHandler() const {
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

  void ReplyFetchSettingsAndAlbums(
      bool success,
      base::Optional<ash::AmbientSettings> settings = base::nullopt) {
    fake_backend_controller_->ReplyFetchSettingsAndAlbums(success,
                                                          std::move(settings));
  }

  bool IsUpdateSettingsPendingAtBackend() const {
    return fake_backend_controller_->IsUpdateSettingsPending();
  }

  void ReplyUpdateSettings(bool success) {
    fake_backend_controller_->ReplyUpdateSettings(success);
  }

  std::string BoolToString(bool x) { return x ? "true" : "false"; }

  void VerifySettingsSent(ash::AmbientModeTopicSource topic_source,
                          const std::string& temperature_unit) {
    EXPECT_EQ(2U, web_ui_->call_data().size());

    // The call is structured such that the function name is the "web callback"
    // name and the first argument is the name of the message being sent.
    const auto& topic_source_call_data = *web_ui_->call_data().front();
    const auto& temperature_unit_call_data = *web_ui_->call_data().back();

    // Topic Source
    EXPECT_EQ(kWebCallbackFunctionName, topic_source_call_data.function_name());
    EXPECT_EQ("topic-source-changed",
              topic_source_call_data.arg1()->GetString());
    const base::DictionaryValue* dictionary = nullptr;
    topic_source_call_data.arg2()->GetAsDictionary(&dictionary);
    const base::Value* topic_source_value = dictionary->FindKey("topicSource");
    EXPECT_EQ(static_cast<int>(topic_source), topic_source_value->GetInt());

    // Temperature Unit
    EXPECT_EQ(kWebCallbackFunctionName,
              temperature_unit_call_data.function_name());
    EXPECT_EQ("temperature-unit-changed",
              temperature_unit_call_data.arg1()->GetString());
    EXPECT_EQ(temperature_unit, temperature_unit_call_data.arg2()->GetString());
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
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
};

TEST_F(AmbientModeHandlerTest, TestSendTemperatureUnitAndTopicSource) {
  RequestSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  // In FakeAmbientBackendControllerImpl, the |topic_source| is kGooglePhotos,
  // the |temperature_unit| is kCelsius.
  VerifySettingsSent(ash::AmbientModeTopicSource::kGooglePhotos, "celsius");
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
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());
}

TEST_F(AmbientModeHandlerTest, TestUpdateSettingsTwice) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/true);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(HasPendingUpdatesAtHandler());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(HasPendingUpdatesAtHandler());
}

TEST_F(AmbientModeHandlerTest, TestUpdateSettingsFailedWillRetry) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());
}

TEST_F(AmbientModeHandlerTest, TestUpdateSettingsSecondRetryWillBackoff) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  base::TimeDelta delay1 = GetUpdateSettingsDelay();
  FastForwardBy(delay1 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  base::TimeDelta delay2 = GetUpdateSettingsDelay();
  EXPECT_GT(delay2, delay1);

  FastForwardBy(delay2 * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());
}

TEST_F(AmbientModeHandlerTest,
       TestUpdateSettingsWillNotRetryMoreThanThreeTimes) {
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  ReplyUpdateSettings(/*success=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());

  // Will not retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(HasPendingUpdatesAtHandler());
}

TEST_F(AmbientModeHandlerTest, TestNoFetchRequestWhenUpdatingSettings) {
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
  UpdateSettings();
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());

  RequestSettings();
  EXPECT_TRUE(HasPendingFetchRequestAtHandler());
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());
}

TEST_F(AmbientModeHandlerTest, TestSendSettingsWhenUpdatedSettings) {
  // Simulate initial page request.
  RequestSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
  SetTopicSource(ash::AmbientModeTopicSource::kArtGallery);
  UpdateSettings();
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());

  RequestSettings();
  EXPECT_TRUE(HasPendingFetchRequestAtHandler());
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  web_ui()->ClearTrackedCalls();
  ReplyUpdateSettings(/*success=*/true);

  // In FakeAmbientBackendControllerImpl, the |topic_source| is kGooglePhotos,
  // the |temperature_unit| is kCelsius.
  VerifySettingsSent(ash::AmbientModeTopicSource::kArtGallery, "celsius");
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
}

TEST_F(AmbientModeHandlerTest,
       TestSendAlbumsOfGooglePhotosWhenUpdatedSettings) {
  // Simulate initial page request.
  ash::AmbientModeTopicSource topic_source =
      ash::AmbientModeTopicSource::kGooglePhotos;
  RequestAlbums(topic_source);
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  web_ui()->ClearTrackedCalls();

  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
  UpdateSettings();
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());

  RequestAlbums(topic_source);
  EXPECT_TRUE(HasPendingFetchRequestAtHandler());
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  ReplyUpdateSettings(/*success=*/true);
  VerifyAlbumsSent(topic_source);
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
}

TEST_F(AmbientModeHandlerTest, TestSendAlbumsOfArtGalleryWhenUpdatedSettings) {
  // Simulate initial page request.
  ash::AmbientModeTopicSource topic_source =
      ash::AmbientModeTopicSource::kGooglePhotos;
  RequestAlbums(topic_source);
  ReplyFetchSettingsAndAlbums(/*success=*/true);
  web_ui()->ClearTrackedCalls();

  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
  UpdateSettings();
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());

  RequestAlbums(topic_source);
  EXPECT_TRUE(HasPendingFetchRequestAtHandler());
  EXPECT_FALSE(IsFetchSettingsPendingAtBackend());

  ReplyUpdateSettings(/*success=*/true);
  VerifyAlbumsSent(topic_source);
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
}

TEST_F(AmbientModeHandlerTest, TestNotUpdateUIWhenFetechedSettings) {
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
  RequestSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());

  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(AmbientModeHandlerTest, TestNotSendSettingsWhenFetechedSettings) {
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());
  RequestSettings();
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());

  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(AmbientModeHandlerTest, TestNotSendAlbumsWhenFetechedSettings) {
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());

  ash::AmbientModeTopicSource topic_source =
      ash::AmbientModeTopicSource::kGooglePhotos;
  RequestAlbums(topic_source);
  EXPECT_TRUE(IsFetchSettingsPendingAtBackend());
  EXPECT_FALSE(HasPendingFetchRequestAtHandler());

  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());

  ReplyFetchSettingsAndAlbums(/*success=*/true);
  EXPECT_EQ(0U, web_ui()->call_data().size());
}

TEST_F(AmbientModeHandlerTest, TestSendSettingsWhenUpdateSettingsFailed) {
  // Simulate initial page request.
  RequestSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  SetTopicSource(ash::AmbientModeTopicSource::kArtGallery);
  UpdateSettings();
  ReplyUpdateSettings(/*success=*/false);

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);

  web_ui()->ClearTrackedCalls();
  EXPECT_EQ(0U, web_ui()->call_data().size());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);
  // In FakeAmbientBackendControllerImpl, the |topic_source| is kGooglePhotos,
  // the |temperature_unit| is kCelsius.
  VerifySettingsSent(ash::AmbientModeTopicSource::kGooglePhotos, "celsius");
}

TEST_F(AmbientModeHandlerTest,
       TestSendAlbumsOfGooglePhotosWhenUpdateSettingsFailed) {
  // Simulate initial page request.
  ash::AmbientModeTopicSource topic_source =
      ash::AmbientModeTopicSource::kGooglePhotos;
  SetTopicSource(topic_source);
  RequestAlbums(topic_source);
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  UpdateSettings();
  ReplyUpdateSettings(/*success=*/false);

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);

  web_ui()->ClearTrackedCalls();
  EXPECT_EQ(0U, web_ui()->call_data().size());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);
  VerifyAlbumsSent(topic_source);
}

TEST_F(AmbientModeHandlerTest,
       TestSendAlbumsOfArtGalleryWhenUpdateSettingsFailed) {
  // Simulate initial page request.
  ash::AmbientModeTopicSource topic_source =
      ash::AmbientModeTopicSource::kArtGallery;
  SetTopicSource(topic_source);
  RequestAlbums(topic_source);
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  UpdateSettings();
  ReplyUpdateSettings(/*success=*/false);

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);

  web_ui()->ClearTrackedCalls();
  EXPECT_EQ(0U, web_ui()->call_data().size());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);
  VerifyAlbumsSent(topic_source);
}

// Test that there are two updates, the first update succeeded and the second
// update failed. When the second update failed, it will update UI to restore
// the latest successfully updated settings.
TEST_F(AmbientModeHandlerTest, TestSendSettingsWithCachedSettings) {
  ash::AmbientModeTopicSource topic_source_google_photos =
      ash::AmbientModeTopicSource::kGooglePhotos;
  ash::AmbientModeTopicSource topic_source_art_gallery =
      ash::AmbientModeTopicSource::kArtGallery;

  // Simulate initial page request.
  RequestSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  // The first update.
  SetTopicSource(topic_source_art_gallery);
  UpdateSettings();
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());

  // There is the second change and pending update before retry.
  SetTopicSource(topic_source_google_photos);
  UpdateSettings();
  EXPECT_TRUE(HasPendingUpdatesAtHandler());

  // First update returns true and will start the second update.
  ReplyUpdateSettings(/*success=*/true);
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());

  ReplyUpdateSettings(/*success=*/false);

  // 1st retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);

  // 2nd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);

  web_ui()->ClearTrackedCalls();
  EXPECT_EQ(0U, web_ui()->call_data().size());

  // 3rd retry.
  FastForwardBy(GetUpdateSettingsDelay() * 1.5);
  ReplyUpdateSettings(/*success=*/false);
  VerifySettingsSent(topic_source_art_gallery, "celsius");
}

TEST_F(AmbientModeHandlerTest, TestAlbumNumbersAreRecorded) {
  RequestSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  base::ListValue args;
  base::DictionaryValue dictionary;
  ash::AmbientModeTopicSource topic_source =
      ash::AmbientModeTopicSource::kGooglePhotos;
  dictionary.SetKey("topicSource", base::Value(static_cast<int>(topic_source)));

  base::Value albums(base::Value::Type::LIST);
  base::Value album(base::Value::Type::DICTIONARY);
  album.SetKey("albumId", base::Value("0"));
  albums.Append(std::move(album));
  dictionary.SetKey("albums", std::move(albums));

  args.Append(std::move(dictionary));
  HandleSetSelectedAlbums(&args);

  histogram_tester().ExpectTotalCount("Ash.AmbientMode.TotalNumberOfAlbums",
                                      /*count=*/1);
  histogram_tester().ExpectTotalCount("Ash.AmbientMode.SelectedNumberOfAlbums",
                                      /*count=*/1);
}

TEST_F(AmbientModeHandlerTest, TestTemperatureUnitChangeUpdatesSettings) {
  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kCelsius);

  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  base::ListValue args;
  args.Append("fahrenheit");

  HandleSetSelectedTemperatureUnit(&args);

  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());

  ReplyUpdateSettings(/*success=*/true);

  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
}

TEST_F(AmbientModeHandlerTest, TestSameTemperatureUnitSkipsUpdate) {
  SetTemperatureUnit(ash::AmbientModeTemperatureUnit::kCelsius);

  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  base::ListValue args;
  args.Append("celsius");

  HandleSetSelectedTemperatureUnit(&args);

  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());
}

TEST_F(AmbientModeHandlerTest, TestEnabledPrefChangeUpdatesSettings) {
  // Simulate initial page request.
  RequestSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true);

  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // Should not trigger |UpdateSettings|.
  SetEnabledPref(/*enabled=*/false);
  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // Settings this to true should trigger |UpdateSettings|.
  SetEnabledPref(/*enabled=*/true);
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());
}

TEST_F(AmbientModeHandlerTest, TestWeatherFalseTriggersUpdateSettings) {
  ash::AmbientSettings weather_off_settings;
  weather_off_settings.show_weather = false;

  // Simulate initial page request with weather settings false. Because Ambient
  // mode pref is enabled and |settings.show_weather| is false, this should
  // trigger a call to |UpdateSettings| that sets |settings.show_weather| to
  // true.
  RequestSettings();
  ReplyFetchSettingsAndAlbums(/*success=*/true, weather_off_settings);

  // A call to |UpdateSettings| should have happened.
  EXPECT_TRUE(IsUpdateSettingsPendingAtHandler());
  EXPECT_TRUE(IsUpdateSettingsPendingAtBackend());

  ReplyUpdateSettings(/*success=*/true);

  EXPECT_FALSE(IsUpdateSettingsPendingAtHandler());
  EXPECT_FALSE(IsUpdateSettingsPendingAtBackend());

  // |settings.show_weather| should now be true after the successful settings
  // update.
  EXPECT_TRUE(settings()->show_weather);
}

}  // namespace settings
}  // namespace chromeos
