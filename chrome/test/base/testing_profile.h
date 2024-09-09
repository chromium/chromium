// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

class ExtensionSpecialStoragePolicy;
class HostContentSettingsMap;
class TestingPrefStore;

namespace content {
class SSLHostStateDelegate;
class ZoomLevelDelegate;
}  // namespace content

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace chromeos {
class ScopedLacrosServiceTestHelper;
}  // namespace chromeos
#endif

namespace policy {
class PolicyService;
class ProfilePolicyConnector;
class SchemaRegistryService;
#if BUILDFLAG(IS_CHROMEOS_ASH)
class UserCloudPolicyManagerAsh;
#else
class UserCloudPolicyManager;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace policy

namespace storage {
class SpecialStoragePolicy;
}

namespace sync_preferences {
class PrefServiceSyncable;
class TestingPrefServiceSyncable;
}  // namespace sync_preferences

class TestingProfile : public Profile {
 public:
  static const char kDefaultProfileUserName[];

  // Profile directory name for the test user. This is "Default" on most
  // platforms but must be different on ChromeOS because a logged-in user cannot
  // use "Default" as profile directory.
  // Browser- and UI tests should always use this to get to the user's profile
  // directory. Unit-tests, though, should use `chrome::kInitialProfile`, which
  // is always "Default", because they are running without logged-in user.
  static const char kTestUserProfileDir[];

  // Default constructor that cannot be used with multi-profiles.
  TestingProfile();

  // Wrapper over absl::variant to help type deduction when calling
  // AddTestingFactories(). See example call in the method's comment.
  struct TestingFactory {
    TestingFactory(
        BrowserContextKeyedServiceFactory* service_factory,
        BrowserContextKeyedServiceFactory::TestingFactory testing_factory);
    TestingFactory(RefcountedBrowserContextKeyedServiceFactory* service_factory,
                   RefcountedBrowserContextKeyedServiceFactory::TestingFactory
                       testing_factory);
    TestingFactory(TestingFactory&&);
    TestingFactory& operator=(TestingFactory&&);
    ~TestingFactory();

    absl::variant<
        std::pair<BrowserContextKeyedServiceFactory*,
                  BrowserContextKeyedServiceFactory::TestingFactory>,
        std::pair<RefcountedBrowserContextKeyedServiceFactory*,
                  RefcountedBrowserContextKeyedServiceFactory::TestingFactory>>
        service_factory_and_testing_factory;
  };

  // Wrapper around std::vector that supports construction with aggregate
  // style without relying on std::initializer_list (as it does not work
  // with move-only types).
  class TestingFactories {
   public:
    TestingFactories();

    template <typename... Ts>
      requires(... && std::same_as<Ts, TestingFactory>)
    TestingFactories(Ts&&... ts) {  // NOLINT(runtime/explicit)
      (..., factories_.push_back(std::move(ts)));
    }

    TestingFactories(TestingFactories&&);
    TestingFactories& operator=(TestingFactories&&);

    ~TestingFactories();

    void push_back(TestingFactory testing_factory) {
      factories_.push_back(std::move(testing_factory));
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
      factories_.emplace_back(std::forward<Args>(args)...);
    }

    using iterator = std::vector<TestingFactory>::iterator;
    iterator begin() { return factories_.begin(); }
    iterator end() { return factories_.end(); }

   private:
    std::vector<TestingFactory> factories_;
  };

  // Helper class for building an instance of TestingProfile (allows injecting
  // mocks for various services prior to profile initialization).
  // TODO(atwilson): Remove non-default constructors and various setters in
  // favor of using the Builder API.
  class Builder {
   public:
    Builder();
    Builder(const Builder&) = delete;
    Builder(Builder&&);
    Builder& operator=(const Builder&) = delete;
    Builder& operator=(Builder&&);
    ~Builder();

    // Sets a Delegate to be called back during profile init. Caller maintains
    // ownership of the Delegate and must manage its lifetime so it continues
    // to exist until profile initialization is complete.
    Builder& SetDelegate(Delegate* delegate);

    // Sets profile creation mode to the given one.
    // Setting CreateMode::kAsynchronous causes the final initialization
    // to be performed via a task so the caller must run a MessageLoop.
    Builder& SetCreateMode(CreateMode create_mode);

    // Adds a testing factory to the TestingProfile. These testing factories
    // are applied before the ProfileKeyedServices are created.
    Builder& AddTestingFactory(
        BrowserContextKeyedServiceFactory* service_factory,
        BrowserContextKeyedServiceFactory::TestingFactory testing_factory);
    Builder& AddTestingFactory(
        RefcountedBrowserContextKeyedServiceFactory* service_factory,
        RefcountedBrowserContextKeyedServiceFactory::TestingFactory
            testing_factory);

    // Add multiple testing factories to the TestingProfile. These testing
    // factories are applied before the ProfileKeyedServices are created.
    // Example use:
    //
    // AddTestingFactories(
    //     {TestingProfile::TestingFactory{
    //          RegularServiceFactory::GetInstance(), test_factory1},
    //      TestingProfile::TestingFactory{
    //          RefcountedServiceFactory::GetInstance(), test_factory2}});
    Builder& AddTestingFactories(TestingFactories testing_factories);

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Sets the ExtensionSpecialStoragePolicy to be returned by
    // GetExtensionSpecialStoragePolicy().
    Builder& SetExtensionSpecialStoragePolicy(
        scoped_refptr<ExtensionSpecialStoragePolicy> policy);
#endif

    // Sets the path to the directory to be used to hold profile data.
    Builder& SetPath(const base::FilePath& path);

    // Sets the PrefService to be used by this profile.
    Builder& SetPrefService(
        std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs);

    // Makes the Profile being built a guest profile.
    Builder& SetGuestSession();

    // Makes Profile::AllowsBrowserWindows() return false.
    Builder& DisallowBrowserWindows();

    // Set the value to be returned by Profile::IsNewProfile().
    Builder& SetIsNewProfile(bool is_new_profile);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Set the value to be returned by Profile::IsMainProfile().
    Builder& SetIsMainProfile(bool is_main_profile);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    // Marks profile as a Family Link supervised profile.
    Builder& SetIsSupervisedProfile();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    Builder& SetUserCloudPolicyManagerAsh(
        std::unique_ptr<policy::UserCloudPolicyManagerAsh>
            user_cloud_policy_manager);
#else
    Builder& SetUserCloudPolicyManager(
        std::unique_ptr<policy::UserCloudPolicyManager>
            user_cloud_policy_manager);
    Builder& SetProfileCloudPolicyManager(
        std::unique_ptr<policy::ProfileCloudPolicyManager>
            profile_cloud_policy_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Sets the PolicyService to be used by this profile.
    Builder& SetPolicyService(
        std::unique_ptr<policy::PolicyService> policy_service);

    // Sets the UserProfileName to be used by this profile.
    Builder& SetProfileName(const std::string& profile_name);

    // Sets the SharedURLLoaderFactory to be used by this profile.
    Builder& SetSharedURLLoaderFactory(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

    Builder& OverridePolicyConnectorIsManagedForTesting(bool is_managed);

    // Creates the TestingProfile using previously-set settings.
    std::unique_ptr<TestingProfile> Build();

    // Build an OffTheRecord profile, owned by |original_profile|. Note: unless
    // you need to customize the Builder, or access TestingProfile member
    // functions, you can use original_profile->GetOffTheRecordProfile().
    TestingProfile* BuildOffTheRecord(TestingProfile* original_profile,
                                      const OTRProfileID& otr_profile_id);

    TestingProfile* BuildIncognito(TestingProfile* original_profile);

    const base::FilePath& GetPath() const { return path_; }

   private:
    // If true, Build() has already been called.
    bool build_called_ = false;

    // Various staging variables where values are held until Build() is invoked.
    std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy_;
#endif
    base::FilePath path_;
    raw_ptr<Delegate> delegate_ = nullptr;
    CreateMode create_mode_ = CreateMode::kSynchronous;
    bool guest_session_ = false;
    bool allows_browser_windows_ = true;
    bool is_new_profile_ = false;
    bool is_supervised_profile_ = false;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    bool is_main_profile_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::unique_ptr<policy::UserCloudPolicyManagerAsh>
        user_cloud_policy_manager_;
#else
    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
    std::unique_ptr<policy::ProfileCloudPolicyManager>
        profile_cloud_policy_manager_;
#endif
    std::unique_ptr<policy::PolicyService> policy_service_;
    TestingFactories testing_factories_;
    std::string profile_name_{kDefaultProfileUserName};
    std::optional<bool> override_policy_connector_is_managed_;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  };

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile. This constructor is meant to be used by
  // TestingProfileManager::CreateTestingProfile. If you need to create multi-
  // profile profiles, use that factory method instead of this directly.
  // Exception: if you need to create multi-profile profiles for testing the
  // ProfileManager, then use the constructor below instead.
  explicit TestingProfile(const base::FilePath& path);

  // Multi-profile aware constructor that takes the path to a directory managed
  // for this profile and a delegate. This constructor is meant to be used
  // for unittesting the ProfileManager.
  TestingProfile(const base::FilePath& path,
                 Delegate* delegate,
                 CreateMode create_mode);

  // Full constructor allowing the setting of all possible instance data.
  // Callers should use Builder::Build() instead of invoking this constructor.
  TestingProfile(
      const base::FilePath& path,
      Delegate* delegate,
      CreateMode create_mode,
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
      absl::variant<std::unique_ptr<policy::UserCloudPolicyManager>,
                    std::unique_ptr<policy::ProfileCloudPolicyManager>>
          policy_manager,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      std::unique_ptr<policy::PolicyService> policy_service,
      TestingFactories testing_factories,
      const std::string& profile_name,
      std::optional<bool> override_policy_connector_is_managed,
      const OTRProfileID* otr_profile_id,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~TestingProfile() override;

  // Note: Calling the Builder methods instead is preferred.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Allow setting the return value of IsMainProfile().
  void SetIsMainProfile(bool is_main_profile);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Allow setting a profile as Guest after-the-fact to simplify some tests.
  void SetGuestSession(bool guest);

  // Allow setting the return value of IsNewProfile.
  void SetIsNewProfile(bool is_new_profile);

  // Called on the parent of an OffTheRecord |otr_profile|. Usually called from
  // the constructor of an OffTheRecord TestingProfile, but can also be used by
  // tests to provide an OffTheRecordProfileImpl instance.
  // |otr_profile| cannot be empty.
  void SetOffTheRecordProfile(std::unique_ptr<Profile> otr_profile);

  // Marks profile Family Link supervision status.
  void SetIsSupervisedProfile(bool is_supervised_profile = true);

  sync_preferences::TestingPrefServiceSyncable* GetTestingPrefService();

  // content::BrowserContext
  base::FilePath GetPath() override;
  base::FilePath GetPath() const override;
  base::Time GetCreationTime() const override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsMainProfile() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override;

  TestingProfile* AsTestingProfile() override;

  // Profile
  std::string GetProfileUserName() const override;

  Profile* GetOffTheRecordProfile(const OTRProfileID& otr_profile_id,
                                  bool create_if_needed) override;
  std::vector<Profile*> GetAllOffTheRecordProfiles() override;
  void DestroyOffTheRecordProfile(Profile* otr_profile) override;
  bool HasOffTheRecordProfile(const OTRProfileID& otr_profile_id) override;
  bool HasAnyOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  const Profile* GetOriginalProfile() const override;
  bool IsChild() const override;
  bool AllowsBrowserWindows() const override;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  void SetExtensionSpecialStoragePolicy(
      scoped_refptr<ExtensionSpecialStoragePolicy>
          extension_special_storage_policy);
#endif
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;

  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

  bool IsSameOrParent(Profile* profile) override;
  base::Time GetStartTime() const override;
  ProfileKey* GetProfileKey() const override;
  policy::SchemaRegistryService* GetPolicySchemaRegistryService() override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUserCloudPolicyManagerAsh(
      std::unique_ptr<policy::UserCloudPolicyManagerAsh>
          user_cloud_policy_manager);
  policy::UserCloudPolicyManagerAsh* GetUserCloudPolicyManagerAsh() override;
#else
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
  policy::ProfileCloudPolicyManager* GetProfileCloudPolicyManager() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  policy::CloudPolicyManager* GetCloudPolicyManager() override;
  policy::ProfilePolicyConnector* GetProfilePolicyConnector() override;
  const policy::ProfilePolicyConnector* GetProfilePolicyConnector()
      const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  bool IsGuestSession() const override;
  bool IsNewProfile() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ChangeAppLocale(const std::string&, AppLocaleChangedVia) override;
  void OnLogin() override {}
  void InitChromeOSPreferences() override {}
  ash::ScopedCrosSettingsTestHelper* ScopedCrosSettingsTestHelper();

  std::optional<std::string> requested_locale() { return requested_locale_; }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Schedules a task on the history backend and runs a nested loop until the
  // task is processed.  This has the effect of blocking the caller until the
  // history service processes all pending requests.
  void BlockUntilHistoryProcessesPendingRequests();

  GURL GetHomePage() override;

  void SetCreationTimeForTesting(base::Time creation_time) override;

  void RecordPrimaryMainFrameNavigation() override {}

  void set_profile_name(const std::string& profile_name) {
    profile_name_ = profile_name;
  }

  using ProfileDestructionCallback = base::OnceCallback<void()>;
  void SetProfileDestructionObserver(ProfileDestructionCallback callback) {
    profile_destruction_callback_ = std::move(callback);
  }

  void SetPermissionControllerDelegate(
      std::unique_ptr<content::PermissionControllerDelegate> delegate) {
    permission_controller_delegate_ = std::move(delegate);
  }

 private:
  // Called when profile is deleted.
  ProfileDestructionCallback profile_destruction_callback_;

 protected:
  base::Time start_time_{base::Time::Now()};

  // The key to index KeyedService instances created by
  // SimpleKeyedServiceFactory.
  std::unique_ptr<ProfileKey> key_;

  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  // ref only for right type, lifecycle is managed by prefs_
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> testing_prefs_ =
      nullptr;

  // Profile implementation.
  bool IsSignedIn() override;

 private:
  // Common initialization between the two constructors.
  void Init(bool is_supervised_profile, CreateMode create_mode);

  // Finishes initialization when a profile is created asynchronously.
  void FinishInit(CreateMode create_mode);

  void InitializeProfileType();

  // Creates a TestingPrefService and associates it with the TestingProfile.
  void CreateTestingPrefService();

  // Creates a pref service that uses SupervisedUserPrefStore and associates
  // it with the TestingProfile.
  void CreatePrefServiceForSupervisedUser();

  // Initializes |prefs_| for an incognito profile, derived from
  // |original_profile_|.
  void CreateIncognitoPrefService();

  // Creates a ProfilePolicyConnector.
  void CreateProfilePolicyConnector();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::map<OTRProfileID, std::unique_ptr<Profile>> otr_profiles_;
  raw_ptr<TestingProfile> original_profile_ = nullptr;

  bool guest_session_ = false;

  bool allows_browser_windows_ = true;

  bool is_new_profile_ = false;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool is_main_profile_ = false;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  std::unique_ptr<content::PermissionControllerDelegate>
      permission_controller_delegate_;

  base::FilePath last_selected_directory_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
#endif

  // The path to this profile. This will be valid in either of the two above
  // cases.
  base::FilePath profile_path_;

  base::FilePath extensions_path_;

  // We keep a weak pointer to the dependency manager we want to notify on our
  // death. Defaults to the Singleton implementation but overridable for
  // testing.
  raw_ptr<SimpleDependencyManager> simple_dependency_manager_{
      SimpleDependencyManager::GetInstance()};
  raw_ptr<BrowserContextDependencyManager> browser_context_dependency_manager_{
      BrowserContextDependencyManager::GetInstance()};

  std::unique_ptr<policy::SchemaRegistryService> schema_registry_service_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<policy::UserCloudPolicyManagerAsh> user_cloud_policy_manager_;
#else
  std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
  std::unique_ptr<policy::ProfileCloudPolicyManager>
      profile_cloud_policy_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Weak pointer to a delegate for indicating that a profile was created.
  raw_ptr<Delegate> delegate_ = nullptr;

  std::string profile_name_{kDefaultProfileUserName};

  std::optional<bool> override_policy_connector_is_managed_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::ScopedCrosSettingsTestHelper>
      scoped_cros_settings_test_helper_;

  std::optional<std::string> requested_locale_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<chromeos::ScopedLacrosServiceTestHelper>
      lacros_service_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::unique_ptr<policy::PolicyService> policy_service_;

  scoped_refptr<TestingPrefStore> supervised_user_pref_store_ = nullptr;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_H_
