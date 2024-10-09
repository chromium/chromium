// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_

#include <stddef.h>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/supervised_user/core/browser/remote_web_approvals_manager.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;
class SupervisedUserServiceObserver;
class SupervisedUserServiceFactory;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace supervised_user {
class SupervisedUserSettingsService;

// This class handles all the information related to a given supervised profile
// (e.g. the default URL filtering behavior, or manual allowlist/denylist
// overrides).
class SupervisedUserService : public KeyedService {
 public:
  // Delegate encapsulating platform-specific logic that is invoked from SUS.
  class PlatformDelegate {
   public:
    virtual ~PlatformDelegate() = default;

    // Returns the country code stored for this client.
    // Country code is in the format of lowercase ISO 3166-1 alpha-2. Example:
    // us, br, in.
    virtual std::string GetCountryCode() const = 0;

    // Returns the channel for the installation.
    virtual version_info::Channel GetChannel() const = 0;

    // Close all incognito tabs for this service. Called the profile becomes
    // supervised.
    virtual void CloseIncognitoTabs() = 0;
  };

  SupervisedUserService(const SupervisedUserService&) = delete;
  SupervisedUserService& operator=(const SupervisedUserService&) = delete;

  ~SupervisedUserService() override;

  supervised_user::RemoteWebApprovalsManager& remote_web_approvals_manager() {
    return remote_web_approvals_manager_;
  }

  // Initializes this object.
  void Init();

  // Returns the URL filter for filtering navigations and classifying sites in
  // the history view. Both this method and the returned filter may only be used
  // on the UI thread.
  supervised_user::SupervisedUserURLFilter* GetURLFilter() const;

  // Returns the email address of the custodian.
  std::string GetCustodianEmailAddress() const;

  // Returns the obfuscated GAIA id of the custodian.
  std::string GetCustodianObfuscatedGaiaId() const;

  // Returns the name of the custodian, or the email address if the name is
  // empty.
  std::string GetCustodianName() const;

  // Returns the email address of the second custodian, or the empty string
  // if there is no second custodian.
  std::string GetSecondCustodianEmailAddress() const;

  // Returns the obfuscated GAIA id of the second custodian or the empty
  // string if there is no second custodian.
  std::string GetSecondCustodianObfuscatedGaiaId() const;

  // Returns the name of the second custodian, or the email address if the name
  // is empty, or the empty string if there is no second custodian.
  std::string GetSecondCustodianName() const;

  // Returns true if there is a custodian for the child.  A child can have
  // up to 2 custodians, and this returns true if they have at least 1.
  bool HasACustodian() const;

  // Returns true if the url is blocked due to supervision restrictions on the
  // primary account user.
  bool IsBlockedURL(const GURL& url) const;

  void AddObserver(SupervisedUserServiceObserver* observer);
  void RemoveObserver(SupervisedUserServiceObserver* observer);

  // ProfileKeyedService override:
  void Shutdown() override;

#if BUILDFLAG(IS_CHROMEOS)
  bool signout_required_after_supervision_enabled() {
    return signout_required_after_supervision_enabled_;
  }
  void set_signout_required_after_supervision_enabled() {
    signout_required_after_supervision_enabled_ = true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Updates the kFirstTimeInterstitialBannerState pref to indicate that the
  // user has been shown the interstitial banner. This will only update users
  // who haven't yet seen the banner.
  void MarkFirstTimeInterstitialBannerShown() const;

  // Returns true if the interstitial banner needs to be shown to user.
  bool ShouldShowFirstTimeInterstitialBanner() const;

  // Use |SupervisedUserServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  // Public to allow visibility to iOS factory.
  SupervisedUserService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService& user_prefs,
      supervised_user::SupervisedUserSettingsService& settings_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<supervised_user::SupervisedUserURLFilter::Delegate>
          url_filter_delegate,
      std::unique_ptr<supervised_user::SupervisedUserService::PlatformDelegate>
          platform_delegate,
      bool can_show_first_time_interstitial_banner);

 private:
  friend class SupervisedUserServiceExtensionTestBase;
  friend class ::SupervisedUserServiceFactory;
  FRIEND_TEST_ALL_PREFIXES(
      SupervisedUserServiceExtensionTest,
      ExtensionManagementPolicyProviderWithoutSUInitiatedInstalls);
  FRIEND_TEST_ALL_PREFIXES(
      SupervisedUserServiceExtensionTest,
      ExtensionManagementPolicyProviderWithSUInitiatedInstalls);
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserServiceTest, InterstitialBannerState);
  FRIEND_TEST_ALL_PREFIXES(SupervisedUserNavigationThrottleTest,
                           BlockedMatureSitesRecordedInBlockSafeSitesBucket);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleTest,
                           BlockedMatureSitesRecordedInBlockSafeSitesBucket);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleTest,
                           ClassificationIsFasterThanHttp);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleTest,
                           ClassificationIsSlowerThanHttp);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleTest,
                           ReverseOrderOfResponsesAfterContentIsReady);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleParallelizationTest,
                           ClassificationIsFasterThanHttp);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleParallelizationTest,
                           ClassificationIsSlowerThanHttp);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleParallelizationTest,
                           ShortCircuitsSynchronousBlock);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleParallelizationTest,
                           HandlesLateAsynchronousBlock);
  FRIEND_TEST_ALL_PREFIXES(ClassifyUrlNavigationThrottleParallelizationTest,
                           OutOfOrderClassification);

  // Method used in testing to set the given test_filter as the url_filter_
  void SetURLFilterForTesting(
      std::unique_ptr<SupervisedUserURLFilter> test_filter);

  FirstTimeInterstitialBannerState GetUpdatedBannerState(
      FirstTimeInterstitialBannerState original_state);

  void SetActive(bool active);

  void OnCustodianInfoChanged();

  void OnSupervisedUserIdChanged();

  void OnDefaultFilteringBehaviorChanged();

  void OnSafeSitesSettingChanged();

  // Updates the manual overrides for hosts in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualHosts();

  // Updates the manual overrides for URLs in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualURLs();

  const raw_ref<PrefService> user_prefs_;

  const raw_ref<supervised_user::SupervisedUserSettingsService>
      settings_service_;

  const raw_ptr<syncer::SyncService> sync_service_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  bool active_ = false;

  std::unique_ptr<PlatformDelegate> platform_delegate_;

  PrefChangeRegistrar pref_change_registrar_;

  // True only when |Init()| method has been called.
  bool did_init_ = false;

  // True only when |Shutdown()| method has been called.
  bool did_shutdown_ = false;

  std::unique_ptr<SupervisedUserURLFilter> url_filter_;

  const bool can_show_first_time_interstitial_banner_;

  // Manages remote web approvals.
  RemoteWebApprovalsManager remote_web_approvals_manager_;

  base::ObserverList<SupervisedUserServiceObserver>::Unchecked observer_list_;

#if BUILDFLAG(IS_CHROMEOS)
  bool signout_required_after_supervision_enabled_ = false;
#endif

  // When there is change between WebFilterType::kTryToBlockMatureSites and
  // WebFilterType::kCertainSites, both
  // prefs::kDefaultSupervisedUserFilteringBehavior and
  // prefs::kSupervisedUserSafeSites change. Uses this member to avoid duplicate
  // reports. Initialized in the SetActive().
  WebFilterType current_web_filter_type_ = WebFilterType::kMaxValue;

  base::WeakPtrFactory<SupervisedUserService> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_
