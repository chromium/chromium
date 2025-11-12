// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/supervised_user/core/browser/remote_web_approvals_manager.h"
#include "components/supervised_user/core/browser/supervised_user_content_filters_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/core/common/supervised_users.h"
#include "google_apis/gaia/gaia_id.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"
#endif

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

// Represents custodian data - who is responsible for managing the supervised
// user's settings.
class Custodian {
 public:
  Custodian();
  Custodian(std::string_view name,
            std::string_view email_address,
            std::string_view profile_image_url);
  Custodian(std::string_view name,
            std::string_view email_address,
            GaiaId obfuscated_gaia_id,
            std::string_view profile_image_url);
  Custodian(const Custodian& other);
  ~Custodian();

  std::string GetName() const { return name_; }
  std::string GetEmailAddress() const { return email_address_; }
  GaiaId GetObfuscatedGaiaId() const { return obfuscated_gaia_id_; }
  std::string GetProfileImageUrl() const { return profile_image_url_; }

 private:
  std::string name_;
  std::string email_address_;
  GaiaId obfuscated_gaia_id_;
  std::string profile_image_url_;
};

// Orchestrates cooperation between components of user supervision. Manages the
// lifecycle of url filtering, remote approval workflows, custodian data and
// incognito mode availability.
// The state of features is driven by changes to the following preferences:
// * `profile.managed_user_id` for url filtering, remove approvals and custodian
//    data,
// * `incognito.mode_availability` for incognito mode.
class SupervisedUserService : public KeyedService {
 public:
  // Delegate encapsulating platform-specific logic that is invoked from this
  // service.
  class PlatformDelegate {
   public:
    virtual ~PlatformDelegate() = default;

    // Returns the country code stored for this client.
    // Country code is in the format of lowercase ISO 3166-1 alpha-2. Example:
    // us, br, in.
    virtual std::string GetCountryCode() const = 0;

    // Returns the channel for the installation.
    virtual version_info::Channel GetChannel() const = 0;

    // Decides if incognito tabs should be closed. Tested when the supervision
    // features are enabled.
    virtual bool ShouldCloseIncognitoTabs() const = 0;

    // Close all incognito tabs for this service. Called when the supervision
    // features are enabled and require disabling of incognito mode.
    virtual void CloseIncognitoTabs() = 0;
  };

  SupervisedUserService(const SupervisedUserService&) = delete;
  SupervisedUserService& operator=(const SupervisedUserService&) = delete;

  ~SupervisedUserService() override;

  RemoteWebApprovalsManager& remote_web_approvals_manager() {
    return remote_web_approvals_manager_;
  }

  // Returns the URL filter for filtering navigations and classifying sites in
  // the history view. Both this method and the returned filter may only be used
  // on the UI thread.
  SupervisedUserURLFilter* GetURLFilter() const;

  // Returns true if the user is supervised locally (e.g. on the device).
  // Currently, local supervision is only supported on Android.
  bool IsSupervisedLocally() const;
  // Returns true if the user is supervised locally (e.g. on the device) and
  // requested browser content to be filtered.
  bool IsLocalBrowserFilteringEnabled() const;
  // Returns true if the user is supervised locally (e.g. on the device) and
  // requested search content to be filtered.
  bool IsLocalSearchFilteringEnabled() const;

  std::optional<Custodian> GetCustodian() const;
  std::optional<Custodian> GetSecondCustodian() const;

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

  // Use |SupervisedUserServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  // Public to allow visibility to iOS factory.
  SupervisedUserService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService& user_prefs,
      SupervisedUserSettingsService& settings_service,
      SupervisedUserContentFiltersService* content_filters_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<SupervisedUserURLFilter> url_filter,
      std::unique_ptr<SupervisedUserService::PlatformDelegate> platform_delegate
#if BUILDFLAG(IS_ANDROID)
      ,
      ContentFiltersObserverBridge::Factory
          content_filters_observer_bridge_factory
#endif
  );

 protected:
#if BUILDFLAG(IS_ANDROID)
  ContentFiltersObserverBridge* browser_content_filters_observer();
  ContentFiltersObserverBridge* search_content_filters_observer();
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  // Activates the service which controls managed settings of url filtering and
  // incognito mode.
  void SetSettingsServiceActive(bool active);

  void OnCustodianInfoChanged();
  void OnSupervisedUserIdChanged();

  // Handles the change of supervision status driven by Family Link parental
  // controls.
  void OnFamilyLinkParentalControlsEnabled();
  // Handler when supervision is disabled. Intentionally idempotent.
  void OnFamilyLinkParentalControlsDisabled();

  // Single handler for all url filter changes.
  // If present, `pref_name` indicates the actual pref that changed and might
  // dispatch additional work to the URL filter (eg. to update its internal data
  // structures). When `pref_name` is absent, the filter will refresh the data
  // structures unconditionally.
  void UpdateURLFilter(std::optional<std::string> pref_name = std::nullopt);

  // Interface for the above suitable for pref change registrar.
  void OnURLFilterChanged(const std::string& pref_name);

  // Adds url filtering change handlers, originating from Family Link.
  void AddURLFilterPrefChangeHandlers();
  // Adds sentinel handlers that prevent unintended changes to url filtering.
  void AddURLFilterPrefChangeSentinels();
  // Removes all url filtering change handlers. Intentionally idempotent.
  void RemoveURLFilterPrefChangeHandlers();
  // Add or remove all pref handlers related to custodians. The removal method
  // is intentionally idempotent.
  void AddCustodianPrefChangeHandlers();
  void RemoveCustodianPrefChangeHandlers();

#if BUILDFLAG(IS_ANDROID)
  // Enables content filters and then notifies observers.
  void EnableSearchContentFilters();
  void DisableSearchContentFilters();
  void EnableBrowserContentFilters();
  void DisableBrowserContentFilters();
#endif  // BUILDFLAG(IS_ANDROID)

  const raw_ref<PrefService> user_prefs_;

  const raw_ref<SupervisedUserSettingsService> settings_service_;

  const raw_ptr<SupervisedUserContentFiltersService> content_filters_service_;

  const raw_ptr<syncer::SyncService> sync_service_;

  raw_ptr<signin::IdentityManager> identity_manager_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<SupervisedUserURLFilter> url_filter_;

  std::unique_ptr<PlatformDelegate> platform_delegate_;

  // Registrar for core prefs that drive this service.
  PrefChangeRegistrar main_pref_change_registrar_;
  // Registrar for preferences that drive URL filtering. All prefs except for
  // the safe sites mode are observed only when the profile is subject to
  // parental controls. The safe sites pref is observed at all times, with
  // varying handlers for enabled or disabled parental controls.
  PrefChangeRegistrar url_filter_pref_change_registrar_;
  // Registrar for preferences that control custodian data. They're observed
  // only when the profile is subject to parental controls.
  PrefChangeRegistrar custodian_pref_change_registrar_;

#if BUILDFLAG(IS_ANDROID)
  // Observers for the content filters
  std::unique_ptr<ContentFiltersObserverBridge>
      browser_content_filters_observer_;
  std::unique_ptr<ContentFiltersObserverBridge>
      search_content_filters_observer_;

#endif  // BUILDFLAG(IS_ANDROID)

  // True only when |Shutdown()| method has been called.
  bool did_shutdown_ = false;

  // Manages remote web approvals.
  RemoteWebApprovalsManager remote_web_approvals_manager_;

  base::ObserverList<SupervisedUserServiceObserver>::Unchecked observer_list_;

#if BUILDFLAG(IS_CHROMEOS)
  bool signout_required_after_supervision_enabled_ = false;
#endif
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SERVICE_H_
