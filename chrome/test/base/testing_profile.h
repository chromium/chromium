// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "components/domain_reliability/clear_mode.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/cookies/cookie_store.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#endif

class ExtensionSpecialStoragePolicy;
class HostContentSettingsMap;
class TestingPrefStore;

namespace content {
class MockResourceContext;
class SSLHostStateDelegate;
#if !defined(OS_ANDROID)
class ZoomLevelDelegate;
#endif  // !defined(OS_ANDROID)
}  // namespace content

namespace net {
class CookieStore;
}

namespace policy {
class PolicyService;
class ProfilePolicyConnector;
class SchemaRegistryService;
#if BUILDFLAG(IS_CHROMEOS_ASH)
class UserCloudPolicyManagerChromeOS;
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
  // directory. Unit-tests, though, should use |kInitialProfile|, which is
  // always "Default", because they are runnining without logged-in user.
  static const char kTestUserProfileDir[];

  // Sets the feature list to enable/disable ephemeral Guest profiles.
  // Returns true if ephemeral Guest profiles are supported on the platform and
  // feature list is initialized.
  // TODO(https://crbug.com/1125474): Expand to cover ChromeOS when ChromeOS
  // code supports ephemeral Guest profiles.
  static bool SetScopedFeatureListForEphemeralGuestProfiles(
      base::test::ScopedFeatureList& scoped_feature_list,
      bool enabled);

  // Default constructor that cannot be used with multi-profiles.
  TestingProfile();

  using TestingFactories =
      std::vector<std::pair<BrowserContextKeyedServiceFactory*,
                            BrowserContextKeyedServiceFactory::TestingFactory>>;

  // Helper class for building an instance of TestingProfile (allows injecting
  // mocks for various services prior to profile initialization).
  // TODO(atwilson): Remove non-default constructors and various setters in
  // favor of using the Builder API.
  class Builder {
   public:
    Builder();
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    ~Builder();

    // Sets a Delegate to be called back during profile init. This causes the
    // final initialization to be performed via a task so the caller must run
    // a MessageLoop. Caller maintains ownership of the Delegate
    // and must manage its lifetime so it continues to exist until profile
    // initialization is complete.
    void SetDelegate(Delegate* delegate);

    // Adds a testing factory to the TestingProfile. These testing factories
    // are applied before the ProfileKeyedServices are created.
    void AddTestingFactory(
        BrowserContextKeyedServiceFactory* service_factory,
        BrowserContextKeyedServiceFactory::TestingFactory testing_factory);

    // Add multiple testing factories to the TestingProfile. These testing
    // factories are applied before the ProfileKeyedServices are created.
    void AddTestingFactories(const TestingFactories& testing_factories);

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Sets the ExtensionSpecialStoragePolicy to be returned by
    // GetExtensionSpecialStoragePolicy().
    void SetExtensionSpecialStoragePolicy(
        scoped_refptr<ExtensionSpecialStoragePolicy> policy);
#endif

    // Sets the path to the directory to be used to hold profile data.
    void SetPath(const base::FilePath& path);

    // Sets the PrefService to be used by this profile.
    void SetPrefService(
        std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs);

    // Makes the Profile being built a guest profile.
    void SetGuestSession();

    // Makes Profile::AllowsBrowserWindows() return false.
    void DisallowBrowserWindows();

    // Set the value to be returned by Profile::IsNewProfile().
    void SetIsNewProfile(bool is_new_profile);

    // Sets the supervised user ID (which is empty by default). If it is set to
    // a non-empty string, the profile is supervised.
    void SetSupervisedUserId(const std::string& supervised_user_id);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    void SetUserCloudPolicyManagerChromeOS(
        std::unique_ptr<policy::UserCloudPolicyManagerChromeOS>
            user_cloud_policy_manager);
#else
    void SetUserCloudPolicyManager(
        std::unique_ptr<policy::UserCloudPolicyManager>
            user_cloud_policy_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Sets the PolicyService to be used by this profile.
    void SetPolicyService(
        std::unique_ptr<policy::PolicyService> policy_service);

    // Sets the UserProfileName to be used by this profile.
    void SetProfileName(const std::string& profile_name);

    void OverridePolicyConnectorIsManagedForTesting(bool is_managed);

    // Creates the TestingProfile using previously-set settings.
    std::unique_ptr<TestingProfile> Build();

    // Build an OffTheRecord profile, owned by |original_profile|. Note: unless
    // you need to customize the Builder, or access TestingProfile member
    // functions, you can use original_profile->GetOffTheRecordProfile().
    TestingProfile* BuildOffTheRecord(TestingProfile* original_profile,
                                      const OTRProfileID& otr_profile_id);

    TestingProfile* BuildIncognito(TestingProfile* original_profile);

   private:
    // If true, Build() has already been called.
    bool build_called_ = false;

    // Various staging variables where values are held until Build() is invoked.
    std::unique_ptr<sync_preferences::PrefServiceSyncable> pref_service_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<ExtensionSpecialStoragePolicy> extension_policy_;
#endif
    base::FilePath path_;
    Delegate* delegate_ = nullptr;
    bool guest_session_ = false;
    bool allows_browser_windows_ = true;
    bool is_new_profile_ = false;
    std::string supervised_user_id_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::unique_ptr<policy::UserCloudPolicyManagerChromeOS>
        user_cloud_policy_manager_;
#else
    std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
#endif
    std::unique_ptr<policy::PolicyService> policy_service_;
    TestingFactories testing_factories_;
    std::string profile_name_{kDefaultProfileUserName};
    base::Optional<bool> override_policy_connector_is_managed_;
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
  TestingProfile(const base::FilePath& path, Delegate* delegate);

  // Full constructor allowing the setting of all possible instance data.
  // Callers should use Builder::Build() instead of invoking this constructor.
  TestingProfile(
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
      base::Optional<OTRProfileID> otr_profile_id);

  ~TestingProfile() override;

  // !!!!!!!! WARNING: THIS IS GENERALLY NOT SAFE TO CALL! !!!!!!!!
  // This bypasses the BrowserContextDependencyManager, and in particular, it
  // destroys any previously-created HistoryService. That means any other
  // KeyedServices that depend on HistoryService may be left with dangling
  // pointers.
  // Instead, use Builder::AddTestingFactory to inject your own factories.
  // !!!!!!!! WARNING: THIS IS GENERALLY NOT SAFE TO CALL! !!!!!!!!
  // Creates the history service. Returns true on success.
  // TODO(crbug.com/1106699): Remove this API and adopt the Builder instead.
  bool CreateHistoryService() WARN_UNUSED_RESULT;

  // Creates a WebDataService. If not invoked, the web data service is NULL.
  // TODO(crbug.com/1106699): Remove this API and adopt the Builder instead.
  void CreateWebDataService();

  // Allow setting a profile as Guest after-the-fact to simplify some tests.
  void SetGuestSession(bool guest);

  // Allow setting the return value of IsNewProfile.
  void SetIsNewProfile(bool is_new_profile);

  sync_preferences::TestingPrefServiceSyncable* GetTestingPrefService();

  // Called on the parent of an OffTheRecord |otr_profile|. Usually called from
  // the constructor of an OffTheRecord TestingProfile, but can also be used by
  // tests to provide an OffTheRecordProfileImpl instance.
  // |otr_profile| cannot be empty.
  void SetOffTheRecordProfile(std::unique_ptr<Profile> otr_profile);

  void SetSupervisedUserId(const std::string& id);

  // content::BrowserContext
  base::FilePath GetPath() override;
  base::FilePath GetPath() const override;
  base::Time GetCreationTime() const override;
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
#endif  // !defined(OS_ANDROID)
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  // Do not override IsOffTheRecord to turn a normal profile into an incognito
  // profile dynamically.
  bool IsOffTheRecord() final;
  bool IsOffTheRecord() const final;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsMainProfile() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  const OTRProfileID& GetOTRProfileID() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
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

  TestingProfile* AsTestingProfile() override;

  // Profile
  std::string GetProfileUserName() const override;

  // TODO(https://crbug.com/1191315): Remove default value after auditing and
  // updating use cases.
  Profile* GetOffTheRecordProfile(const OTRProfileID& otr_profile_id,
                                  bool create_if_needed = true) override;
  std::vector<Profile*> GetAllOffTheRecordProfiles() override;
  void DestroyOffTheRecordProfile(Profile* otr_profile) override;
  bool HasOffTheRecordProfile(const OTRProfileID& otr_profile_id) override;
  bool HasAnyOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  const Profile* GetOriginalProfile() const override;
  bool IsSupervised() const override;
  bool IsChild() const override;
  bool AllowsBrowserWindows() const override;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  void SetExtensionSpecialStoragePolicy(
      ExtensionSpecialStoragePolicy* extension_special_storage_policy);
#endif
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;

  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
#if !defined(OS_ANDROID)
  ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;
#endif  // !defined(OS_ANDROID)
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

  void set_last_session_exited_cleanly(bool value) {
    last_session_exited_cleanly_ = value;
  }
  bool IsSameOrParent(Profile* profile) override;
  base::Time GetStartTime() const override;
  ProfileKey* GetProfileKey() const override;
  policy::SchemaRegistryService* GetPolicySchemaRegistryService() override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::UserCloudPolicyManagerChromeOS* GetUserCloudPolicyManagerChromeOS()
      override;
  policy::ActiveDirectoryPolicyManager* GetActiveDirectoryPolicyManager()
      override;
#else
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  policy::ProfilePolicyConnector* GetProfilePolicyConnector() override;
  const policy::ProfilePolicyConnector* GetProfilePolicyConnector()
      const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  bool IsGuestSession() const override;
  bool IsEphemeralGuestProfile() const override;
  bool IsNewProfile() const override;
  void SetExitType(ExitType exit_type) override {}
  ExitType GetLastSessionExitType() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ChangeAppLocale(const std::string&, AppLocaleChangedVia) override;
  void OnLogin() override {}
  void InitChromeOSPreferences() override {}
  ash::ScopedCrosSettingsTestHelper* ScopedCrosSettingsTestHelper();

  base::Optional<std::string> requested_locale() { return requested_locale_; }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Schedules a task on the history backend and runs a nested loop until the
  // task is processed.  This has the effect of blocking the caller until the
  // history service processes all pending requests.
  void BlockUntilHistoryProcessesPendingRequests();

  GURL GetHomePage() override;

  void SetCreationTimeForTesting(base::Time creation_time) override;

  PrefService* GetOffTheRecordPrefs() override;

  void RecordMainFrameNavigation() override {}

  void set_profile_name(const std::string& profile_name) {
    profile_name_ = profile_name;
  }

  using ProfileDestructionCallback = base::OnceCallback<void()>;
  void SetProfileDestructionObserver(ProfileDestructionCallback callback) {
    profile_destruction_callback_ = std::move(callback);
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
  sync_preferences::TestingPrefServiceSyncable* testing_prefs_ = nullptr;

  // Profile implementation.
  bool IsSignedIn() override;

 private:
  // Common initialization between the two constructors.
  void Init();

  // Finishes initialization when a profile is created asynchronously.
  void FinishInit();

  // TODO(https://crbug.com/1169142): Update when profile type variables are
  // removed (is_guest_session_, is_system_profile_, ...)
  void UpdateBrowserProfileType();

  // Creates a TestingPrefService and associates it with the TestingProfile.
  void CreateTestingPrefService();

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Creates a pref service that uses SupervisedUserPrefStore and associates
  // it with the TestingProfile.
  void CreatePrefServiceForSupervisedUser();
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  // Initializes |prefs_| for an incognito profile, derived from
  // |original_profile_|.
  void CreateIncognitoPrefService();

  // Creates a ProfilePolicyConnector.
  void CreateProfilePolicyConnector();

  std::unique_ptr<net::CookieStore, content::BrowserThread::DeleteOnIOThread>
      extensions_cookie_store_;

  std::map<OTRProfileID, std::unique_ptr<Profile>> otr_profiles_;
  TestingProfile* original_profile_ = nullptr;

  bool guest_session_ = false;

  bool allows_browser_windows_ = true;

  bool is_new_profile_ = false;

  std::string supervised_user_id_;

  // Whether the last session exited cleanly.
  bool last_session_exited_cleanly_ = true;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

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
  SimpleDependencyManager* simple_dependency_manager_{
      SimpleDependencyManager::GetInstance()};
  BrowserContextDependencyManager* browser_context_dependency_manager_{
      BrowserContextDependencyManager::GetInstance()};

  // Owned, but must be deleted on the IO thread so not placing in a
  // std::unique_ptr<>.
  content::MockResourceContext* resource_context_ = nullptr;

  std::unique_ptr<policy::SchemaRegistryService> schema_registry_service_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<policy::UserCloudPolicyManagerChromeOS>
      user_cloud_policy_manager_;
#else
  std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Weak pointer to a delegate for indicating that a profile was created.
  Delegate* delegate_ = nullptr;

  std::string profile_name_{kDefaultProfileUserName};

  base::Optional<bool> override_policy_connector_is_managed_;
  base::Optional<OTRProfileID> otr_profile_id_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::ScopedCrosSettingsTestHelper>
      scoped_cros_settings_test_helper_;

  base::Optional<std::string> requested_locale_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  std::unique_ptr<policy::PolicyService> policy_service_;

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  TestingPrefStore* supervised_user_pref_store_ = nullptr;
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_H_
