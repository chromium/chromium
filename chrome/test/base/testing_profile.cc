// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile.h"

#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/in_memory_url_index_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/schema_registry_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/storage_partition_descriptor.h"
#include "chrome/browser/search_engines/template_url_fetcher_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/bookmark_sync_service_factory.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/favicon/core/favicon_service.h"
#include "components/history/content/browser/content_visit_delegate.h"
#include "components/history/content/browser/history_database_helper.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/history_index_restore_observer.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema.h"
#include "components/prefs/testing_pref_store.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "components/webdata_services/web_data_service_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_system.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"
#else  // !defined(OS_ANDROID)
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/zoom_level_delegate.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_pref_store.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#endif

using base::Time;
using bookmarks::BookmarkModel;
using content::BrowserThread;
using content::DownloadManagerDelegate;
using testing::NiceMock;
using testing::Return;

namespace {

// Default profile name
const char kTestingProfile[] = "testing_profile";

std::unique_ptr<KeyedService> BuildHistoryService(
    content::BrowserContext* context) {
  return std::make_unique<history::HistoryService>(
      std::make_unique<ChromeHistoryClient>(
          BookmarkModelFactory::GetForBrowserContext(context)),
      std::make_unique<history::ContentVisitDelegate>(context));
}

std::unique_ptr<KeyedService> BuildInMemoryURLIndex(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<InMemoryURLIndex> in_memory_url_index(
      new InMemoryURLIndex(BookmarkModelFactory::GetForBrowserContext(profile),
                           HistoryServiceFactory::GetForProfile(
                               profile, ServiceAccessType::IMPLICIT_ACCESS),
                           TemplateURLServiceFactory::GetForProfile(profile),
                           profile->GetPath(), SchemeSet()));
  in_memory_url_index->Init();
  return std::move(in_memory_url_index);
}

std::unique_ptr<KeyedService> BuildBookmarkModel(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<BookmarkModel> bookmark_model(
      new BookmarkModel(std::make_unique<ChromeBookmarkClient>(
          profile, ManagedBookmarkServiceFactory::GetForProfile(profile),
          BookmarkSyncServiceFactory::GetForProfile(profile))));
  bookmark_model->Load(profile->GetPrefs(), profile->GetPath(),
                       profile->GetIOTaskRunner(),
                       base::CreateSingleThreadTaskRunnerWithTraits(
                           {content::BrowserThread::UI}));
  return std::move(bookmark_model);
}

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
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
      sync_start_util::GetFlareForSyncableService(context_path),
      base::BindRepeating(&TestProfileErrorCallback));
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
std::unique_ptr<KeyedService> BuildOfflinePageModel(
    content::BrowserContext* context) {
  return std::make_unique<offline_pages::StubOfflinePageModel>();
}
#endif

}  // namespace

// static
#if defined(OS_CHROMEOS)
// Must be kept in sync with
// ChromeBrowserMainPartsChromeos::PreEarlyInitialization.
const char TestingProfile::kTestUserProfileDir[] = "test-user";
#else
const char TestingProfile::kTestUserProfileDir[] = "Default";
#endif

TestingProfile::TestingProfile()
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(NULL),
      profile_name_(kTestingProfile) {
  CreateTempProfileDir();
  profile_path_ = temp_dir_.GetPath();

  Init();
  FinishInit();
}

TestingProfile::TestingProfile(const base::FilePath& path)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(NULL),
      profile_name_(kTestingProfile) {
  Init();
  FinishInit();
}

TestingProfile::TestingProfile(const base::FilePath& path, Delegate* delegate)
    : start_time_(Time::Now()),
      testing_prefs_(NULL),
      force_incognito_(false),
      original_profile_(NULL),
      guest_session_(false),
      last_session_exited_cleanly_(true),
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(delegate),
      profile_name_(kTestingProfile) {
  Init();
  if (delegate_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&TestingProfile::FinishInit, base::Unretained(this)));
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
    base::Optional<bool> is_new_profile,
    const std::string& supervised_user_id,
    std::unique_ptr<policy::PolicyService> policy_service,
    TestingFactories testing_factories,
    const std::string& profile_name)
    : start_time_(Time::Now()),
      prefs_(std::move(prefs)),
      testing_prefs_(NULL),
      force_incognito_(false),
      original_profile_(parent),
      guest_session_(guest_session),
      is_new_profile_(std::move(is_new_profile)),
      supervised_user_id_(supervised_user_id),
      last_session_exited_cleanly_(true),
#if BUILDFLAG(ENABLE_EXTENSIONS)
      extension_special_storage_policy_(extension_policy),
#endif
      profile_path_(path),
      browser_context_dependency_manager_(
          BrowserContextDependencyManager::GetInstance()),
      resource_context_(NULL),
      delegate_(delegate),
      profile_name_(profile_name),
      policy_service_(policy_service.release()) {
  if (parent)
    parent->SetOffTheRecordProfile(std::unique_ptr<Profile>(this));

  // If no profile path was supplied, create one.
  if (profile_path_.empty()) {
    CreateTempProfileDir();
    profile_path_ = temp_dir_.GetPath();
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
        base::Bind(&TestingProfile::FinishInit, base::Unretained(this)));
  } else {
    FinishInit();
  }

  SetSupervisedUserId(supervised_user_id);
}

void TestingProfile::CreateTempProfileDir() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create unique temporary directory.";

    // Fallback logic in case we fail to create unique temporary directory.
    base::FilePath system_tmp_dir;
    bool success = base::PathService::Get(base::DIR_TEMP, &system_tmp_dir);

    // We're severly screwed if we can't get the system temporary
    // directory. Die now to avoid writing to the filesystem root
    // or other bad places.
    CHECK(success);

    base::FilePath fallback_dir(
        system_tmp_dir.AppendASCII("TestingProfilePath"));
    base::DeleteFile(fallback_dir, true);
    base::CreateDirectory(fallback_dir);
    if (!temp_dir_.Set(fallback_dir)) {
      // That shouldn't happen, but if it does, try to recover.
      LOG(ERROR) << "Failed to use a fallback temporary directory.";

      // We're screwed if this fails, see CHECK above.
      CHECK(temp_dir_.Set(system_tmp_dir));
    }
  }
}

void TestingProfile::Init() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // If threads have been initialized, we should be on the UI thread.
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  set_is_guest_profile(guest_session_);

  BrowserContext::Initialize(this, profile_path_);

#if defined(OS_ANDROID)
  // Make sure token service knows its running in tests.
  OAuth2TokenServiceDelegateAndroid::set_is_testing_profile();
#endif

  // Normally this would happen during browser startup, but for tests
  // we need to trigger creation of Profile-related services.
  ChromeBrowserMainExtraPartsProfiles::
      EnsureBrowserContextKeyedServiceFactoriesBuilt();

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (!IsOffTheRecord()) {
    SupervisedUserSettingsService* settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(this);
    TestingPrefStore* store = new TestingPrefStore();
    settings_service->Init(store);
    settings_service->MergeDataAndStartSyncing(
        syncer::SUPERVISED_USER_SETTINGS, syncer::SyncDataList(),
        std::unique_ptr<syncer::SyncChangeProcessor>(
            new syncer::FakeSyncChangeProcessor),
        std::unique_ptr<syncer::SyncErrorFactory>(
            new syncer::SyncErrorFactoryMock));

    store->SetInitializationCompleted();
  }
#endif

  if (prefs_.get())
    user_prefs::UserPrefs::Set(this, prefs_.get());
  else if (IsOffTheRecord())
    CreateIncognitoPrefService();
  else if (!supervised_user_id_.empty())
    CreatePrefServiceForSupervisedUser();
  else
    CreateTestingPrefService();

  if (!base::PathExists(profile_path_))
    base::CreateDirectory(profile_path_);

#if defined(OS_CHROMEOS)
  if (!chromeos::CrosSettings::IsInitialized()) {
    scoped_cros_settings_test_helper_.reset(
        new chromeos::ScopedCrosSettingsTestHelper);
  }
  arc::ArcServiceLauncher* launcher = arc::ArcServiceLauncher::Get();
  if (launcher)
    launcher->MaybeSetProfile(this);
#endif

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
          std::vector<extensions::ExtensionPrefsObserver*>()));
  extensions::ExtensionPrefsFactory::GetInstance()->SetInstanceForTesting(
      this, std::move(extension_prefs));

  extensions::ExtensionSystemFactory::GetInstance()->SetTestingFactory(
      this, base::BindRepeating(&extensions::TestExtensionSystem::Build));

  extensions::EventRouterFactory::GetInstance()->SetTestingFactory(
      this, BrowserContextKeyedServiceFactory::TestingFactory());
#endif

  // Prefs for incognito profiles are set in CreateIncognitoPrefService() by
  // simulating ProfileImpl::GetOffTheRecordPrefs().
  if (!IsOffTheRecord()) {
    DCHECK(!original_profile_);
    user_prefs::PrefRegistrySyncable* pref_registry =
        static_cast<user_prefs::PrefRegistrySyncable*>(
            prefs_->DeprecatedGetPrefRegistry());
    browser_context_dependency_manager_->
        RegisterProfilePrefsForServices(this, pref_registry);
  }

  browser_context_dependency_manager_->CreateBrowserContextServicesForTest(
      this);
}

void TestingProfile::FinishInit() {
  DCHECK(content::NotificationService::current());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::Source<Profile>(static_cast<Profile*>(this)),
      content::NotificationService::NoDetails());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager)
    profile_manager->InitProfileUserPrefs(this);

  if (delegate_)
    delegate_->OnProfileCreated(this, true, false);
}

TestingProfile::~TestingProfile() {
  // Revert to non-incognito mode before shutdown.
  force_incognito_ = false;

  // If this profile owns an incognito profile, tear it down first.
  incognito_profile_.reset();

  // Any objects holding live URLFetchers should be deleted before teardown.
  TemplateURLFetcherFactory::ShutdownForProfile(this);

  MaybeSendDestroyedNotification();

  browser_context_dependency_manager_->DestroyBrowserContextServices(this);

  if (host_content_settings_map_.get())
    host_content_settings_map_->ShutdownOnUIThread();

  // Shutdown storage partitions before we post a task to delete
  // the resource context.
  ShutdownStoragePartitions();

  // Failing a post == leaks == heapcheck failure. Make that an immediate test
  // failure.
  if (resource_context_) {
    CHECK(BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
                                    resource_context_));
    resource_context_ = NULL;
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  base::ScopedAllowBlockingForTesting allow_blocking;
  ignore_result(temp_dir_.Delete());
}

void TestingProfile::CreateFaviconService() {
  // It is up to the caller to create the history service if one is needed.
  FaviconServiceFactory::GetInstance()->SetTestingFactory(
      this, FaviconServiceFactory::GetDefaultFactory());
}

bool TestingProfile::CreateHistoryService(bool delete_file, bool no_db) {
  // Should never be created multiple times.
  DCHECK(!HistoryServiceFactory::GetForProfileWithoutCreating(this));

  if (delete_file) {
    base::FilePath path = GetPath();
    path = path.Append(history::kHistoryFilename);
    if (!base::DeleteFile(path, false) || base::PathExists(path))
      return false;
  }
  // This will create and init the history service.
  history::HistoryService* history_service =
      static_cast<history::HistoryService*>(
          HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
              this, base::BindRepeating(&BuildHistoryService)));
  if (!history_service->Init(
          no_db, history::HistoryDatabaseParamsForPath(GetPath()))) {
    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        this, BrowserContextKeyedServiceFactory::TestingFactory());
    return false;
  }
  // Some tests expect that CreateHistoryService() will also make the
  // InMemoryURLIndex available.
  InMemoryURLIndexFactory::GetInstance()->SetTestingFactory(
      this, base::BindRepeating(&BuildInMemoryURLIndex));
  // Disable WebHistoryService by default, since it makes network requests.
  WebHistoryServiceFactory::GetInstance()->SetTestingFactory(
      this, BrowserContextKeyedServiceFactory::TestingFactory());
  return true;
}

void TestingProfile::CreateBookmarkModel(bool delete_file) {
  if (delete_file) {
    base::FilePath path = GetPath().Append(bookmarks::kBookmarksFileName);
    base::DeleteFile(path, false);
  }
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageModelFactory::GetInstance()->SetTestingFactory(
      this, base::BindRepeating(&BuildOfflinePageModel));
#endif
  ManagedBookmarkServiceFactory::GetInstance()->SetTestingFactory(
      this, ManagedBookmarkServiceFactory::GetDefaultFactory());
  // This creates the BookmarkModel.
  ignore_result(BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
      this, base::BindRepeating(&BuildBookmarkModel)));
}

void TestingProfile::CreateWebDataService() {
  WebDataServiceFactory::GetInstance()->SetTestingFactory(
      this, base::BindRepeating(&BuildWebDataService));
}

void TestingProfile::BlockUntilHistoryIndexIsRefreshed() {
  // Only get the history service if it actually exists since the caller of the
  // test should explicitly call CreateHistoryService to build it.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(this);
  DCHECK(history_service);
  InMemoryURLIndex* index = InMemoryURLIndexFactory::GetForProfile(this);
  if (!index || index->restored())
    return;
  base::RunLoop run_loop;
  HistoryIndexRestoreObserver observer(
      content::GetDeferredQuitTaskForRunLoop(&run_loop));
  index->set_restore_cache_observer(&observer);
  run_loop.Run();
  index->set_restore_cache_observer(NULL);
  DCHECK(index->restored());
}

void TestingProfile::SetGuestSession(bool guest) {
  guest_session_ = guest;
}

void TestingProfile::SetIsNewProfile(bool is_new_profile) {
  is_new_profile_ = is_new_profile;
}

base::FilePath TestingProfile::GetPath() const {
  return profile_path_;
}

base::FilePath TestingProfile::GetCachePath() const {
  base::FilePath cache_path;
  chrome::GetUserCacheDirectory(profile_path_, &cache_path);
  return cache_path;
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

Profile::ProfileType TestingProfile::GetProfileType() const {
  if (guest_session_)
    return GUEST_PROFILE;
  if (force_incognito_ || original_profile_)
    return INCOGNITO_PROFILE;
  return REGULAR_PROFILE;
}

bool TestingProfile::IsOffTheRecord() const {
  return force_incognito_ || original_profile_;
}

void TestingProfile::SetOffTheRecordProfile(std::unique_ptr<Profile> profile) {
  DCHECK(!IsOffTheRecord());
  if (profile)
    DCHECK_EQ(this, profile->GetOriginalProfile());
  incognito_profile_ = std::move(profile);
}

Profile* TestingProfile::GetOffTheRecordProfile() {
  if (IsOffTheRecord())
    return this;
  if (!incognito_profile_)
    TestingProfile::Builder().BuildIncognito(this);
  return incognito_profile_.get();
}

bool TestingProfile::HasOffTheRecordProfile() {
  return incognito_profile_.get() != NULL;
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

bool TestingProfile::IsLegacySupervised() const {
  return IsSupervised() && !IsChild();
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
    extension_special_storage_policy_ = new ExtensionSpecialStoragePolicy(NULL);
  return extension_special_storage_policy_.get();
#else
  return NULL;
#endif
}

void TestingProfile::CreateTestingPrefService() {
  DCHECK(!prefs_.get());
  testing_prefs_ = new sync_preferences::TestingPrefServiceSyncable();
  prefs_.reset(testing_prefs_);
  user_prefs::UserPrefs::Set(this, prefs_.get());
  RegisterUserProfilePrefs(testing_prefs_->registry());
}

void TestingProfile::CreatePrefServiceForSupervisedUser() {
  DCHECK(!prefs_.get());
  DCHECK(!supervised_user_id_.empty());
  sync_preferences::PrefServiceMockFactory factory;
  SupervisedUserSettingsService* supervised_user_settings =
      SupervisedUserSettingsServiceFactory::GetForProfile(this);
  scoped_refptr<PrefStore> supervised_user_prefs =
      base::MakeRefCounted<SupervisedUserPrefStore>(supervised_user_settings);

  factory.set_supervised_user_prefs(supervised_user_prefs);

  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable);

  prefs_ = factory.CreateSyncable(registry.get());
  RegisterUserProfilePrefs(registry.get());
  user_prefs::UserPrefs::Set(this, prefs_.get());
}

void TestingProfile::CreateIncognitoPrefService() {
  DCHECK(original_profile_);
  DCHECK(!testing_prefs_);
  // Simplified version of ProfileImpl::GetOffTheRecordPrefs(). Note this
  // leaves testing_prefs_ unset.
  prefs_ = CreateIncognitoPrefServiceSyncable(original_profile_->prefs_.get(),
                                              nullptr, nullptr);
  user_prefs::UserPrefs::Set(this, prefs_.get());
}

void TestingProfile::CreateProfilePolicyConnector() {
  schema_registry_service_ =
      policy::SchemaRegistryServiceFactory::CreateForContext(
          this, policy::Schema(), NULL);
  CHECK_EQ(schema_registry_service_.get(),
           policy::SchemaRegistryServiceFactory::GetForContext(this));

  if (!policy_service_) {
    std::vector<policy::ConfigurationPolicyProvider*> providers;
    std::unique_ptr<policy::PolicyServiceImpl> policy_service =
        std::make_unique<policy::PolicyServiceImpl>(std::move(providers));
    policy_service_ = std::move(policy_service);
  }
  profile_policy_connector_.reset(new policy::ProfilePolicyConnector());
  profile_policy_connector_->InitForTesting(std::move(policy_service_));
  policy::ProfilePolicyConnectorFactory::GetInstance()->SetServiceForTesting(
      this, profile_policy_connector_.get());
  CHECK_EQ(profile_policy_connector_.get(),
           policy::ProfilePolicyConnectorFactory::GetForBrowserContext(this));
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
  return NULL;
}

net::URLRequestContextGetter* TestingProfile::GetRequestContext() {
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

base::OnceCallback<net::CookieStore*()>
TestingProfile::GetExtensionsCookieStoreGetter() {
  return base::BindOnce(
      [](std::unique_ptr<net::CookieStore,
                         content::BrowserThread::DeleteOnIOThread>*
             cookie_store) {
        if (!*cookie_store) {
          content::CookieStoreConfig cookie_config;
          cookie_config.cookieable_schemes.push_back(
              extensions::kExtensionScheme);
          cookie_store->reset(
              content::CreateCookieStore(cookie_config, nullptr /* netlog */)
                  .release());
        }
        return cookie_store->get();
      },
      &extensions_cookie_store_);
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
  return NULL;
#endif
}

content::PushMessagingService* TestingProfile::GetPushMessagingService() {
  return NULL;
}

bool TestingProfile::IsSameProfile(Profile *profile) {
  if (this == profile)
    return true;
  Profile* otr_profile = incognito_profile_.get();
  return otr_profile && profile == otr_profile;
}

base::Time TestingProfile::GetStartTime() const {
  return start_time_;
}

base::FilePath TestingProfile::last_selected_directory() {
  return last_selected_directory_;
}

void TestingProfile::set_last_selected_directory(const base::FilePath& path) {
  last_selected_directory_ = path;
}

#if defined(OS_CHROMEOS)
void TestingProfile::ChangeAppLocale(const std::string& locale,
                                     AppLocaleChangedVia via) {
  requested_locale_ = locale;
}

chromeos::ScopedCrosSettingsTestHelper*
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

PrefService* TestingProfile::GetOffTheRecordPrefs() {
  return NULL;
}

storage::SpecialStoragePolicy* TestingProfile::GetSpecialStoragePolicy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return GetExtensionSpecialStoragePolicy();
#else
  return NULL;
#endif
}

content::SSLHostStateDelegate* TestingProfile::GetSSLHostStateDelegate() {
  return NULL;
}

content::PermissionControllerDelegate*
TestingProfile::GetPermissionControllerDelegate() {
  return NULL;
}

content::BackgroundFetchDelegate* TestingProfile::GetBackgroundFetchDelegate() {
  return nullptr;
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

net::URLRequestContextGetter* TestingProfile::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  return new net::TestURLRequestContextGetter(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
}

net::URLRequestContextGetter*
TestingProfile::CreateRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  // We don't test storage partitions here yet, so returning the same dummy
  // context is sufficient for now.
  return GetRequestContext();
}

net::URLRequestContextGetter* TestingProfile::CreateMediaRequestContext() {
  return NULL;
}

net::URLRequestContextGetter*
TestingProfile::CreateMediaRequestContextForStoragePartition(
    const base::FilePath& partition_path,
    bool in_memory) {
  return NULL;
}

bool TestingProfile::WasCreatedByVersionOrLater(const std::string& version) {
  return true;
}

bool TestingProfile::IsGuestSession() const {
  return guest_session_;
}

bool TestingProfile::IsNewProfile() {
  if (is_new_profile_.has_value())
    return is_new_profile_.value();
  return Profile::IsNewProfile();
}

Profile::ExitType TestingProfile::GetLastSessionExitType() {
  return last_session_exited_cleanly_ ? EXIT_NORMAL : EXIT_CRASHED;
}

network::mojom::NetworkContextPtr TestingProfile::CreateNetworkContext(
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    network::mojom::NetworkContextPtr network_context;
    network_context_request_ = mojo::MakeRequest(&network_context);
    return network_context;
  }
  return nullptr;
}

TestingProfile::Builder::Builder()
    : build_called_(false),
      delegate_(NULL),
      guest_session_(false),
      profile_name_(kTestingProfile) {}

TestingProfile::Builder::~Builder() {
}

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

void TestingProfile::Builder::OverrideIsNewProfile(bool is_new_profile) {
  is_new_profile_ = is_new_profile;
}

void TestingProfile::Builder::SetSupervisedUserId(
    const std::string& supervised_user_id) {
  supervised_user_id_ = supervised_user_id;
}

void TestingProfile::Builder::SetPolicyService(
    std::unique_ptr<policy::PolicyService> policy_service) {
  policy_service_ = std::move(policy_service);
}

void TestingProfile::Builder::SetProfileName(const std::string& profile_name) {
  profile_name_ = profile_name;
}

void TestingProfile::Builder::AddTestingFactory(
    BrowserContextKeyedServiceFactory* service_factory,
    BrowserContextKeyedServiceFactory::TestingFactory testing_factory) {
  testing_factories_.emplace_back(service_factory, std::move(testing_factory));
}

std::unique_ptr<TestingProfile> TestingProfile::Builder::Build() {
  DCHECK(!build_called_);
  build_called_ = true;

  return std::unique_ptr<TestingProfile>(
      new TestingProfile(path_, delegate_,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                         extension_policy_,
#endif
                         std::move(pref_service_), NULL, guest_session_,
                         std::move(is_new_profile_), supervised_user_id_,
                         std::move(policy_service_),
                         std::move(testing_factories_), profile_name_));
}

TestingProfile* TestingProfile::Builder::BuildIncognito(
    TestingProfile* original_profile) {
  DCHECK(!build_called_);
  DCHECK(original_profile);
  build_called_ = true;

  // Note: Owned by |original_profile|.
  return new TestingProfile(path_, delegate_,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                            extension_policy_,
#endif
                            std::move(pref_service_), original_profile,
                            guest_session_, std::move(is_new_profile_),
                            supervised_user_id_, std::move(policy_service_),
                            std::move(testing_factories_), profile_name_);
}
