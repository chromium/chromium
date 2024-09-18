// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"

#include <memory>
#include <optional>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_controller.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/mock_file_suggest_keyed_service.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/birch/birch_file_suggest_provider.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"
#include "chrome/browser/ui/ash/birch/birch_lost_media_provider.h"
#include "chrome/browser/ui/ash/birch/birch_self_share_provider.h"
#include "chrome/browser/ui/ash/holding_space/scoped_test_mount_point.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/media_session/public/cpp/test/test_media_controller.h"
#include "services/media_session/public/mojom/media_session.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kSessionName1[] = "test_session_name 1";
constexpr char kSessionName2[] = "test_session_name 2";

constexpr char kSessionTag1[] = "SessionTag1";
constexpr char kSessionTag2[] = "SessionTag2";

constexpr char kExampleURL1[] = "http://www.example.com/1";
constexpr char kExampleURL2[] = "http://www.example.com/2";

constexpr char16_t kTabTitle1[] = u"Tab Title 1";
constexpr char16_t kTabTitle2[] = u"Tab Title 2";

constexpr char kTargetDeviceFullName[] = "Device_1";
constexpr char kTargetDeviceShortName[] = "Device_1";
constexpr char kTargetDeviceCacheGuid[] = "device_guid_1";

constexpr char kChromeSyncGuid[] = "Entry Guid";
constexpr char kChromeSyncDeviceName[] = "Device Name";
constexpr char kChromeSyncUrl[] = "https://www.example.com";

constexpr char16_t kSessionMetadataTitle[] = u"Media Title";
constexpr char16_t kSessionMetadataSourceTitle[] = u"youtube.com";
constexpr char16_t kSessionMetadataSourceTitleFull[] =
    u"https://www.youtube.com";

constexpr char kMediaAppUrl[] = "https://meet.google.com";
constexpr char16_t kMediaAppTitle[] = u"Google Meet";

std::unique_ptr<sync_sessions::SyncedSession> CreateNewSession(
    const std::string& session_name,
    const std::string& session_tag,
    syncer::DeviceInfo::FormFactor form_factor) {
  auto session = std::make_unique<sync_sessions::SyncedSession>();
  auto window = std::make_unique<sync_sessions::SyncedSessionWindow>();
  auto tab = std::make_unique<sessions::SessionTab>();

  session->SetSessionName(session_name);
  session->SetDeviceTypeAndFormFactor(sync_pb::SyncEnums::TYPE_UNSET,
                                      form_factor);

  window->wrapped_window.tabs.push_back(std::move(tab));
  session->windows[SessionID::NewUnique()] = std::move(window);
  session->SetSessionTag(session_tag);
  return session;
}

class MockSessionSyncService : public sync_sessions::SessionSyncService {
 public:
  MockSessionSyncService() = default;
  ~MockSessionSyncService() override = default;

  MOCK_METHOD(syncer::GlobalIdMapper*,
              GetGlobalIdMapper,
              (),
              (const, override));
  MOCK_METHOD(sync_sessions::OpenTabsUIDelegate*,
              GetOpenTabsUIDelegate,
              (),
              (override));
  base::CallbackListSubscription SubscribeToForeignSessionsChanged(
      const base::RepeatingClosure& cb) override {
    return subscriber_list_.Add(cb);
  }
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());

  void NotifyMockForeignSessionsChanged() { subscriber_list_.Notify(); }

  bool IsSubscribersEmpty() { return subscriber_list_.empty(); }

 private:
  base::RepeatingClosureList subscriber_list_;
};

class MockOpenTabsUIDelegate : public sync_sessions::OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate() {
    foreign_sessions_owned_.push_back(CreateNewSession(
        kSessionName1, kSessionTag1, syncer::DeviceInfo::FormFactor::kDesktop));
    foreign_sessions_.push_back(foreign_sessions_owned_.back().get());
    foreign_sessions_owned_.push_back(CreateNewSession(
        kSessionName2, kSessionTag2, syncer::DeviceInfo::FormFactor::kPhone));
    foreign_sessions_.push_back(foreign_sessions_owned_.back().get());

    std::vector<std::unique_ptr<sessions::SessionTab>> session_tabs_one;
    auto tab = std::make_unique<sessions::SessionTab>();
    tab->timestamp = base::Time::Now();
    tab->navigations.push_back(sessions::SerializedNavigationEntryTestHelper::
                                   CreateNavigationForTest());
    tab->navigations[0].set_timestamp(base::Time::Now());
    tab->navigations[0].set_title(kTabTitle1);
    tab->navigations[0].set_virtual_url(GURL(kExampleURL1));
    session_tabs_one.push_back(std::move(tab));

    std::vector<std::unique_ptr<sessions::SessionTab>> session_tabs_two;
    tab = std::make_unique<sessions::SessionTab>();
    tab->timestamp = base::Time::Now();
    tab->navigations.push_back(sessions::SerializedNavigationEntryTestHelper::
                                   CreateNavigationForTest());
    tab->navigations[0].set_timestamp(base::Time::Now() + base::Minutes(5));
    tab->navigations[0].set_title(kTabTitle2);
    tab->navigations[0].set_virtual_url(GURL(kExampleURL2));
    session_tabs_two.push_back(std::move(tab));

    session_tabs_.emplace(kSessionTag1, std::move(session_tabs_one));
    session_tabs_.emplace(kSessionTag2, std::move(session_tabs_two));
  }

  bool GetAllForeignSessions(
      std::vector<raw_ptr<const sync_sessions::SyncedSession,
                          VectorExperimental>>* sessions) override {
    *sessions = foreign_sessions_;
    base::ranges::sort(*sessions, std::greater(),
                       [](const sync_sessions::SyncedSession* session) {
                         return session->GetModifiedTime();
                       });

    return !sessions->empty();
  }

  MOCK_METHOD1(GetLocalSession,
               bool(const sync_sessions::SyncedSession** local_session));

  MOCK_METHOD3(GetForeignTab,
               bool(const std::string& tag,
                    const SessionID tab_id,
                    const sessions::SessionTab** tab));

  MOCK_METHOD1(DeleteForeignSession, void(const std::string& tag));

  MOCK_METHOD1(
      GetForeignSession,
      std::vector<const sessions::SessionWindow*>(const std::string& tag));

  bool GetForeignSessionTabs(
      const std::string& tag,
      std::vector<const sessions::SessionTab*>* tabs) override {
    auto it = session_tabs_.find(tag);
    if (it != session_tabs_.end()) {
      for (auto& tab : it->second) {
        tabs->push_back(tab.get());
      }
    }

    return true;
  }

 private:
  std::vector<std::unique_ptr<sync_sessions::SyncedSession>>
      foreign_sessions_owned_;
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      foreign_sessions_;
  std::map<std::string, std::vector<std::unique_ptr<sessions::SessionTab>>>
      session_tabs_;
};

std::unique_ptr<KeyedService> BuildMockSessionSyncService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<MockSessionSyncService>>();
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

class SendTabToSelfModelMock : public send_tab_to_self::TestSendTabToSelfModel {
 public:
  SendTabToSelfModelMock() = default;

  ~SendTabToSelfModelMock() override = default;

  MOCK_METHOD1(DeleteEntry, void(const std::string&));
  MOCK_METHOD1(DismissEntry, void(const std::string&));

  send_tab_to_self::SendTabToSelfEntry* AddEntry(
      const GURL& url,
      const std::string& title,
      const std::string& target_device_cache_guid) override {
    auto entry = std::make_unique<send_tab_to_self::SendTabToSelfEntry>(
        kChromeSyncGuid, url, title, base::Time::Now(), kChromeSyncDeviceName,
        target_device_cache_guid);

    auto* result = entry.get();

    entries_.emplace(kChromeSyncGuid, std::move(entry));

    return result;
  }

  const send_tab_to_self::SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const override {
    auto it = entries_.find(guid);
    return it != entries_.end() ? it->second.get() : nullptr;
  }

  std::vector<std::string> GetAllGuids() const override {
    std::vector<std::string> keys;
    for (const auto& it : entries_) {
      DCHECK_EQ(it.first, it.second->GetGUID());
      keys.push_back(it.first);
    }
    return keys;
  }

  void MarkEntryOpened(const std::string& guid) override {
    auto it = entries_.find(guid);
    if (it != entries_.end()) {
      if (auto* entry = it->second.get()) {
        entry->MarkOpened();
      }
    }
  }

  std::vector<send_tab_to_self::TargetDeviceInfo>
  GetTargetDeviceInfoSortedList() override {
    return devices_;
  }

  void AddMockTargetDevice(syncer::DeviceInfo::FormFactor form_factor) {
    devices_.emplace_back(kTargetDeviceFullName, kTargetDeviceShortName,
                          kTargetDeviceCacheGuid, form_factor,
                          base::Time::Now());
  }

 private:
  std::map<std::string, std::unique_ptr<send_tab_to_self::SendTabToSelfEntry>>
      entries_;

  std::vector<send_tab_to_self::TargetDeviceInfo> devices_;
};

class TestSendTabToSelfSyncService
    : public send_tab_to_self::SendTabToSelfSyncService {
 public:
  TestSendTabToSelfSyncService() : fake_delegate_(syncer::SEND_TAB_TO_SELF) {}

  ~TestSendTabToSelfSyncService() override = default;

  send_tab_to_self::SendTabToSelfModel* GetSendTabToSelfModel() override {
    return &model_mock_;
  }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    return fake_delegate_.GetWeakPtr();
  }

 protected:
  syncer::FakeDataTypeControllerDelegate fake_delegate_;
  SendTabToSelfModelMock model_mock_;
};

std::unique_ptr<KeyedService> BuildTestSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<TestSendTabToSelfSyncService>();
}

class FaviconServiceMock : public favicon::MockFaviconService {
 public:
  FaviconServiceMock() {
    // This default implementation is provided to satisfy both actual
    // functionality and the ability to set expectations in tests.
    ON_CALL(*this,
            GetLargestRawFaviconForPageURL(testing::_, testing::_, testing::_,
                                           testing::_, testing::_))
        .WillByDefault(testing::Invoke(
            this, &FaviconServiceMock::DefaultGetLargestRawFaviconForPageURL));

    ON_CALL(*this, GetRawFavicon(testing::_, testing::_, testing::_, testing::_,
                                 testing::_))
        .WillByDefault(
            testing::Invoke(this, &FaviconServiceMock::DefaultGetRawFavicon));
  }
  ~FaviconServiceMock() override = default;
  FaviconServiceMock(const FaviconServiceMock&) = delete;
  FaviconServiceMock& operator=(const FaviconServiceMock&) = delete;

 private:
  base::CancelableTaskTracker::TaskId DefaultGetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<favicon_base::IconTypeSet>& icon_types,
      int minimum_size_in_pixels,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) {
    favicon_base::FaviconRawBitmapResult result;

    std::move(callback).Run(result);

    return base::CancelableTaskTracker::kBadTaskId;
  }

  base::CancelableTaskTracker::TaskId DefaultGetRawFavicon(
      const GURL& icon_url,
      const favicon_base::IconType& icon_type,
      int desired_size_in_pixels,
      favicon_base::FaviconRawBitmapCallback callback,
      base::CancelableTaskTracker* tracker) {
    favicon_base::FaviconRawBitmapResult result;

    std::move(callback).Run(result);

    return base::CancelableTaskTracker::kBadTaskId;
  }
};

std::unique_ptr<KeyedService> BuildFaviconServiceMock(
    content::BrowserContext* context) {
  return std::make_unique<FaviconServiceMock>();
}

}  // namespace

// TODO(https://crbug.com/1370774): move `ScopedTestMountPoint` out of holding
// space to remove the dependency on holding space code.
using ash::holding_space::ScopedTestMountPoint;

using media_session::test::TestMediaController;

class BirchKeyedServiceTest : public BrowserWithTestWindowTest {
 public:
  BirchKeyedServiceTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        fake_user_manager_(std::make_unique<FakeChromeUserManager>()) {
    feature_list_.InitWithFeatures(
        {features::kForestFeature,
         ash::features::kReleaseNotesNotificationAllChannels,
         ash::features::kBirchVideoConferenceSuggestions},
        {});
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    BrowserWithTestWindowTest::SetUp();

    file_suggest_service_ = static_cast<MockFileSuggestKeyedService*>(
        FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            GetProfile()));

    mount_point_ = std::make_unique<ScopedTestMountPoint>(
        "test_mount", storage::kFileSystemTypeLocal,
        file_manager::VOLUME_TYPE_TESTING);
    mount_point_->Mount(GetProfile());

    birch_keyed_service_ =
        BirchKeyedServiceFactory::GetInstance()->GetService(GetProfile());

    session_sync_service_ = static_cast<MockSessionSyncService*>(
        SessionSyncServiceFactory::GetInstance()->GetForProfile(GetProfile()));
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));

    SetSessionServiceToReturnOpenTabsDelegate(true);

    send_tab_to_self_model_ = static_cast<SendTabToSelfModelMock*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile())
            ->GetSendTabToSelfModel());

    favicon_service_ =
        static_cast<FaviconServiceMock*>(FaviconServiceFactory::GetForProfile(
            GetProfile(), ServiceAccessType::EXPLICIT_ACCESS));

    // Inject the test media controller into the media controls view.
    media_controller_ = std::make_unique<TestMediaController>();

    GetLostMediaProvider()->set_fake_media_controller_for_testing(
        media_controller_->CreateMediaControllerRemote());

    vc_controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    GetLostMediaProvider()->set_fake_video_conference_controller_for_testing(
        vc_controller_.get());
  }

  void SetSessionServiceToReturnOpenTabsDelegate(bool return_delegate) {
    EXPECT_CALL(*session_sync_service_, GetOpenTabsUIDelegate())
        .WillRepeatedly(
            testing::Return(return_delegate ? &open_tabs_delegate_ : nullptr));
  }

  void TearDown() override {
    GetLostMediaProvider()->set_fake_video_conference_controller_for_testing(
        nullptr);
    vc_controller_.reset();
    media_controller_.reset();
    send_tab_to_self_model_ = nullptr;
    mount_point_.reset();
    birch_keyed_service_ = nullptr;
    file_suggest_service_ = nullptr;
    session_sync_service_ = nullptr;
    sync_service_ = nullptr;
    release_notes_storage_ = nullptr;
    favicon_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void LogIn(const std::string& email) override {
    // TODO(crbug.com/40286020): merge into BrowserWithTestWindowTest.
    const AccountId account_id(AccountId::FromUserEmail(email));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    GetSessionControllerClient()->AddUserSession(email);
    GetSessionControllerClient()->SwitchActiveUser(account_id);
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    return profile_manager()->CreateTestingProfile(
        profile_name, {}, u"user_name", /*avatar_id=*/0, GetTestingFactories());
  }

  void SetUpReleaseNotesStorage() {
    release_notes_storage_ =
        std::make_unique<ReleaseNotesStorage>(GetProfile());
  }

  void MakePrimaryAccountAvailable() {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile());
    signin::MakePrimaryAccountAvailable(identity_manager, "user@gmail.com",
                                        signin::ConsentLevel::kSignin);
  }

  void AddNewChromeSyncEntry() {
    const GURL kUrl(kChromeSyncUrl);
    const std::string kTitle("Chrome Sync Title");
    const std::string kTargetDeviceSyncCacheGuid(kTargetDeviceCacheGuid);
    send_tab_to_self_model_->AddEntry(kUrl, kTitle, kTargetDeviceSyncCacheGuid);
  }

  void SimulateMediaMetadataInit() {
    media_session::MediaMetadata metadata;
    metadata.source_title = kSessionMetadataSourceTitle;
    metadata.title = kSessionMetadataTitle;

    GetLostMediaProvider()->MediaSessionMetadataChanged(metadata);
  }

  void SimulateMediaMetadataEnd() {
    media_session::MediaMetadata metadata;
    GetLostMediaProvider()->MediaSessionMetadataChanged(metadata);
  }

  void SimulateMediaSessionInfoChanged(bool is_playing,
                                       SecondaryIconType type) {
    media_session::mojom::MediaSessionInfoPtr info(
        media_session::mojom::MediaSessionInfo::New());
    info->playback_state =
        is_playing ? media_session::mojom::MediaPlaybackState::kPlaying
                   : media_session::mojom::MediaPlaybackState::kPaused;
    info->audio_video_states =
        std::vector<media_session::mojom::MediaAudioVideoState>();
    media_session::mojom::MediaAudioVideoState state;
    switch (type) {
      case SecondaryIconType::kLostMediaAudio:
        state = media_session::mojom::MediaAudioVideoState::kAudioOnly;
        break;
      case SecondaryIconType::kLostMediaVideo:
        state = media_session::mojom::MediaAudioVideoState::kVideoOnly;
        break;
      case SecondaryIconType::kLostMediaVideoConference:
      case SecondaryIconType::kTabFromDesktop:
      case SecondaryIconType::kTabFromTablet:
      case SecondaryIconType::kTabFromPhone:
      case SecondaryIconType::kTabFromUnknown:
      case SecondaryIconType::kNoIcon:
        state = media_session::mojom::MediaAudioVideoState::kDeprecatedUnknown;
        break;
    }
    info->audio_video_states->emplace_back(state);
    GetLostMediaProvider()->MediaSessionInfoChanged(std::move(info));
  }

  void AddMediaApp() {
    vc_controller_->AddMediaApp(
        crosapi::mojom::VideoConferenceMediaAppInfo::New(
            /*id=*/base::UnguessableToken::Create(),
            /*last_activity_time=*/base::Time::Now(),
            /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
            /*is_capturing_screen=*/false, /*title=*/kMediaAppTitle,
            /*url=*/GURL(kMediaAppUrl)));
  }

  void ClearMediaApps() { vc_controller_->ClearMediaApps(); }

  void ClearReleaseNotesSurfacesTimesLeftToShowPref() {
    GetProfile()->GetPrefs()->ClearPref(
        ::prefs::kReleaseNotesSuggestionChipTimesLeftToShow);
  }

  void MarkMilestoneUpToDate() {
    release_notes_storage_->MarkNotificationShown();
  }

  void MarkReleaseNotesSurfacesTimesLeftToShow(int times_left_to_show) {
    GetProfile()->GetPrefs()->SetInteger(
        ::prefs::kReleaseNotesSuggestionChipTimesLeftToShow,
        times_left_to_show);
  }

  int GetCurrentMilestone() {
    return version_info::GetVersion().components()[0];
  }

  TestSessionControllerClient* GetSessionControllerClient() {
    return ash_test_helper()->test_session_controller_client();
  }

  BirchLostMediaProvider* GetLostMediaProvider() {
    return static_cast<BirchLostMediaProvider*>(
        birch_keyed_service()->GetLostMediaProvider());
  }

  MockFileSuggestKeyedService* file_suggest_service() {
    return file_suggest_service_;
  }

  MockSessionSyncService* session_sync_service() {
    return session_sync_service_;
  }

  SendTabToSelfModelMock* send_tab_to_self_model() {
    return send_tab_to_self_model_;
  }

  FaviconServiceMock* favicon_service() { return favicon_service_; }

  TestMediaController* media_controller() const {
    return media_controller_.get();
  }

  FakeVideoConferenceTrayController* vc_controller() const {
    return vc_controller_.get();
  }

  syncer::TestSyncService* sync_service() { return sync_service_; }

  BirchKeyedService* birch_keyed_service() { return birch_keyed_service_; }

  ScopedTestMountPoint* mount_point() { return mount_point_.get(); }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {
        TestingProfile::TestingFactory{
            SyncServiceFactory::GetInstance(),
            base::BindRepeating(&BuildTestSyncService)},
        TestingProfile::TestingFactory{
            FileSuggestKeyedServiceFactory::GetInstance(),
            base::BindRepeating(
                &MockFileSuggestKeyedService::BuildMockFileSuggestKeyedService,
                temp_dir_.GetPath())},
        TestingProfile::TestingFactory{
            SessionSyncServiceFactory::GetInstance(),
            base::BindRepeating(&BuildMockSessionSyncService)},
        TestingProfile::TestingFactory{
            SendTabToSelfSyncServiceFactory::GetInstance(),
            base::BindRepeating(&BuildTestSendTabToSelfSyncService)},
        TestingProfile::TestingFactory{
            FaviconServiceFactory::GetInstance(),
            base::BindRepeating(&BuildFaviconServiceMock)},
    };
  }

  sync_preferences::TestingPrefServiceSyncable* GetDefaultPrefs() const {
    return profile()->GetTestingPrefService();
  }

 private:
  user_manager::TypedScopedUserManager<FakeChromeUserManager>
      fake_user_manager_;

  base::ScopedTempDir temp_dir_;

  std::unique_ptr<ScopedTestMountPoint> mount_point_;

  raw_ptr<MockFileSuggestKeyedService> file_suggest_service_ = nullptr;

  raw_ptr<BirchKeyedService> birch_keyed_service_ = nullptr;

  raw_ptr<MockSessionSyncService> session_sync_service_;

  raw_ptr<syncer::TestSyncService> sync_service_;

  raw_ptr<SendTabToSelfModelMock> send_tab_to_self_model_;

  raw_ptr<FaviconServiceMock> favicon_service_;

  std::unique_ptr<TestMediaController> media_controller_;

  std::unique_ptr<FakeVideoConferenceTrayController> vc_controller_;

  MockOpenTabsUIDelegate open_tabs_delegate_;

  std::unique_ptr<ReleaseNotesStorage> release_notes_storage_;

  base::test::ScopedFeatureList feature_list_;
};

// A test harness that enables focus mode.
class BirchKeyedServiceFocusModeTest : public BirchKeyedServiceTest {
 public:
  BirchKeyedServiceFocusModeTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kFocusMode);
  }
  BirchKeyedServiceFocusModeTest(const BirchKeyedServiceFocusModeTest&) =
      delete;
  BirchKeyedServiceFocusModeTest& operator=(
      const BirchKeyedServiceFocusModeTest&) = delete;
  ~BirchKeyedServiceFocusModeTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(BirchKeyedServiceTest, HasDataProviders) {
  WaitUntilFileSuggestServiceReady(
      ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          GetProfile()));
  EXPECT_TRUE(birch_keyed_service()->GetCalendarProvider());
  EXPECT_TRUE(birch_keyed_service()->GetFileSuggestProvider());
  EXPECT_TRUE(birch_keyed_service()->GetRecentTabsProvider());
  EXPECT_TRUE(birch_keyed_service()->GetSelfShareProvider());
  EXPECT_TRUE(birch_keyed_service()->GetLostMediaProvider());
}

TEST_F(BirchKeyedServiceTest, BirchFileSuggestProvider) {
  WaitUntilFileSuggestServiceReady(
      ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          GetProfile()));

  EXPECT_EQ(Shell::Get()->birch_model()->GetFileSuggestItemsForTest().size(),
            0u);

  const base::FilePath file_path_1 = mount_point()->CreateArbitraryFile();
  const base::FilePath file_path_2 = mount_point()->CreateArbitraryFile();

  file_suggest_service()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kDriveFile, file_path_1,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt},
          {FileSuggestionType::kDriveFile, file_path_2,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});

  birch_keyed_service()
      ->GetFileSuggestProviderForTest()
      ->OnFileSuggestionUpdated(FileSuggestionType::kDriveFile);

  task_environment()->RunUntilIdle();

  // Check that the birch model now has two file suggestions.
  EXPECT_EQ(Shell::Get()->birch_model()->GetFileSuggestItemsForTest().size(),
            2u);
}

TEST_F(BirchKeyedServiceTest, BirchFileSuggestProvider_NoFilesAvailable) {
  WaitUntilFileSuggestServiceReady(
      ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          GetProfile()));

  BirchModel* model = Shell::Get()->birch_model();
  model->SetCalendarItems({});
  model->SetRecentTabItems({});
  model->SetLastActiveItems({});
  model->SetMostVisitedItems({});
  model->SetSelfShareItems({});
  model->SetLostMediaItems({});
  model->SetWeatherItems({});
  model->SetReleaseNotesItems({});
  model->SetAttachmentItems({});

  // Trigger a file update, with no available files.
  birch_keyed_service()
      ->GetFileSuggestProviderForTest()
      ->OnFileSuggestionUpdated(FileSuggestionType::kDriveFile);
  task_environment()->RunUntilIdle();

  // Check that model data is not fresh, because no file items have yet
  // been provided.
  EXPECT_EQ(model->GetFileSuggestItemsForTest().size(), 0u);
  EXPECT_FALSE(model->IsDataFresh());

  const base::FilePath file_path_1 = mount_point()->CreateArbitraryFile();

  // Once file suggest data has been set and updated, the data should be marked
  // fresh after the file provider has notified of the change.
  file_suggest_service()->SetSuggestionsForType(
      FileSuggestionType::kDriveFile,
      /*suggestions=*/std::vector<FileSuggestData>{
          {FileSuggestionType::kDriveFile, file_path_1,
           /*title=*/std::nullopt,
           /*new_prediction_reason=*/std::nullopt,
           /*modified_time=*/std::nullopt,
           /*viewed_time=*/std::nullopt,
           /*shared_time=*/std::nullopt,
           /*new_score=*/std::nullopt,
           /*drive_file_id=*/std::nullopt,
           /*icon_url=*/std::nullopt}});
  birch_keyed_service()
      ->GetFileSuggestProviderForTest()
      ->OnFileSuggestionUpdated(FileSuggestionType::kDriveFile);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(Shell::Get()->birch_model()->GetFileSuggestItemsForTest().size(),
            1u);
  EXPECT_TRUE(model->IsDataFresh());
}

TEST_F(BirchKeyedServiceTest, BirchRecentTabProvider) {
  WaitUntilFileSuggestServiceReady(
      ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          GetProfile()));

  // No tabs data exists before a data fetch has occurred.
  EXPECT_EQ(Shell::Get()->birch_model()->GetTabsForTest().size(), 0u);

  // Request birch data fetch, then verify that tabs data is correct.
  birch_keyed_service()->GetRecentTabsProvider()->RequestBirchDataFetch();

  auto& tabs = Shell::Get()->birch_model()->GetTabsForTest();
  ASSERT_EQ(tabs.size(), 2u);

  EXPECT_EQ(tabs[0].title(), kTabTitle1);
  EXPECT_EQ(tabs[0].url(), GURL(kExampleURL1));
  EXPECT_EQ(tabs[0].session_name(), kSessionName1);
  EXPECT_EQ(tabs[0].form_factor(), BirchTabItem::DeviceFormFactor::kDesktop);
  EXPECT_EQ(tabs[0].secondary_icon_type(), SecondaryIconType::kTabFromDesktop);

  EXPECT_EQ(tabs[1].title(), kTabTitle2);
  EXPECT_EQ(tabs[1].url(), GURL(kExampleURL2));
  EXPECT_EQ(tabs[1].session_name(), kSessionName2);
  EXPECT_EQ(tabs[1].form_factor(), BirchTabItem::DeviceFormFactor::kPhone);
  EXPECT_EQ(tabs[1].secondary_icon_type(), SecondaryIconType::kTabFromPhone);

  // Disable tab sync, then try fetching again and expect an empty list of tabs.
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything = */ false, /*types=*/{});
  birch_keyed_service()->GetRecentTabsProvider()->RequestBirchDataFetch();
  EXPECT_EQ(Shell::Get()->birch_model()->GetTabsForTest().size(), 0u);
  EXPECT_TRUE(session_sync_service()->IsSubscribersEmpty());
}

TEST_F(BirchKeyedServiceTest, ReleaseNotesProvider) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* release_notes_provider =
      birch_keyed_service()->GetReleaseNotesProvider();

  SetUpReleaseNotesStorage();
  MakePrimaryAccountAvailable();

  EXPECT_EQ(model->GetReleaseNotesItemsForTest().size(), 0u);

  release_notes_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetSelfShareItems(std::vector<BirchSelfShareItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  auto& release_notes_items = model->GetReleaseNotesItemsForTest();

  ASSERT_EQ(release_notes_items.size(), 1u);
  EXPECT_EQ(release_notes_items[0].title(), u"Chromebook Updated");
  EXPECT_EQ(release_notes_items[0].subtitle(), u"See what's new");
  EXPECT_EQ(release_notes_items[0].url(), GURL("chrome://help-app/updates"));
  EXPECT_EQ(GetProfile()->GetPrefs()->GetInteger(
                ::prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
            3);

  MarkMilestoneUpToDate();
  MarkReleaseNotesSurfacesTimesLeftToShow(1);
  task_environment()->FastForwardBy(base::Hours(23));

  release_notes_provider->RequestBirchDataFetch();
  model->SetCalendarItems({});
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetSelfShareItems(std::vector<BirchSelfShareItem>());

  EXPECT_EQ(model->GetReleaseNotesItemsForTest().size(), 1u);
  EXPECT_EQ(GetProfile()->GetPrefs()->GetInteger(
                ::prefs::kHelpAppNotificationLastShownMilestone),
            GetCurrentMilestone());
  EXPECT_EQ(GetProfile()->GetPrefs()->GetInteger(
                ::prefs::kReleaseNotesSuggestionChipTimesLeftToShow),
            1);

  ClearReleaseNotesSurfacesTimesLeftToShowPref();

  release_notes_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetSelfShareItems(std::vector<BirchSelfShareItem>());

  EXPECT_EQ(model->GetReleaseNotesItemsForTest().size(), 0u);
  EXPECT_EQ(GetProfile()->GetPrefs()->GetInteger(
                ::prefs::kHelpAppNotificationLastShownMilestone),
            GetCurrentMilestone());
  EXPECT_TRUE(
      GetProfile()
          ->GetPrefs()
          ->FindPreference(::prefs::kReleaseNotesSuggestionChipTimesLeftToShow)
          ->IsDefaultValue());
}

TEST_F(BirchKeyedServiceTest, BirchRecentTabsWaitForForeignSessionsChange) {
  SetSessionServiceToReturnOpenTabsDelegate(false);

  // Request tab data, and check that no tabs are set when no open tabs delegate
  // is available.
  birch_keyed_service()->GetRecentTabsProvider()->RequestBirchDataFetch();
  EXPECT_EQ(Shell::Get()->birch_model()->GetTabsForTest().size(), 0u);
  EXPECT_FALSE(session_sync_service()->IsSubscribersEmpty());

  SetSessionServiceToReturnOpenTabsDelegate(true);

  // Notify session service of foreign session change, and check that tabs have
  // been set by the recent tabs provider.
  session_sync_service()->NotifyMockForeignSessionsChanged();
  EXPECT_EQ(Shell::Get()->birch_model()->GetTabsForTest().size(), 2u);
  EXPECT_TRUE(session_sync_service()->IsSubscribersEmpty());
}

TEST_F(BirchKeyedServiceTest, SelfShareProvider_FromTablet) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* self_share_provider =
      birch_keyed_service()->GetSelfShareProvider();

  EXPECT_EQ(model->GetSelfShareItemsForTest().size(), 0u);

  AddNewChromeSyncEntry();
  send_tab_to_self_model()->AddMockTargetDevice(
      syncer::DeviceInfo::FormFactor::kTablet);
  self_share_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetReleaseNotesItems(std::vector<BirchReleaseNotesItem>());
  model->SetLostMediaItems(std::vector<BirchLostMediaItem>());
  auto& self_share_items = model->GetSelfShareItemsForTest();
  EXPECT_EQ(self_share_items.size(), 1u);
  EXPECT_EQ(self_share_items[0].title(), u"Chrome Sync Title");
  EXPECT_EQ(self_share_items[0].secondary_icon_type(),
            SecondaryIconType::kTabFromTablet);

  // Mark Self Share Item as opened, the provider should now return zero items.
  model->GetSelfShareItemsForTest()[0].PerformAction(/*is_post_login=*/false);
  self_share_provider->RequestBirchDataFetch();
  EXPECT_EQ(model->GetSelfShareItemsForTest().size(), 0u);
}

TEST_F(BirchKeyedServiceTest, SelfShareProvider_FromPhone) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* self_share_provider =
      birch_keyed_service()->GetSelfShareProvider();

  EXPECT_EQ(model->GetSelfShareItemsForTest().size(), 0u);

  AddNewChromeSyncEntry();
  send_tab_to_self_model()->AddMockTargetDevice(
      syncer::DeviceInfo::FormFactor::kPhone);
  self_share_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetReleaseNotesItems(std::vector<BirchReleaseNotesItem>());
  model->SetLostMediaItems(std::vector<BirchLostMediaItem>());
  auto& self_share_items = model->GetSelfShareItemsForTest();
  EXPECT_EQ(self_share_items.size(), 1u);
  EXPECT_EQ(self_share_items[0].secondary_icon_type(),
            SecondaryIconType::kTabFromPhone);

  // Mark Self Share Item as opened, the provider should now return zero items.
  model->GetSelfShareItemsForTest()[0].PerformAction(/*is_post_login=*/false);
  self_share_provider->RequestBirchDataFetch();
  EXPECT_EQ(model->GetSelfShareItemsForTest().size(), 0u);
}

TEST_F(BirchKeyedServiceTest, SelfShareProvider_FromDesktop) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* self_share_provider =
      birch_keyed_service()->GetSelfShareProvider();

  EXPECT_EQ(model->GetSelfShareItemsForTest().size(), 0u);

  AddNewChromeSyncEntry();
  send_tab_to_self_model()->AddMockTargetDevice(
      syncer::DeviceInfo::FormFactor::kDesktop);
  self_share_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetReleaseNotesItems(std::vector<BirchReleaseNotesItem>());
  model->SetLostMediaItems(std::vector<BirchLostMediaItem>());
  auto& self_share_items = model->GetSelfShareItemsForTest();
  EXPECT_EQ(self_share_items.size(), 1u);
  EXPECT_EQ(self_share_items[0].secondary_icon_type(),
            SecondaryIconType::kTabFromDesktop);

  // Mark Self Share Item as opened, the provider should now return zero items.
  model->GetSelfShareItemsForTest()[0].PerformAction(/*is_post_login=*/false);
  self_share_provider->RequestBirchDataFetch();
  EXPECT_EQ(model->GetSelfShareItemsForTest().size(), 0u);
}

TEST_F(BirchKeyedServiceTest, LostMediaProvider_AudioItem) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* lost_media_provider =
      birch_keyed_service()->GetLostMediaProvider();
  ClearMediaApps();

  EXPECT_EQ(model->GetLostMediaItemsForTest().size(), 0u);

  SimulateMediaMetadataInit();
  SimulateMediaSessionInfoChanged(/*is_playing=*/true,
                                  SecondaryIconType::kLostMediaAudio);
  lost_media_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetReleaseNotesItems(std::vector<BirchReleaseNotesItem>());
  model->SetSelfShareItems(std::vector<BirchSelfShareItem>());

  auto& lost_media_items = model->GetLostMediaItemsForTest();
  EXPECT_EQ(lost_media_items.size(), 1u);
  EXPECT_EQ(lost_media_items[0].source_url(),
            GURL(kSessionMetadataSourceTitleFull));
  EXPECT_EQ(lost_media_items[0].title(), kSessionMetadataTitle);
  EXPECT_EQ(lost_media_items[0].secondary_icon_type(),
            SecondaryIconType::kLostMediaAudio);

  // Media item should still show after activation.
  lost_media_items[0].PerformAction(/*is_post_login=*/false);
  lost_media_items = model->GetLostMediaItemsForTest();
  lost_media_provider->RequestBirchDataFetch();
  EXPECT_EQ(lost_media_items.size(), 1u);
  EXPECT_EQ(lost_media_items[0].source_url(),
            GURL(kSessionMetadataSourceTitleFull));
  EXPECT_EQ(lost_media_items[0].title(), kSessionMetadataTitle);
  EXPECT_EQ(lost_media_items[0].secondary_icon_type(),
            SecondaryIconType::kLostMediaAudio);

  // There should be no items if metadata does not have a valid `source_url`
  // or `title`.
  SimulateMediaMetadataEnd();
  lost_media_provider->RequestBirchDataFetch();
  lost_media_items = model->GetLostMediaItemsForTest();
  ASSERT_EQ(lost_media_items.size(), 0u);
}

TEST_F(BirchKeyedServiceTest, LostMediaProvider_PausedDoesNotShow) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* lost_media_provider =
      birch_keyed_service()->GetLostMediaProvider();
  ClearMediaApps();

  // Simulate a piece of media.
  SimulateMediaMetadataInit();

  // Simulate a paused audio stream.
  SimulateMediaSessionInfoChanged(/*is_playing=*/false,
                                  SecondaryIconType::kLostMediaAudio);
  lost_media_provider->RequestBirchDataFetch();
  auto& lost_media_items = model->GetLostMediaItemsForTest();

  // The media item does not appear in the model.
  EXPECT_EQ(lost_media_items.size(), 0u);

  // Simulate a playing audio stream.
  SimulateMediaSessionInfoChanged(/*is_playing=*/true,
                                  SecondaryIconType::kLostMediaAudio);
  lost_media_provider->RequestBirchDataFetch();
  lost_media_items = model->GetLostMediaItemsForTest();

  // The media item appears in the model.
  EXPECT_EQ(lost_media_items.size(), 1u);
}

TEST_F(BirchKeyedServiceFocusModeTest, LostMediaProvider_FocusModeDoesNotShow) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* lost_media_provider =
      birch_keyed_service()->GetLostMediaProvider();
  ClearMediaApps();

  // Simulate a playing audio stream.
  SimulateMediaMetadataInit();
  SimulateMediaSessionInfoChanged(/*is_playing=*/true,
                                  SecondaryIconType::kLostMediaAudio);

  // Simulate that the audio is coming from a focus mode playlist.
  focus_mode_util::SelectedPlaylist playlist;
  playlist.id = "123";
  playlist.state = focus_mode_util::SoundState::kPlaying;
  FocusModeController::Get()
      ->focus_mode_sounds_controller()
      ->set_selected_playlist_for_testing(playlist);

  // Fetch the birch items.
  lost_media_provider->RequestBirchDataFetch();
  auto& lost_media_items = model->GetLostMediaItemsForTest();

  // The lost media item does not appear in the model.
  EXPECT_EQ(lost_media_items.size(), 0u);
}

TEST_F(BirchKeyedServiceTest, LostMediaProvider_VideoItem) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* lost_media_provider =
      birch_keyed_service()->GetLostMediaProvider();
  ClearMediaApps();

  EXPECT_EQ(model->GetLostMediaItemsForTest().size(), 0u);

  SimulateMediaMetadataInit();
  SimulateMediaSessionInfoChanged(/*is_playing=*/true,
                                  SecondaryIconType::kLostMediaVideo);
  lost_media_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetReleaseNotesItems(std::vector<BirchReleaseNotesItem>());
  model->SetSelfShareItems(std::vector<BirchSelfShareItem>());

  auto& lost_media_items = model->GetLostMediaItemsForTest();
  EXPECT_EQ(lost_media_items.size(), 1u);
  EXPECT_EQ(lost_media_items[0].source_url(),
            GURL(kSessionMetadataSourceTitleFull));
  EXPECT_EQ(lost_media_items[0].title(), kSessionMetadataTitle);
  EXPECT_EQ(lost_media_items[0].secondary_icon_type(),
            SecondaryIconType::kLostMediaVideo);

  // Media item should still show after activation.
  lost_media_items[0].PerformAction(/*is_post_login=*/false);
  lost_media_items = model->GetLostMediaItemsForTest();
  lost_media_provider->RequestBirchDataFetch();
  EXPECT_EQ(lost_media_items.size(), 1u);
  EXPECT_EQ(lost_media_items[0].source_url(),
            GURL(kSessionMetadataSourceTitleFull));
  EXPECT_EQ(lost_media_items[0].title(), kSessionMetadataTitle);
  EXPECT_EQ(lost_media_items[0].secondary_icon_type(),
            SecondaryIconType::kLostMediaVideo);

  // There should be no items if metadata does not have a valid `source_url`
  // or `title`.
  SimulateMediaMetadataEnd();
  lost_media_provider->RequestBirchDataFetch();
  lost_media_items = model->GetLostMediaItemsForTest();
  ASSERT_EQ(lost_media_items.size(), 0u);
}

TEST_F(BirchKeyedServiceTest, LostMediaProvider_VideoConferenceItem) {
  BirchModel* model = Shell::Get()->birch_model();
  BirchDataProvider* lost_media_provider =
      birch_keyed_service()->GetLostMediaProvider();
  ClearMediaApps();

  EXPECT_EQ(model->GetLostMediaItemsForTest().size(), 0u);

  // There should be one video conference item if there is both vc and
  // media items available.
  SimulateMediaMetadataInit();
  AddMediaApp();
  lost_media_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetReleaseNotesItems(std::vector<BirchReleaseNotesItem>());
  model->SetSelfShareItems(std::vector<BirchSelfShareItem>());

  auto& lost_media_items = model->GetLostMediaItemsForTest();
  ASSERT_EQ(lost_media_items.size(), 1u);
  EXPECT_EQ(lost_media_items[0].source_url(), GURL(kMediaAppUrl));
  EXPECT_EQ(lost_media_items[0].title(), kMediaAppTitle);
  EXPECT_EQ(lost_media_items[0].secondary_icon_type(),
            SecondaryIconType::kLostMediaVideoConference);

  // VC item still should show after activation.
  lost_media_items[0].PerformAction(/*is_post_login=*/false);
  lost_media_items = model->GetLostMediaItemsForTest();
  lost_media_provider->RequestBirchDataFetch();
  model->SetCalendarItems(std::vector<BirchCalendarItem>());
  model->SetRecentTabItems(std::vector<BirchTabItem>());
  model->SetFileSuggestItems(std::vector<BirchFileItem>());
  model->SetReleaseNotesItems(std::vector<BirchReleaseNotesItem>());
  model->SetSelfShareItems(std::vector<BirchSelfShareItem>());
  ASSERT_EQ(lost_media_items.size(), 1u);
  EXPECT_EQ(lost_media_items[0].source_url(), GURL(kMediaAppUrl));
  EXPECT_EQ(lost_media_items[0].title(), kMediaAppTitle);
  EXPECT_EQ(lost_media_items[0].secondary_icon_type(),
            SecondaryIconType::kLostMediaVideoConference);
}

TEST_F(BirchKeyedServiceTest, NoTabSuggestionsWithDisabledChromeSyncPref) {
  BirchModel* model = Shell::Get()->birch_model();

  // Request birch data fetch, verify that tabs are populated.
  birch_keyed_service()->GetRecentTabsProvider()->RequestBirchDataFetch();
  AddNewChromeSyncEntry();
  birch_keyed_service()->GetSelfShareProvider()->RequestBirchDataFetch();
  EXPECT_EQ(model->GetTabsForTest().size(), 2u);
  EXPECT_EQ(model->GetSelfShareItemsForTest().size(), 1u);

  // Disable ChromeSync integrations by policy, no tabs should be fetched.
  GetDefaultPrefs()->SetList(prefs::kContextualGoogleIntegrationsConfiguration,
                             {});
  birch_keyed_service()->GetRecentTabsProvider()->RequestBirchDataFetch();
  birch_keyed_service()->GetSelfShareProvider()->RequestBirchDataFetch();
  EXPECT_EQ(model->GetTabsForTest().size(), 0u);
  EXPECT_EQ(model->GetSelfShareItemsForTest().size(), 0u);
}

TEST_F(BirchKeyedServiceTest, RemoveFileItemFromLauncher) {
  WaitUntilFileSuggestServiceReady(
      ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
          GetProfile()));

  // Override the default behavior in MockFileSuggestKeyedService, which calls
  // into the production code and causes failures.
  ON_CALL(*file_suggest_service(), RemoveSuggestionsAndNotify(testing::_))
      .WillByDefault([](const std::vector<base::FilePath>& paths) {
        // Do nothing.
      });

  base::FilePath test_path(
      "/media/fuse/drivefs-48de6bc248c2f6d8e809521347ef6190/root/Test "
      "doc.gdoc");
  std::vector<base::FilePath> paths = {test_path};

  // Removing a file item via the birch keyed service will call into file
  // suggest keyed service and remove it.
  EXPECT_CALL(*file_suggest_service(), RemoveSuggestionsAndNotify(paths));
  birch_keyed_service()->RemoveFileItemFromLauncher(test_path);
}

// Verifies that `GetFaviconImage` for icon urls calls `GetRawFavicon` in
// favicon service.
TEST_F(BirchKeyedServiceTest, GetFaviconImage_ForIconUrl) {
  GURL icon_url("http://example.com/favicon.ico");
  EXPECT_CALL(
      *favicon_service(),
      GetRawFavicon(icon_url, testing::_, testing::_, testing::_, testing::_));
  birch_keyed_service()->GetFaviconImage(icon_url, /*is_page_url=*/false,
                                         base::DoNothing());
}

// Verifies that `GetFaviconImage` for page urls calls
// `GetLargestRawFaviconForPageURL` in favicon service.
TEST_F(BirchKeyedServiceTest, GetFaviconImage_ForPageUrl) {
  GURL page_url("http://example.com/");
  EXPECT_CALL(*favicon_service(),
              GetLargestRawFaviconForPageURL(page_url, testing::_, testing::_,
                                             testing::_, testing::_));
  birch_keyed_service()->GetFaviconImage(page_url, /*is_page_url=*/true,
                                         base::DoNothing());
}

}  // namespace ash
