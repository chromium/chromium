// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_factory.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_builder.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile_key.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/testing_pref_store.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/zoom_level_delegate.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "net/cookies/cookie_store.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/service_manager/public/cpp/service.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_system.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/net/delay_network_call.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#endif

using base::Time;
using content::BrowserThread;
using content::DownloadManagerDelegate;
using testing::NiceMock;
using testing::Return;

namespace {

std::unique_ptr<KeyedService> BuildPersonalDataManagerInstanceFor(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      autofill::PersonalDataManagerFactory::BuildPersonalDataManager(context));
}

}  // namespace

// static
const char TestingProfile::kDefaultProfileUserName[] = "testing_profile@test";

// static
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Must be kept in sync with
// `ChromeBrowserMainPartsAsh::PreEarlyInitialization`.
const char TestingProfile::kTestUserProfileDir[] = "test-user";
#else
const char TestingProfile::kTestUserProfileDir[] = "Default";
#endif

TestingProfile::TestingProfile() : TestingProfile(base::FilePath()) {}

TestingProfile::TestingProfile(const base::FilePath& path)
    : TestingProfile(path, nullptr) {}

TestingProfile::TestingProfile(const base::FilePath& path, Delegate* delegate)
    : profile_path_(path), delegate_(delegate) {
  if (profile_path_.empty()) {
    profile_path_ = base::CreateUniqueTempDirectoryScopedToTest();
  }
  Init(/*is_supervised_profile=*/false);
  if (delegate_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestingProfile::FinishInit, base::Unretained(this)));
  } else {
    FinishInit();
  }
}

TestingProfile::TestingProfile(
    const base::FilePath& path,
    Delegate* delegate,
#if BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy,
#endif
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs,
    TestingProfile* parent,
    bool guest_session,
    bool allows_browser_windows,
    bool is_new_profile,
    bool is_supervised_profile,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    bool is_main_profile,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::unique_ptr<policy::UserCloudPolicyManagerAsh> policy_manager,
#else
    std::unique_ptr<policy::UserCloudPolicyManager> policy_manager,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    std::unique_ptr<policy::PolicyService> policy_service,
    TestingFactories testing_factories,
    const std::string& profile_name,
    absl::optional<bool> override_policy_connector_is_managed,
    absl::optional<OTRProfileID> otr_profile_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : prefs_(std::move(prefs)),
      original_profile_(parent),
      guest_session_(guest_session),
      allows_browser_windows_(allows_browser_windows),
      is_new_profile_(is_new_profile),
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      is_main_profile_(is_main_profile),
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_special_storage_policy_(extension_policy),
#endif
      profile_path_(path),
      user_cloud_policy_manager_(std::move(policy_manager)),
      delegate_(delegate),
      profile_name_(profile_name),
      override_policy_connector_is_managed_(
          override_policy_connector_is_managed),
      otr_profile_id_(otr_profile_id),
      policy_service_(std::move(policy_service)),
      url_loader_factory_(url_loader_factory) {
  if (parent)
    parent->SetOffTheRecordProfile(std::unique_ptr<Profile>(this));

  // Only OffTheRecord profiles have an OTRProfileID.
  DCHECK(!parent || otr_profile_id_.has_value());

  // If no profile path was supplied, create one.
  if (profile_path_.empty()) {
    profile_path_ = base::CreateUniqueTempDirectoryScopedToTest();
  }

  // Set any testing factories prior to initializing the services.
  for (TestingFactories::value_type& pair : testing_factories)
    pair.first->SetTestingFactory(this, std::move(pair.second));
  testing_factories.clear();

  Init(is_supervised_profile);

  // If caller supplied a delegate, delay the FinishInit invocation until other
  // tasks have run.
  // TODO(atwilson): See if this is still required once we convert the current
  // users of the constructor that takes a Delegate* param.
  if (delegate_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestingProfile::FinishInit, base::Unretained(this)));
  } else {
    FinishInit();
  }
}

void TestingProfile::Init(bool is_supervised_profile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // If threads have been initialized, we should be on the UI thread.
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  InitializeProfileType();

  if (IsOffTheRecord()) {
    key_ = std::make_unique<TestingProfileKey>(
        this, original_profile_->GetPath(), original_profile_->GetProfileKey());
  } else {
    key_ = std::make_unique<TestingProfileKey>(this, profile_path_);
  }

#if BUILDFLAG(IS_ANDROID)
  signin::SetUpMockAccountManagerFacade();
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // `LacrosService` has to be initialized before
  // `EnsureBrowserContextKeyedServiceFactoriesBuilt` call.
  if (!chromeos::LacrosService::Get()) {
    lacros_service_test_helper_ =
        std::make_unique<chromeos::ScopedLacrosServiceTestHelper>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Normally this would happen during browser startup, but for tests
  // we need to trigger creation of Profile-related services.
  ChromeBrowserMainExtraPartsProfiles::
      EnsureBrowserContextKeyedServiceFactoriesBuilt();

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (!IsOffTheRecord()) {
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForKey(key_.get());
    supervised_user_pref_store_ = new TestingPrefStore();
    settings_service->Init(supervised_user_pref_store_.get());
    settings_service->MergeDataAndStartSyncing(
        syncer::SUPERVISED_USER_SETTINGS, syncer::SyncDataList(),
        std::unique_ptr<syncer::SyncChangeProcessor>(
            new syncer::FakeSyncChangeProcessor));

    supervised_user_pref_store_->SetInitializationCompleted();
  }
#endif

  if (prefs_.get())
    user_prefs::UserPrefs::Set(this, prefs_.get());
  else if (IsOffTheRecord())
    CreateIncognitoPrefService();
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  else if (is_supervised_profile)
    CreatePrefServiceForSupervisedUser();
#endif
  else
    CreateTestingPrefService();

  if (is_supervised_profile)
    SetIsSupervisedProfile();

  key_->SetPrefs(prefs_.get());
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());

  if (!base::PathExists(profile_path_))
    base::CreateDirectory(profile_path_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Initialize |account_manager::AccountManager|.
  auto* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  auto* account_manager = factory->GetAccountManager(profile_path_.value());
  account_manager::AccountManager::DelayNetworkCallRunner
      immediate_callback_runner = base::BindRepeating(
          [](base::OnceClosure closure) -> void { std::move(closure).Run(); });
  account_manager->Initialize(profile_path_, GetURLLoaderFactory(),
                              immediate_callback_runner);
  account_manager->SetPrefService(GetPrefs());
  if (!ash::CrosSettings::IsInitialized()) {
    scoped_cros_settings_test_helper_ =
        std::make_unique<ash::ScopedCrosSettingsTestHelper>();
  }
  arc::ArcServiceLauncher* launcher = arc::ArcServiceLauncher::Get();
  if (launcher)
    launcher->MaybeSetProfile(this);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (!AreKeyedServicesDisabledForProfileByDefault(this)) {
    autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        this, base::BindRepeating(&BuildPersonalDataManagerInstanceFor));

    ReadingListModelFactory::GetInstance()->SetTestingFactory(
        this, ReadingListModelFactory::GetDefaultFactoryForTesting());

    // TODO(joaodasilva): remove this once this PKS isn't created in ProfileImpl
    // anymore, after converting the PrefService to a PKS. Until then it must
    // be associated with a TestingProfile too.
    if (!IsOffTheRecord())
      CreateProfilePolicyConnector();

    extensions_path_ = profile_path_.AppendASCII("Extensions");

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Note that the GetPrefs() creates a TestingPrefService, therefore
    // the extension controlled pref values set in ExtensionPrefs
    // are not reflected in the pref service. One would need to
    // inject a new ExtensionPrefStore(extension_pref_value_map, false).
    bool extensions_disabled =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableExtensions);
    std::unique_ptr<extensions::ExtensionPrefs> extension_prefs(
        extensions::ExtensionPrefs::Create(
            this, GetPrefs(), extensions_path_,
            ExtensionPrefValueMapFactory::GetForBrowserContext(this),
            extensions_disabled,
            std::vector<extensions::EarlyExtensionPrefsObserver*>()));
    extensions::ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(
        this, std::move(extension_prefs));

    extensions::ExtensionSystemFactory::GetInstance()->SetTestingFactory(
        this, base::BindRepeating(&extensions::TestExtensionSystem::Build));

    extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
        this, BrowserContextKeyedServiceFactory::TestingFactory());

    web_app::WebAppProviderFactory::GetInstance()->SetTestingFactory(
        this, base::BindRepeating(&web_app::FakeWebAppProvider::BuildDefault));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ash::SystemWebAppManagerFactory::GetInstance()->SetTestingFactory(
        this, base::BindRepeating(&ash::TestSystemWebAppManager::BuildDefault));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  // Prefs for incognito profiles are set in CreateIncognitoPrefService().
  SimpleFactoryKey* key = GetProfileKey();
  if (!IsOffTheRecord()) {
    DCHECK(!original_profile_);
    user_prefs::PrefRegistrySyncable* pref_registry =
        static_cast<user_prefs::PrefRegistrySyncable*>(
            prefs_->DeprecatedGetPrefRegistry());
    simple_dependency_manager_->RegisterProfilePrefsForServices(pref_registry);
    browser_context_dependency_manager_->RegisterProfilePrefsForServices(
        pref_registry);
  }

  FullBrowserTransitionManager::Get()->OnProfileCreated(this);

  simple_dependency_manager_->CreateServicesForTest(key);
  browser_context_dependency_manager_->CreateBrowserContextServicesForTest(
      this);
}

void TestingProfile::InitializeProfileType() {
  if (guest_session_) {
    profile_metrics::SetBrowserProfileType(
        this, profile_metrics::BrowserProfileType::kGuest);
    return;
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  bool is_system = false;
  if (IsOffTheRecord()) {
    is_system = original_profile_->IsSystemProfile();
  } else {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    if (profile_manager) {
      is_system = (profile_path_ == profile_manager->GetSystemProfilePath());
    }
  }
  if (is_system) {
    profile_metrics::SetBrowserProfileType(
        this, profile_metrics::BrowserProfileType::kSystem);
    return;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

  if (IsOffTheRecord()) {
    profile_metrics::SetBrowserProfileType(
        this,
        (otr_profile_id_ == OTRProfileID::PrimaryID())
            ? profile_metrics::BrowserProfileType::kIncognito
            : profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile);
    return;
  }

  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kRegular);
}

void TestingProfile::FinishInit() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->InitProfileUserPrefs(this);

  if (original_profile_) {
    DCHECK(!delegate_) << "Not expecting a delegate for an OTR profile";
    original_profile_->NotifyOffTheRecordProfileCreated(this);
    return;
  }

  if (delegate_) {
    delegate_->OnProfileCreationFinished(this, CREATE_MODE_ASYNCHRONOUS, true,
                                         false);
  } else {
    // It is the role of the delegate to ensure that the signout allowed is
    // properly updated after the profile is create is initialized.
    // For testing profiles that do not have a delegate, the signout allowed
    // must be initialized when the testing profile finishes its
    // initialization.

    signin_util::UserSignoutSetting::GetForProfile(this)
        ->InitializeUserSignoutSettingIfNeeded();
  }
}

TestingProfile::~TestingProfile() {
  if (!profile_destruction_callback_.is_null())
    std::move(profile_destruction_callback_).Run();

  // If this profile owns OffTheRecord profiles, tear them down first.
  otr_profiles_.clear();

  // Any objects holding live URLFetchers should be deleted before teardown.
  TemplateURLFetcherFactory::ShutdownForProfile(this);

  MaybeSendDestroyedNotification();

  FullBrowserTransitionManager::Get()->OnProfileDestroyed(this);

  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService
  // instances in the BrowserContextDependencyManager's dependency graph can
  // depend on the ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      browser_context_dependency_manager_, this, simple_dependency_manager_,
      key_.get());
  key_.reset();

  SimpleKeyMap::GetInstance()->Dissociate(this);

  if (profile_policy_connector_)
    profile_policy_connector_->Shutdown();

  if (user_cloud_policy_manager_)
    user_cloud_policy_manager_->Shutdown();

  if (host_content_settings_map_.get())
    host_content_settings_map_->ShutdownOnUIThread();

  // Make sure SharedProtoDatabase doesn't post delayed tasks anymore.
  ForEachLoadedStoragePartition(
      base::BindRepeating([](content::StoragePartition* storage_partition) {
        if (auto* provider =
                storage_partition->GetProtoDatabaseProviderForTesting()) {
          provider->SetSharedDBDeleteObsoleteDelayForTesting(base::TimeDelta());
        }
      }));

  // Shutdown storage partitions before we post a task to delete
  // the resource context.
  ShutdownStoragePartitions();
}

void TestingProfile::SetGuestSession(bool guest) {
  guest_session_ = guest;
  InitializeProfileType();
}

void TestingProfile::SetIsNewProfile(bool is_new_profile) {
  is_new_profile_ = is_new_profile;
}

base::FilePath TestingProfile::GetPath() {
  return profile_path_;
}

base::FilePath TestingProfile::GetPath() const {
  return profile_path_;
}

base::Time TestingProfile::GetCreationTime() const {
  return start_time_;
}

std::unique_ptr<content::ZoomLevelDelegate>
TestingProfile::CreateZoomLevelDelegate(const base::FilePath& partition_path) {
  return std::make_unique<ChromeZoomLevelPrefs>(
      GetPrefs(), GetPath(), partition_path,
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}

scoped_refptr<base::SequencedTaskRunner> TestingProfile::GetIOTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

sync_preferences::TestingPrefServiceSyncable*
TestingProfile::GetTestingPrefService() {
  DCHECK(prefs_);
  DCHECK(testing_prefs_);
  return testing_prefs_;
}

TestingProfile* TestingProfile::AsTestingProfile() {
  return this;
}

std::string TestingProfile::GetProfileUserName() const {
  return profile_name_;
}

bool TestingProfile::IsOffTheRecord() {
  return original_profile_;
}

bool TestingProfile::IsOffTheRecord() const {
  return original_profile_;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool TestingProfile::IsMainProfile() const {
  return is_main_profile_;
}

void TestingProfile::SetIsMainProfile(bool is_main_profile) {
  is_main_profile_ = is_main_profile;
}

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

const Profile::OTRProfileID& TestingProfile::GetOTRProfileID() const {
  DCHECK(IsOffTheRecord());
  return *otr_profile_id_;
}

void TestingProfile::SetOffTheRecordProfile(
    std::unique_ptr<Profile> otr_profile) {
  DCHECK(otr_profile);
  DCHECK(!IsOffTheRecord());
  DCHECK_EQ(this, otr_profile->GetOriginalProfile());
  otr_profiles_[otr_profile->GetOTRProfileID()] = std::move(otr_profile);
}

Profile* TestingProfile::GetOffTheRecordProfile(
    const OTRProfileID& otr_profile_id,
    bool create_if_needed) {
  if (IsOffTheRecord()) {
    return original_profile_->GetOffTheRecordProfile(otr_profile_id,
                                                     create_if_needed);
  }

  if (!HasOffTheRecordProfile(otr_profile_id)) {
    if (!create_if_needed)
      return nullptr;

    TestingProfile::Builder builder;
    if (IsGuestSession() && otr_profile_id == OTRProfileID::PrimaryID())
      builder.SetGuestSession();
    builder.SetPath(GetPath());
    builder.BuildOffTheRecord(this, otr_profile_id);
    DCHECK(HasOffTheRecordProfile(otr_profile_id));
  }

  return otr_profiles_[otr_profile_id].get();
}

std::vector<Profile*> TestingProfile::GetAllOffTheRecordProfiles() {
  // This matches the implementation of OffTheRecordProfileImpl:
  if (IsOffTheRecord())
    return original_profile_->GetAllOffTheRecordProfiles();

  // This matches the implementation of ProfileImpl:
  std::vector<Profile*> otr_profiles;
  for (auto& otr : otr_profiles_)
    otr_profiles.push_back(otr.second.get());
  return otr_profiles;
}

void TestingProfile::DestroyOffTheRecordProfile(Profile* otr_profile) {
  if (HasOffTheRecordProfile(otr_profile->GetOTRProfileID()))
    otr_profiles_.erase(otr_profile->GetOTRProfileID());
}

bool TestingProfile::HasOffTheRecordProfile(
    const OTRProfileID& otr_profile_id) {
  return base::Contains(otr_profiles_, otr_profile_id);
}

bool TestingProfile::HasAnyOffTheRecordProfile() {
  return !otr_profiles_.empty();
}

Profile* TestingProfile::GetOriginalProfile() {
  if (original_profile_)
    return original_profile_;
  return this;
}

const Profile* TestingProfile::GetOriginalProfile() const {
  if (original_profile_)
    return original_profile_;
  return this;
}

void TestingProfile::SetIsSupervisedProfile() {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  GetPrefs()->SetString(prefs::kSupervisedUserId,
                        supervised_users::kChildAccountSUID);
#else
  NOTREACHED() << "Supervised users are not enabled";
#endif
}

bool TestingProfile::IsChild() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return GetPrefs()->GetString(prefs::kSupervisedUserId) ==
         supervised_users::kChildAccountSUID;
#else
  return false;
#endif
}

bool TestingProfile::AllowsBrowserWindows() const {
  return allows_browser_windows_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void TestingProfile::SetExtensionSpecialStoragePolicy(
    scoped_refptr<ExtensionSpecialStoragePolicy>
        extension_special_storage_policy) {
  extension_special_storage_policy_ =
      std::move(extension_special_storage_policy);
}
#endif

ExtensionSpecialStoragePolicy*
TestingProfile::GetExtensionSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!extension_special_storage_policy_.get()) {
    extension_special_storage_policy_ =
        base::MakeRefCounted<ExtensionSpecialStoragePolicy>(nullptr);
  }
  return extension_special_storage_policy_.get();
#else
  return nullptr;
#endif
}

void TestingProfile::CreateTestingPrefService() {
  DCHECK(!prefs_.get());
  testing_prefs_ = new sync_preferences::TestingPrefServiceSyncable();
  prefs_.reset(testing_prefs_);
  user_prefs::UserPrefs::Set(this, prefs_.get());
  RegisterUserProfilePrefs(testing_prefs_->registry());
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
void TestingProfile::CreatePrefServiceForSupervisedUser() {
  DCHECK(!prefs_.get());

  // Construct testing_prefs_ by hand to add the supervised user pref store.
  testing_prefs_ = new sync_preferences::TestingPrefServiceSyncable(
      /*managed_prefs=*/new TestingPrefStore, supervised_user_pref_store_,
      /*extension_prefs=*/new TestingPrefStore,
      /*standalone_browser_prefs=*/new TestingPrefStore,
      /*user_prefs=*/new TestingPrefStore,
      /*recommended_prefs=*/new TestingPrefStore,
      new user_prefs::PrefRegistrySyncable, new PrefNotifierImpl);
  prefs_.reset(testing_prefs_);
  user_prefs::UserPrefs::Set(this, prefs_.get());
  RegisterUserProfilePrefs(testing_prefs_->registry());
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

void TestingProfile::CreateIncognitoPrefService() {
  DCHECK(original_profile_);
  DCHECK(!testing_prefs_);
  // Simplified version of ProfileImpl::GetOffTheRecordPrefs(). Note this
  // leaves testing_prefs_ unset.
  prefs_ = CreateIncognitoPrefServiceSyncable(original_profile_->prefs_.get(),
                                              nullptr);
  user_prefs::UserPrefs::Set(this, prefs_.get());
}

void TestingProfile::CreateProfilePolicyConnector() {
  schema_registry_service_ =
      BuildSchemaRegistryServiceForProfile(this, policy::Schema(), nullptr);

  if (!policy_service_) {
    std::vector<policy::ConfigurationPolicyProvider*> providers;
    std::unique_ptr<policy::PolicyServiceImpl> policy_service =
        std::make_unique<policy::PolicyServiceImpl>(std::move(providers));
    policy_service_ = std::move(policy_service);
  }
  profile_policy_connector_ =
      std::make_unique<policy::ProfilePolicyConnector>();
  profile_policy_connector_->InitForTesting(std::move(policy_service_));
  if (override_policy_connector_is_managed_.has_value())
    profile_policy_connector_->OverrideIsManagedForTesting(
        override_policy_connector_is_managed_.value());
}

PrefService* TestingProfile::GetPrefs() {
  DCHECK(prefs_);
  return prefs_.get();
}

const PrefService* TestingProfile::GetPrefs() const {
  DCHECK(prefs_);
  return prefs_.get();
}

ChromeZoomLevelPrefs* TestingProfile::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetDefaultStoragePartition()->GetZoomLevelDelegate());
}

DownloadManagerDelegate* TestingProfile::GetDownloadManagerDelegate() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestingProfile::GetURLLoaderFactory() {
  return url_loader_factory_;
}

content::ResourceContext* TestingProfile::GetResourceContext() {
  // TODO(arthursonzogni): This should only be called on the IO thread. Consider
  // adding a DCHECK_CURRENTLY_ON(content::BrowserThread::IO) after fixing the
  // non compliant tests: SpellingMenuObserverTest.SuggestionsForceTopSeparator
  if (!resource_context_) {
    resource_context_ =
        std::unique_ptr<content::MockResourceContext,
                        content::BrowserThread::DeleteOnIOThread>(
            new content::MockResourceContext);
  }
  return resource_context_.get();
}

content::BrowserPluginGuestManager* TestingProfile::GetGuestManager() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return nullptr;
#endif
}

content::PlatformNotificationService*
TestingProfile::GetPlatformNotificationService() {
  return nullptr;
}

content::PushMessagingService* TestingProfile::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
TestingProfile::GetStorageNotificationService() {
  return nullptr;
}

bool TestingProfile::IsSameOrParent(Profile* profile) {
  if (this == profile)
    return true;
  return profile && profile->GetOriginalProfile() == this;
}

base::Time TestingProfile::GetStartTime() const {
  return start_time_;
}

ProfileKey* TestingProfile::GetProfileKey() const {
  DCHECK(key_);
  return key_.get();
}

policy::SchemaRegistryService*
TestingProfile::GetPolicySchemaRegistryService() {
  return schema_registry_service_.get();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
policy::UserCloudPolicyManagerAsh*
TestingProfile::GetUserCloudPolicyManagerAsh() {
  return user_cloud_policy_manager_.get();
}

policy::ActiveDirectoryPolicyManager*
TestingProfile::GetActiveDirectoryPolicyManager() {
  return nullptr;
}
#else
policy::UserCloudPolicyManager* TestingProfile::GetUserCloudPolicyManager() {
  return user_cloud_policy_manager_.get();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

policy::ProfilePolicyConnector* TestingProfile::GetProfilePolicyConnector() {
  // This matches OffTheRecordProfileImpl::GetProfilePolicyConnector()
  // implementation.
  if (IsOffTheRecord())
    return original_profile_->GetProfilePolicyConnector();

  return profile_policy_connector_.get();
}

const policy::ProfilePolicyConnector*
TestingProfile::GetProfilePolicyConnector() const {
  // This matches OffTheRecordProfileImpl::GetProfilePolicyConnector()
  // implementation.
  if (IsOffTheRecord())
    return original_profile_->GetProfilePolicyConnector();

  return profile_policy_connector_.get();
}

base::FilePath TestingProfile::last_selected_directory() {
  return last_selected_directory_;
}

void TestingProfile::set_last_selected_directory(const base::FilePath& path) {
  last_selected_directory_ = path;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void TestingProfile::ChangeAppLocale(const std::string& locale,
                                     AppLocaleChangedVia via) {
  requested_locale_ = locale;
}

ash::ScopedCrosSettingsTestHelper*
TestingProfile::ScopedCrosSettingsTestHelper() {
  return scoped_cros_settings_test_helper_.get();
}
#endif

void TestingProfile::BlockUntilHistoryProcessesPendingRequests() {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(this,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history::BlockUntilHistoryProcessesPendingRequests(history_service);
}

GURL TestingProfile::GetHomePage() {
  return GURL(chrome::kChromeUINewTabURL);
}

void TestingProfile::SetCreationTimeForTesting(base::Time creation_time) {
  start_time_ = creation_time;
}

bool TestingProfile::IsSignedIn() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(this);
  return identity_manager &&
         identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
}

storage::SpecialStoragePolicy* TestingProfile::GetSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return nullptr;
#endif
}

content::SSLHostStateDelegate* TestingProfile::GetSSLHostStateDelegate() {
  return StatefulSSLHostStateDelegateFactory::GetForProfile(this);
}

content::PermissionControllerDelegate*
TestingProfile::GetPermissionControllerDelegate() {
  return permission_controller_delegate_ ? permission_controller_delegate_.get()
                                         : nullptr;
}

content::ClientHintsControllerDelegate*
TestingProfile::GetClientHintsControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate* TestingProfile::GetBackgroundFetchDelegate() {
  return BackgroundFetchDelegateFactory::GetForProfile(this);
}

content::BackgroundSyncController*
TestingProfile::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
TestingProfile::GetBrowsingDataRemoverDelegate() {
  // TestingProfile contains a real BrowsingDataRemover from BrowserContext.
  // Since ChromeBrowsingDataRemoverDelegate is just a Chrome-specific extension
  // of BrowsingDataRemover, we include it here for consistency.
  //
  // This is not a problem, since ChromeBrowsingDataRemoverDelegate mostly
  // just serves as an interface to deletion mechanisms of various browsing
  // data backends, which are already mocked if considered too heavy-weight
  // for TestingProfile.
  return ChromeBrowsingDataRemoverDelegateFactory::GetForProfile(this);
}

content::ReduceAcceptLanguageControllerDelegate*
TestingProfile::GetReduceAcceptLanguageControllerDelegate() {
  return nullptr;
}

content::OriginTrialsControllerDelegate*
TestingProfile::GetOriginTrialsControllerDelegate() {
  return OriginTrialsFactory::GetForBrowserContext(this);
}

bool TestingProfile::WasCreatedByVersionOrLater(const std::string& version) {
  return true;
}

bool TestingProfile::IsGuestSession() const {
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kGuest;
}

bool TestingProfile::IsNewProfile() const {
  return is_new_profile_;
}

TestingProfile::Builder::Builder() = default;

TestingProfile::Builder::~Builder() = default;

TestingProfile::Builder& TestingProfile::Builder::SetPath(
    const base::FilePath& path) {
  path_ = path;
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::SetDelegate(
    Delegate* delegate) {
  delegate_ = delegate;
  return *this;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TestingProfile::Builder&
TestingProfile::Builder::SetExtensionSpecialStoragePolicy(
    scoped_refptr<ExtensionSpecialStoragePolicy> policy) {
  extension_policy_ = policy;
  return *this;
}
#endif

TestingProfile::Builder& TestingProfile::Builder::SetPrefService(
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs) {
  pref_service_ = std::move(prefs);
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::SetGuestSession() {
  guest_session_ = true;
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::DisallowBrowserWindows() {
  allows_browser_windows_ = false;
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::SetIsNewProfile(
    bool is_new_profile) {
  is_new_profile_ = is_new_profile;
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::SetIsSupervisedProfile() {
  is_supervised_profile_ = true;
  return *this;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TestingProfile::Builder& TestingProfile::Builder::SetIsMainProfile(
    bool is_main_profile) {
  is_main_profile_ = is_main_profile;
  return *this;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TestingProfile::Builder& TestingProfile::Builder::SetUserCloudPolicyManagerAsh(
    std::unique_ptr<policy::UserCloudPolicyManagerAsh>
        user_cloud_policy_manager) {
  user_cloud_policy_manager_ = std::move(user_cloud_policy_manager);
  return *this;
}
#else
TestingProfile::Builder& TestingProfile::Builder::SetUserCloudPolicyManager(
    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager) {
  user_cloud_policy_manager_ = std::move(user_cloud_policy_manager);
  return *this;
}
#endif

TestingProfile::Builder& TestingProfile::Builder::SetPolicyService(
    std::unique_ptr<policy::PolicyService> policy_service) {
  policy_service_ = std::move(policy_service);
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::SetProfileName(
    const std::string& profile_name) {
  profile_name_ = profile_name;
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::SetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
  return *this;
}

TestingProfile::Builder&
TestingProfile::Builder::OverridePolicyConnectorIsManagedForTesting(
    bool is_managed) {
  override_policy_connector_is_managed_ = is_managed;
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::AddTestingFactory(
    BrowserContextKeyedServiceFactory* service_factory,
    BrowserContextKeyedServiceFactory::TestingFactory testing_factory) {
  testing_factories_.emplace_back(service_factory, std::move(testing_factory));
  return *this;
}

TestingProfile::Builder& TestingProfile::Builder::AddTestingFactories(
    const TestingFactories& testing_factories) {
  testing_factories_.insert(testing_factories_.end(), testing_factories.begin(),
                            testing_factories.end());
  return *this;
}

std::unique_ptr<TestingProfile> TestingProfile::Builder::Build() {
  DCHECK(!build_called_);
  build_called_ = true;

  return std::make_unique<TestingProfile>(
      path_, delegate_,
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_policy_,
#endif
      std::move(pref_service_), nullptr, guest_session_,
      allows_browser_windows_, is_new_profile_, is_supervised_profile_,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      is_main_profile_,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
      std::move(user_cloud_policy_manager_), std::move(policy_service_),
      std::move(testing_factories_), profile_name_,
      override_policy_connector_is_managed_, absl::optional<OTRProfileID>(),
      url_loader_factory_);
}

TestingProfile* TestingProfile::Builder::BuildOffTheRecord(
    TestingProfile* original_profile,
    const OTRProfileID& otr_profile_id) {
  DCHECK(!build_called_);
  DCHECK(original_profile);
  build_called_ = true;

  // Note: Owned by |original_profile|.
  return new TestingProfile(
      path_, delegate_,
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_policy_,
#endif
      std::move(pref_service_), original_profile, guest_session_,
      allows_browser_windows_, is_new_profile_, is_supervised_profile_,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      is_main_profile_,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
      std::move(user_cloud_policy_manager_), std::move(policy_service_),
      std::move(testing_factories_), profile_name_,
      override_policy_connector_is_managed_,
      absl::optional<OTRProfileID>(otr_profile_id), url_loader_factory_);
}

TestingProfile* TestingProfile::Builder::BuildIncognito(
    TestingProfile* original_profile) {
  return BuildOffTheRecord(original_profile, OTRProfileID::PrimaryID());
}
