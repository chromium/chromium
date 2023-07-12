// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_

#include <stddef.h>
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

class PrefService;
class SupervisedUserServiceObserver;
class SupervisedUserServiceFactory;

namespace base {
class Version;
}  // namespace base

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace supervised_user {
class SupervisedUserSettingsService;

// This class handles all the information related to a given supervised profile
// (e.g. the default URL filtering behavior, or manual allowlist/denylist
// overrides).
class SupervisedUserService : public KeyedService,
                              public SupervisedUserURLFilter::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Allows the delegate to handle the (de)activation in a custom way.
    virtual void SetActive(bool active) = 0;
  };

  SupervisedUserService(const SupervisedUserService&) = delete;
  SupervisedUserService& operator=(const SupervisedUserService&) = delete;

  ~SupervisedUserService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  supervised_user::RemoteWebApprovalsManager& remote_web_approvals_manager() {
    return remote_web_approvals_manager_;
  }

  // Initializes this object.
  void Init();

  void SetDelegate(Delegate* delegate);

  // Returns the URL filter for filtering navigations and classifying sites in
  // the history view. Both this method and the returned filter may only be used
  // on the UI thread.
  supervised_user::SupervisedUserURLFilter* GetURLFilter();

  // Get the string used to identify an extension install or update request.
  // Public for testing.
  static std::string GetExtensionRequestId(const std::string& extension_id,
                                           const base::Version& version);

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

  // Returns true if the extensions permissions parental control is enabled.
  bool AreExtensionsPermissionsEnabled() const;

  // Returns true if the URL filtering parental control is enabled.
  bool IsURLFilteringEnabled() const;

  // Returns true if there is a custodian for the child.  A child can have
  // up to 2 custodians, and this returns true if they have at least 1.
  bool HasACustodian() const;

  void AddObserver(SupervisedUserServiceObserver* observer);
  void RemoveObserver(SupervisedUserServiceObserver* observer);

  // ProfileKeyedService override:
  void Shutdown() override;

  // SupervisedUserURLFilter::Observer implementation:
  void OnSiteListUpdated() override;

#if BUILDFLAG(IS_CHROMEOS)
  bool signout_required_after_supervision_enabled() {
    return signout_required_after_supervision_enabled_;
  }
  void set_signout_required_after_supervision_enabled() {
    signout_required_after_supervision_enabled_ = true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // TODO(https://crbug.com/1288986): Enable web filter metrics reporting in
  // LaCrOS.
  // Reports FamilyUser.WebFilterType and FamilyUser.ManagedSiteList
  // metrics. Ignores reporting when AreWebFilterPrefsDefault() is true.
  void ReportNonDefaultWebFilterValue() const;

  // Returns true if both: the user is a type of Family Link supervised account
  // and the platform supports Family Link supervision features.
  // This method should be prefered on gating child-specific features if there
  // is no dedicated method for the feature (e.g IsURLFilteringEnabled).
  virtual bool IsSubjectToParentalControls() const;

  // Updates the kFirstTimeInterstitialBannerState pref to indicate that the
  // user has been shown the interstitial banner. This will only update users
  // who haven't yet seen the banner.
  void MarkFirstTimeInterstitialBannerShown() const;

  // Returns true if the interstitial banner needs to be shown to user.
  bool ShouldShowFirstTimeInterstitialBanner() const;

  // Some Google-affiliated domains are not allowed to delete cookies for
  // supervised users.
  bool IsCookieDeletionDisabled(const GURL& origin) const;

  // Use |SupervisedUserServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  // Public to allow visibility to iOS factory.
  SupervisedUserService(
      signin::IdentityManager* identity_manager,
      KidsChromeManagementClient* kids_chrome_management_client,
      PrefService& user_prefs,
      supervised_user::SupervisedUserSettingsService& settings_service,
      syncer::SyncService& sync_service,
      ValidateURLSupportCallback check_webstore_url_callback,
      std::unique_ptr<supervised_user::SupervisedUserURLFilter::Delegate>
          url_filter_delegate,
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

  void SetActive(bool active);

  void OnCustodianInfoChanged();

  void OnSupervisedUserIdChanged();

  void OnDefaultFilteringBehaviorChanged();

  bool IsSafeSitesEnabled() const;

  void OnSafeSitesSettingChanged();

  void UpdateAsyncUrlChecker();

  // Updates the manual overrides for hosts in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualHosts();

  // Updates the manual overrides for URLs in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualURLs();

  const raw_ref<PrefService> user_prefs_;

  const raw_ref<supervised_user::SupervisedUserSettingsService>
      settings_service_;

  const raw_ref<syncer::SyncService> sync_service_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  raw_ptr<KidsChromeManagementClient> kids_chrome_management_client_;

  bool active_ = false;

  raw_ptr<Delegate> delegate_;

  PrefChangeRegistrar pref_change_registrar_;

  // True only when |Init()| method has been called.
  bool did_init_ = false;

  // True only when |Shutdown()| method has been called.
  bool did_shutdown_ = false;

  SupervisedUserURLFilter url_filter_;

  const bool can_show_first_time_interstitial_banner_;

  // Manages remote web approvals.
  RemoteWebApprovalsManager remote_web_approvals_manager_;

  base::ObserverList<SupervisedUserServiceObserver>::Unchecked observer_list_;

#if BUILDFLAG(IS_CHROMEOS)
  bool signout_required_after_supervision_enabled_ = false;
#endif

  // TODO(https://crbug.com/1288986): Enable web filter metrics reporting in
  // LaCrOS.
  // When there is change between WebFilterType::kTryToBlockMatureSites and
  // WebFilterType::kCertainSites, both
  // prefs::kDefaultSupervisedUserFilteringBehavior and
  // prefs::kSupervisedUserSafeSites change. Uses this member to avoid duplicate
  // reports. Initialized in the SetActive().
  SupervisedUserURLFilter::WebFilterType current_web_filter_type_ =
      SupervisedUserURLFilter::WebFilterType::kMaxValue;

  base::WeakPtrFactory<SupervisedUserService> weak_ptr_factory_{this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_
