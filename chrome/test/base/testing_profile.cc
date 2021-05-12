// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
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
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_builder.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile_key.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_profile_validator.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/history_index_restore_observer.h"
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
#include "components/sync/test/model/fake_sync_change_processor.h"
#include "components/sync/test/model/sync_error_factory_mock.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webdata_services/web_data_service_wrapper.h"
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
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
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
#include "ash/components/account_manager/account_manager.h"
#include "ash/components/account_manager/account_manager_factory.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/chromeos/net/delay_network_call.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

using base::Time;
using content::BrowserThread;
using content::DownloadManagerDelegate;
using testing::NiceMock;
using testing::Return;

namespace {

void TestProfileErrorCallback(WebDataServiceWrapper::ErrorType error_type,
                              sql::InitStatus status,
                              const std::string& diagnostics) {
  NOTREACHED();
}

std::unique_ptr<KeyedService> BuildWebDataService(
    content::BrowserContext* context) {
  const base::FilePath& context_path = context->GetPath();
  return std::make_unique<WebDataServiceWrapper>(
      context_path, g_browser_process->GetApplicationLocale(),
      content::GetUIThreadTaskRunner({}),
      base::BindRepeating(&TestProfileErrorCallback));
}

std::unique_ptr<KeyedService> BuildPersonalDataManagerInstanceFor(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      autofill::PersonalDataManagerFactory::BuildPersonalDataManager(
          autofill::TestAutofillProfileValidator::GetInstance(), context));
}

}  // namespace

// static
const char TestingProfile::kDefaultProfileUserName[] = "testing_profile";

// static
#if BUILDFLAG(IS_CHROMEOS_ASH)
// Must be kept in sync with
// ChromeBrowserMainPartsChromeos::PreEarlyInitialization.
const char TestingProfile::kTestUserProfileDir[] = "test-user";
#else
const char TestingProfile::kTestUserProfileDir[] = "Default";
#endif

// static
bool TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
    base::test::ScopedFeatureList& scoped_feature_list,
    bool enabled) {
// This feature is now only supported on Windows, Linux, and Mac.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  if (enabled)
    scoped_feature_list.InitAndEnableFeature(
        features::kEnableEphemeralGuestProfilesOnDesktop);
  else
    scoped_feature_list.InitAndDisableFeature(
        features::kEnableEphemeralGuestProfilesOnDesktop);
  return true;
#else
  return false;
#endif  // defined(OS_WIN) || defined(OS_MAC) || (defined(OS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))
}

TestingProfile::TestingProfile() : TestingProfile(base::FilePath()) {}

TestingProfile::TestingProfile(const base::FilePath& path)
    : TestingProfile(path, nullptr) {}

TestingProfile::TestingProfile(const base::FilePath& path, Delegate* delegate)
    : profile_path_(path), delegate_(delegate) {
  if (profile_path_.empty()) {
    profile_path_ = base::CreateUniqueTempDirectoryScopedToTest();
  }
  Init();
  if (delegate_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
    const std::string& supervised_user_id,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::unique_ptr<policy::UserCloudPolicyManagerChromeOS> policy_manager,
#else
    std::unique_ptr<policy::UserCloudPolicyManager> policy_manager,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    std::unique_ptr<policy::PolicyService> policy_service,
    TestingFactories testing_factories,
    const std::string& profile_name,
    base::Optional<bool> override_policy_connector_is_managed,
    base::Optional<OTRProfileID> otr_profile_id)
    : prefs_(std::move(prefs)),
      original_profile_(parent),
      guest_session_(guest_session),
      allows_browser_windows_(allows_browser_windows),
      is_new_profile_(is_new_profile),
      supervised_user_id_(supervised_user_id),
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
      policy_service_(std::move(policy_service)) {
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

  Init();
  // If caller supplied a delegate, delay the FinishInit invocation until other
  // tasks have run.
  // TODO(atwilson): See if this is still required once we convert the current
  // users of the constructor that takes a Delegate* param.
  if (delegate_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestingProfile::FinishInit, base::Unretained(this)));
  } else {
    FinishInit();
  }

  SetSupervisedUserId(supervised_user_id);
}

void TestingProfile::Init() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // If threads have been initialized, we should be on the UI thread.
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  set_is_guest_profile(guest_session_);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    if (IsOffTheRecord()) {
      set_is_system_profile(original_profile_->IsSystemProfile());
    } else {
      set_is_system_profile(profile_path_ ==
                            profile_manager->GetSystemProfilePath());
    }
  }
  UpdateBrowserProfileType();

  if (IsOffTheRecord()) {
    key_ = std::make_unique<TestingProfileKey>(
        this, original_profile_->GetPath(), original_profile_->GetProfileKey());
  } else {
    key_ = std::make_unique<TestingProfileKey>(this, profile_path_);
  }

#if defined(OS_ANDROID)
  signin::SetUpMockAccountManagerFacade();
#endif

  // Normally this would happen during browser startup, but for tests
  // we need to trigger creation of Profile-related services.
  ChromeBrowserMainExtraPartsProfiles::
      EnsureBrowserContextKeyedServiceFactoriesBuilt();

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (!IsOffTheRecord()) {
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForKey(key_.get());
    supervised_user_pref_store_ = new TestingPrefStore();
    settings_service->Init(supervised_user_pref_store_);
    settings_service->MergeDataAndStartSyncing(
        syncer::SUPERVISED_USER_SETTINGS, syncer::SyncDataList(),
        std::unique_ptr<syncer::SyncChangeProcessor>(
            new syncer::FakeSyncChangeProcessor),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock));

    supervised_user_pref_store_->SetInitializationCompleted();
  }
#endif

  if (prefs_.get())
    user_prefs::UserPrefs::Set(this, prefs_.get());
  else if (IsOffTheRecord())
    CreateIncognitoPrefService();
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  else if (!supervised_user_id_.empty())
    CreatePrefServiceForSupervisedUser();
#endif
  else
    CreateTestingPrefService();

  if (guest_session_ && IsEphemeralGuestProfileEnabled())
    GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles, true);

  key_->SetPrefs(prefs_.get());
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());

  if (!base::PathExists(profile_path_))
    base::CreateDirectory(profile_path_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Initialize |ash::AccountManager|.
  auto* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  auto* account_manager = factory->GetAccountManager(profile_path_.value());
  ash::AccountManager::DelayNetworkCallRunner immediate_callback_runner =
      base::BindRepeating(
          [](base::OnceClosure closure) -> void { std::move(closure).Run(); });
  account_manager->Initialize(profile_path_, GetURLLoaderFactory(),
                              immediate_callback_runner);
  account_manager->SetPrefService(GetPrefs());
  if (!ash::CrosSettings::IsInitialized()) {
    scoped_cros_settings_test_helper_.reset(
        new ash::ScopedCrosSettingsTestHelper);
  }
  arc::ArcServiceLauncher* launcher = arc::ArcServiceLauncher::Get();
  if (launcher)
    launcher->MaybeSetProfile(this);
#endif

  autofill::PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
      this, base::BindRepeating(&BuildPersonalDataManagerInstanceFor));

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
  bool extensions_disabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
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
      this, base::BindRepeating(&web_app::TestWebAppProvider::BuildDefault));
#endif

  // Prefs for incognito profiles are set in CreateIncognitoPrefService() by
  // simulating ProfileImpl::GetOffTheRecordPrefs().
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

void TestingProfile::UpdateBrowserProfileType() {
  if (IsGuestSession()) {
    profile_metrics::SetBrowserContextType(
        this, profile_metrics::BrowserProfileType::kGuest);
    return;
  }

  if (IsEphemeralGuestProfile()) {
    profile_metrics::SetBrowserContextType(
        this, profile_metrics::BrowserProfileType::kEphemeralGuest);
    return;
  }

  if (IsSystemProfile()) {
    profile_metrics::SetBrowserContextType(
        this, profile_metrics::BrowserProfileType::kSystem);
    return;
  }

  if (IsOffTheRecord()) {
    profile_metrics::SetBrowserContextType(
        this,
        (otr_profile_id_ == OTRProfileID::PrimaryID())
            ? profile_metrics::BrowserProfileType::kIncognito
            : profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile);
    return;
  }
  profile_metrics::SetBrowserContextType(
      this, profile_metrics::BrowserProfileType::kRegular);
}

void TestingProfile::FinishInit() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->InitProfileUserPrefs(this);

  if (delegate_) {
    delegate_->OnProfileCreated(this, true, false);
  } else {
    // It is the role of the delegate to ensure that the signout allowed is
    // properly updated after the profile is create is initialized.
    // For testing profiles that do not have a delegate, the signout allowed
    // must be initialized when the testing profile finishes its
    // initialization.
    signin_util::EnsureUserSignoutAllowedIsInitializedForProfile(this);
  }

  if (original_profile_)
    original_profile_->NotifyOffTheRecordProfileCreated(this);
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
  ForEachStoragePartition(
      this,
      base::BindRepeating([](content::StoragePartition* storage_partition) {
        if (auto* provider =
                storage_partition->GetProtoDatabaseProviderForTesting()) {
          provider->SetSharedDBDeleteObsoleteDelayForTesting(base::TimeDelta());
        }
      }));

  // Shutdown storage partitions before we post a task to delete
  // the resource context.
  ShutdownStoragePartitions();

  // Failing a post == leaks == heapcheck failure. Make that an immediate test
  // failure.
  if (resource_context_) {
    CHECK(BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
                                    resource_context_));
    resource_context_ = nullptr;
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }
}

bool TestingProfile::CreateHistoryService() {
  // Should never be created multiple times.
  DCHECK(!HistoryServiceFactory::GetForProfileWithoutCreating(this));

  // This will create and init the history service.
  history::HistoryService* history_service =
      static_cast<history::HistoryService*>(
          HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              this, HistoryServiceFactory::GetDefaultFactory()));
  if (!history_service) {
    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        this, BrowserContextKeyedServiceFactory::TestingFactory());
    return false;
  }
  return true;
}

void TestingProfile::CreateWebDataService() {
  WebDataServiceFactory::GetInstance()->SetTestingFactory(
      this, base::BindRepeating(&BuildWebDataService));
}

void TestingProfile::SetGuestSession(bool guest) {
  guest_session_ = guest;
  UpdateBrowserProfileType();
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

#if !defined(OS_ANDROID)
std::unique_ptr<content::ZoomLevelDelegate>
TestingProfile::CreateZoomLevelDelegate(const base::FilePath& partition_path) {
  return std::make_unique<ChromeZoomLevelPrefs>(
      GetPrefs(), GetPath(), partition_path,
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr());
}
#endif  // !defined(OS_ANDROID)

scoped_refptr<base::SequencedTaskRunner> TestingProfile::GetIOTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
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
  return false;
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
  if (IsOffTheRecord())
    return original_profile_->GetOffTheRecordProfile(otr_profile_id,
                                                     create_if_needed);

  // Ephemeral Guest profiles do not support Incognito.
  if (IsEphemeralGuestProfile() && otr_profile_id == OTRProfileID::PrimaryID())
    return nullptr;

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

void TestingProfile::SetSupervisedUserId(const std::string& id) {
  supervised_user_id_ = id;
  if (!id.empty())
    GetPrefs()->SetString(prefs::kSupervisedUserId, id);
  else
    GetPrefs()->ClearPref(prefs::kSupervisedUserId);
}

bool TestingProfile::IsSupervised() const {
  return !supervised_user_id_.empty();
}

bool TestingProfile::IsChild() const {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return supervised_user_id_ == supervised_users::kChildAccountSUID;
#else
  return false;
#endif
}

bool TestingProfile::AllowsBrowserWindows() const {
  return allows_browser_windows_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void TestingProfile::SetExtensionSpecialStoragePolicy(
    ExtensionSpecialStoragePolicy* extension_special_storage_policy) {
  extension_special_storage_policy_ = extension_special_storage_policy;
}
#endif

ExtensionSpecialStoragePolicy*
TestingProfile::GetExtensionSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!extension_special_storage_policy_.get())
    extension_special_storage_policy_ =
        new ExtensionSpecialStoragePolicy(nullptr);
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
  DCHECK(!supervised_user_id_.empty());

  // Construct testing_prefs_ by hand to add the supervised user pref store.
  testing_prefs_ = new sync_preferences::TestingPrefServiceSyncable(
      /*managed_prefs=*/new TestingPrefStore, supervised_user_pref_store_,
      /*extension_prefs=*/new TestingPrefStore,
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

#if !defined(OS_ANDROID)
ChromeZoomLevelPrefs* TestingProfile::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetDefaultStoragePartition(this)->GetZoomLevelDelegate());
}
#endif  // !defined(OS_ANDROID)

DownloadManagerDelegate* TestingProfile::GetDownloadManagerDelegate() {
  return nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
TestingProfile::GetURLLoaderFactory() {
  return nullptr;
}

content::ResourceContext* TestingProfile::GetResourceContext() {
  if (!resource_context_)
    resource_context_ = new content::MockResourceContext();
  return resource_context_;
}

content::BrowserPluginGuestManager* TestingProfile::GetGuestManager() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return guest_view::GuestViewManager::FromBrowserContext(this);
#else
  return nullptr;
#endif
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
policy::UserCloudPolicyManagerChromeOS*
TestingProfile::GetUserCloudPolicyManagerChromeOS() {
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
  return profile_policy_connector_.get();
}

const policy::ProfilePolicyConnector*
TestingProfile::GetProfilePolicyConnector() const {
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

PrefService* TestingProfile::GetOffTheRecordPrefs() {
  return nullptr;
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
  return nullptr;
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

bool TestingProfile::WasCreatedByVersionOrLater(const std::string& version) {
  return true;
}

bool TestingProfile::IsGuestSession() const {
  return guest_session_ && !IsEphemeralGuestProfileEnabled();
}

bool TestingProfile::IsEphemeralGuestProfile() const {
  return guest_session_ && IsEphemeralGuestProfileEnabled();
}

bool TestingProfile::IsNewProfile() const {
  return is_new_profile_;
}

Profile::ExitType TestingProfile::GetLastSessionExitType() const {
  return last_session_exited_cleanly_ ? EXIT_NORMAL : EXIT_CRASHED;
}

TestingProfile::Builder::Builder() = default;

TestingProfile::Builder::~Builder() = default;

void TestingProfile::Builder::SetPath(const base::FilePath& path) {
  path_ = path;
}

void TestingProfile::Builder::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void TestingProfile::Builder::SetExtensionSpecialStoragePolicy(
    scoped_refptr<ExtensionSpecialStoragePolicy> policy) {
  extension_policy_ = policy;
}
#endif

void TestingProfile::Builder::SetPrefService(
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs) {
  pref_service_ = std::move(prefs);
}

void TestingProfile::Builder::SetGuestSession() {
  guest_session_ = true;
}

void TestingProfile::Builder::DisallowBrowserWindows() {
  allows_browser_windows_ = false;
}

void TestingProfile::Builder::SetIsNewProfile(bool is_new_profile) {
  is_new_profile_ = is_new_profile;
}

void TestingProfile::Builder::SetSupervisedUserId(
    const std::string& supervised_user_id) {
  supervised_user_id_ = supervised_user_id;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void TestingProfile::Builder::SetUserCloudPolicyManagerChromeOS(
    std::unique_ptr<policy::UserCloudPolicyManagerChromeOS>
        user_cloud_policy_manager) {
  user_cloud_policy_manager_ = std::move(user_cloud_policy_manager);
}
#else
void TestingProfile::Builder::SetUserCloudPolicyManager(
    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager) {
  user_cloud_policy_manager_ = std::move(user_cloud_policy_manager);
}
#endif

void TestingProfile::Builder::SetPolicyService(
    std::unique_ptr<policy::PolicyService> policy_service) {
  policy_service_ = std::move(policy_service);
}

void TestingProfile::Builder::SetProfileName(const std::string& profile_name) {
  profile_name_ = profile_name;
}

void TestingProfile::Builder::OverridePolicyConnectorIsManagedForTesting(
    bool is_managed) {
  override_policy_connector_is_managed_ = is_managed;
}

void TestingProfile::Builder::AddTestingFactory(
    BrowserContextKeyedServiceFactory* service_factory,
    BrowserContextKeyedServiceFactory::TestingFactory testing_factory) {
  testing_factories_.emplace_back(service_factory, std::move(testing_factory));
}

void TestingProfile::Builder::AddTestingFactories(
    const TestingFactories& testing_factories) {
  testing_factories_.insert(testing_factories_.end(), testing_factories.begin(),
                            testing_factories.end());
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
      allows_browser_windows_, is_new_profile_, supervised_user_id_,
      std::move(user_cloud_policy_manager_), std::move(policy_service_),
      std::move(testing_factories_), profile_name_,
      override_policy_connector_is_managed_, base::Optional<OTRProfileID>());
}

TestingProfile* TestingProfile::Builder::BuildOffTheRecord(
    TestingProfile* original_profile,
    const OTRProfileID& otr_profile_id) {
  DCHECK(!build_called_);
  DCHECK(original_profile);
  build_called_ = true;

  // Ephemeral guest profiles do not support Incognito.
  if (original_profile->IsEphemeralGuestProfile() &&
      otr_profile_id == OTRProfileID::PrimaryID())
    return nullptr;

  // Note: Owned by |original_profile|.
  return new TestingProfile(
      path_, delegate_,
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_policy_,
#endif
      std::move(pref_service_), original_profile, guest_session_,
      allows_browser_windows_, is_new_profile_, supervised_user_id_,
      std::move(user_cloud_policy_manager_), std::move(policy_service_),
      std::move(testing_factories_), profile_name_,
      override_policy_connector_is_managed_,
      base::Optional<OTRProfileID>(otr_profile_id));
}

TestingProfile* TestingProfile::Builder::BuildIncognito(
    TestingProfile* original_profile) {
  return BuildOffTheRecord(original_profile, OTRProfileID::PrimaryID());
}
