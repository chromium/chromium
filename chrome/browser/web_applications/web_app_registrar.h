// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/skia/include/core/SkColor.h"

class ProfileManager;

namespace apps {
struct ShareTarget;
}  // namespace apps

namespace content {
class StoragePartitionConfig;
}  // namespace content

namespace webapps {
enum class WebappInstallSource;
}

namespace web_app {

class IsolatedWebAppUrlInfo;
class WebAppRegistrarObserver;
class WebApp;
class WebAppProvider;

using Registry = std::map<webapps::AppId, std::unique_ptr<WebApp>>;

template <typename T>
struct ValueWithPolicy {
  T value;
  bool user_controllable;
};

// A registry model. This is a read-only container, which owns WebApp objects.
class WebAppRegistrar : public ProfileManagerObserver {
 public:
  explicit WebAppRegistrar(Profile* profile);
  WebAppRegistrar(const WebAppRegistrar&) = delete;
  WebAppRegistrar& operator=(const WebAppRegistrar&) = delete;
  ~WebAppRegistrar() override;

  bool is_empty() const { return registry_.empty(); }

  const WebApp* GetAppById(const webapps::AppId& app_id) const;

  // TODO(https://crbug.com/1182363): should be removed when id is introduced to
  // manifest.
  const WebApp* GetAppByStartUrl(const GURL& start_url) const;
  std::vector<webapps::AppId> GetAppsFromSyncAndPendingInstallation() const;
  std::vector<webapps::AppId> GetAppsPendingUninstall() const;

  bool AppsExistWithExternalConfigData() const;

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();
  void Shutdown();

  base::WeakPtr<WebAppRegistrar> AsWeakPtr();

  // Returns an webapps::AppId if there exists an app inside the registry that
  // has a specific install_url.
  absl::optional<webapps::AppId> LookUpAppIdByInstallUrl(
      const GURL& install_url) const;

  // Returns a WebApp if there exists an app inside the registry that
  // has a specific `install_source` with `install_url`.
  // If there are multiple matches an arbitrary one is returned.
  const WebApp* LookUpAppByInstallSourceInstallUrl(
      WebAppManagement::Type install_source,
      const GURL& install_url) const;

  // Returns whether the app with |app_id| is currently listed in the registry.
  // ie. we have data for web app manifest and icons, and this |app_id| can be
  // used in other registrar methods.
  bool IsInstalled(const webapps::AppId& app_id) const;

  // Returns whether the app is currently being uninstalled. This will be true
  // after uninstall has begun but before the OS integration hooks for uninstall
  // have completed. It will return false after uninstallation has completed.
  bool IsUninstalling(const webapps::AppId& app_id) const;

  // Returns whether the app with |app_id| is currently fully locally installed.
  // ie. app is not grey in chrome://apps UI surface and may have OS integration
  // like shortcuts. |IsLocallyInstalled| apps is a subset of |IsInstalled|
  // apps. On Chrome OS all apps are always locally installed.
  bool IsLocallyInstalled(const webapps::AppId& app_id) const;

  // Returns true if the app was actively installed, meaning the app has
  // involved some form of user or administrator action to either install it or
  // configure it to behave like an app.
  bool IsActivelyInstalled(const webapps::AppId& app_id) const;

  // Returns the permissions policy declared as declared in the manifest for
  // the app with |app_id|. This permissions policy is not yet parsed by the
  // PermissionsPolicyParser, and thus may contain invalid permissions and/or
  // origin allowlists.
  blink::ParsedPermissionsPolicy GetPermissionsPolicy(
      const webapps::AppId& app_id) const;

  // Returns true if there exists a currently installed app that has been
  // installed by PreinstalledWebAppManager.
  bool IsInstalledByDefaultManagement(const webapps::AppId& app_id) const;

  // Returns true if the app was preinstalled and NOT installed via any other
  // mechanism.
  bool WasInstalledByDefaultOnly(const webapps::AppId& app_id) const;

  // Returns true if the app was installed by user, false if default installed.
  bool WasInstalledByUser(const webapps::AppId& app_id) const;

  // Returns true if the app was installed by the device OEM. Always false on
  // on non-Chrome OS.
  bool WasInstalledByOem(const webapps::AppId& app_id) const;

  // Returns true if the app was installed by the SubApp API.
  bool WasInstalledBySubApp(const webapps::AppId& app_id) const;

  // Returns true if the app exists and is allowed to be uninstalled by the user
  // e.g. it is not policy installed.
  bool CanUserUninstallWebApp(const webapps::AppId& app_id) const;

  // Returns true if the prevent-close feature is enabled for the given app
  bool IsPreventCloseEnabled(const webapps::AppId& app_id) const;

  // Returns the AppIds and URLs of apps externally installed from
  // |install_source|.
  base::flat_map<webapps::AppId, base::flat_set<GURL>>
  GetExternallyInstalledApps(ExternalInstallSource install_source) const;

  // Returns the app id for |install_url| if the WebAppRegistrar is aware of an
  // externally installed app for it. Note that the |install_url| is the URL
  // that the app was installed from, which may not necessarily match the app's
  // current start URL.
  absl::optional<webapps::AppId> LookupExternalAppId(
      const GURL& install_url) const;

  // Returns whether the WebAppRegistrar has an externally installed app with
  // |app_id| from any |install_source|.
  bool HasExternalApp(const webapps::AppId& app_id) const;

  // Returns whether the WebAppRegistrar has an externally installed app with
  // |app_id| from |install_source|.
  bool HasExternalAppWithInstallSource(
      const webapps::AppId& app_id,
      ExternalInstallSource install_source) const;

  // Returns true if the web app with the |app_id| contains |protocol_scheme|
  // as one of its allowed launch protocols.
  bool IsAllowedLaunchProtocol(const webapps::AppId& app_id,
                               const std::string& protocol_scheme) const;

  // Returns true if the web app with the |app_id| contains |protocol_scheme|
  // as one of its disallowed launch protocols.
  bool IsDisallowedLaunchProtocol(const webapps::AppId& app_id,
                                  const std::string& protocol_scheme) const;

  // Returns true if the web app with the |app_id| has registered to handle
  // |protocol_scheme|.
  bool IsRegisteredLaunchProtocol(const webapps::AppId& app_id,
                                  const std::string& protocol_scheme) const;

  // Gets all allowed launch protocols from all installed apps.
  base::flat_set<std::string> GetAllAllowedLaunchProtocols() const;

  // Gets all disallowed launch protocols from all installed apps.
  base::flat_set<std::string> GetAllDisallowedLaunchProtocols() const;

  // Count a number of all apps which are installed by user (non-default).
  // Requires app registry to be in a ready state.
  int CountUserInstalledApps() const;

  // Count a number of all apps which are installed by the user but not locally
  // installed (aka installed via sync).
  int CountUserInstalledNotLocallyInstalledApps() const;

  // All names are UTF8 encoded.
  std::string GetAppShortName(const webapps::AppId& app_id) const;
  std::string GetAppDescription(const webapps::AppId& app_id) const;
  absl::optional<SkColor> GetAppThemeColor(const webapps::AppId& app_id) const;
  absl::optional<SkColor> GetAppDarkModeThemeColor(
      const webapps::AppId& app_id) const;
  absl::optional<SkColor> GetAppBackgroundColor(
      const webapps::AppId& app_id) const;
  absl::optional<SkColor> GetAppDarkModeBackgroundColor(
      const webapps::AppId& app_id) const;
  const GURL& GetAppStartUrl(const webapps::AppId& app_id) const;
  webapps::ManifestId GetAppManifestId(const webapps::AppId& app_id) const;
  const std::string* GetAppLaunchQueryParams(
      const webapps::AppId& app_id) const;
  const apps::ShareTarget* GetAppShareTarget(
      const webapps::AppId& app_id) const;
  const apps::FileHandlers* GetAppFileHandlers(
      const webapps::AppId& app_id) const;
  bool IsAppFileHandlerPermissionBlocked(const webapps::AppId& app_id) const;
  bool IsIsolated(const webapps::AppId& app_id) const;

  // Returns the state of the File Handling API for the given app.
  ApiApprovalState GetAppFileHandlerApprovalState(
      const webapps::AppId& app_id) const;
  // Returns true iff it's expected that File Handlers have been, **or are in
  // the process of being**, registered with the OS.
  bool ExpectThatFileHandlersAreRegisteredWithOs(
      const webapps::AppId& app_id) const;

  // Returns the start_url with launch_query_params appended to the end if any.
  GURL GetAppLaunchUrl(const webapps::AppId& app_id) const;

  // TODO(crbug.com/1469482): Replace uses of this with GetAppScope().
  absl::optional<GURL> GetAppScopeInternal(const webapps::AppId& app_id) const;

  DisplayMode GetAppDisplayMode(const webapps::AppId& app_id) const;
  absl::optional<mojom::UserDisplayMode> GetAppUserDisplayMode(
      const webapps::AppId& app_id) const;
  std::vector<DisplayMode> GetAppDisplayModeOverride(
      const webapps::AppId& app_id) const;

  // Returns the "url_handlers" field from the app manifest.
  apps::UrlHandlers GetAppUrlHandlers(const webapps::AppId& app_id) const;

  // Returns the `scope_extensions` field from the app manifest, ignoring
  // validation.
  base::flat_set<ScopeExtensionInfo> GetScopeExtensions(
      const webapps::AppId& app_id) const;

  // Returns the `scope_extensions` field from the app manifest after
  // validation. Entries with an origin that validated association with this web
  // app are returned. Other entries are removed. See
  // https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md
  // for association requirements.
  base::flat_set<ScopeExtensionInfo> GetValidatedScopeExtensions(
      const webapps::AppId& app_id) const;

  GURL GetAppManifestUrl(const webapps::AppId& app_id) const;

  base::Time GetAppLastBadgingTime(const webapps::AppId& app_id) const;
  base::Time GetAppLastLaunchTime(const webapps::AppId& app_id) const;
  base::Time GetAppFirstInstallTime(const webapps::AppId& app_id) const;

  absl::optional<webapps::WebappInstallSource> GetLatestAppInstallSource(
      const webapps::AppId& app_id) const;

  // Returns the "icons" field from the app manifest, use |WebAppIconManager| to
  // load icon bitmap data.
  std::vector<apps::IconInfo> GetAppIconInfos(
      const webapps::AppId& app_id) const;

  // Represents which icon sizes we successfully downloaded from the IconInfos.
  SortedSizesPx GetAppDownloadedIconSizesAny(
      const webapps::AppId& app_id) const;

  // Returns the "shortcuts" field from the app manifest, use
  // |WebAppIconManager| to load shortcuts menu icons bitmaps data.
  std::vector<WebAppShortcutsMenuItemInfo> GetAppShortcutsMenuItemInfos(
      const webapps::AppId& app_id) const;

  // Returns the Run on OS Login mode and enterprise policy value.
  ValueWithPolicy<RunOnOsLoginMode> GetAppRunOnOsLoginMode(
      const webapps::AppId& app_id) const;

  // Returns true iff it's expected that the app has been, **or is in
  // the process of being**, registered with the OS.
  absl::optional<RunOnOsLoginMode> GetExpectedRunOnOsLoginOsIntegrationState(
      const webapps::AppId& app_id) const;

  bool GetWindowControlsOverlayEnabled(const webapps::AppId& app_id) const;

  // Gets the IDs for all apps in `GetApps()`.
  std::vector<webapps::AppId> GetAppIds() const;

  // Gets the IDs for all sub-apps of parent app with id |parent_app_id|.
  std::vector<webapps::AppId> GetAllSubAppIds(
      const webapps::AppId& parent_app_id) const;

  // Maps all app IDs to their parent apps' IDs. Maps that do not have a parent
  // are omitted. This query should only be called with an AllAppsLock since all
  // apps are queried for their parent.
  base::flat_map<webapps::AppId, webapps::AppId> GetSubAppToParentMap() const;

  // Returns the "scope" field from the app manifest, or infers a scope from the
  // "start_url" field if unavailable. Returns an invalid GURL iff the |app_id|
  // does not refer to an installed web app.
  GURL GetAppScope(const webapps::AppId& app_id) const;

  // Returns whether |url| is in the scope of |app_id|.
  bool IsUrlInAppScope(const GURL& url, const webapps::AppId& app_id) const;

  // Returns whether |url| is in scope or scope_extensions of |app_id|.
  // Only checks scope if scope_extensions is disabled.
  bool IsUrlInAppExtendedScope(const GURL& url,
                               const webapps::AppId& app_id) const;

  // Returns the strength of matching |url| to the scope and scope_extensions of
  // |app_id|. Returns 0 if not in either.
  // Only checks scope if scope_extensions is disabled.
  size_t GetAppExtendedScopeScore(const GURL& url,
                                  const webapps::AppId& app_id) const;

  // Returns the strength of matching |url_spec| to the scope of |app_id|,
  // returns 0 if not in scope.
  size_t GetUrlInAppScopeScore(const std::string& url_spec,
                               const webapps::AppId& app_id) const;

  // Returns the app id of an app in the registry with the longest scope that is
  // a prefix of |url|, if any.
  absl::optional<webapps::AppId> FindAppWithUrlInScope(const GURL& url) const;

  // Returns true if there exists at least one app installed under |scope|.
  bool DoesScopeContainAnyApp(const GURL& scope) const;

  // Finds all apps that are installed under |scope|.
  std::vector<webapps::AppId> FindAppsInScope(const GURL& scope) const;

  // Returns the app id of an installed app in the registry with the longest
  // scope that is a prefix of |url|, if any. If |window_only| is specified,
  // only apps that open in app windows will be considered.
  absl::optional<webapps::AppId> FindInstalledAppWithUrlInScope(
      const GURL& url,
      bool window_only = false) const;

  // Returns true if there is an app that is not locally installed that has
  // a scope which is a prefix of |url|.
  bool IsNonLocallyInstalledAppWithUrlInScope(const GURL& url) const;

  // Returns whether the app is a shortcut app (as opposed to a PWA).
  bool IsShortcutApp(const webapps::AppId& app_id) const;

  // Returns true if the app with the specified |start_url| is currently fully
  // locally installed. The provided |start_url| must exactly match the launch
  // URL for the app; this method does not consult the app scope or match URLs
  // that fall within the scope.
  bool IsLocallyInstalled(const GURL& start_url) const;

  // Returns whether the app is pending successful navigation in order to
  // complete installation via the ExternallyManagedAppManager.
  bool IsPlaceholderApp(const webapps::AppId& app_id,
                        const WebAppManagement::Type source_type) const;

  // Returns an |app_id| if there is a placeholder app for |install_url|.
  // Returning a nullopt does not mean that there is no app for |install_url|,
  // just that there is no *placeholder app*.
  absl::optional<webapps::AppId> LookupPlaceholderAppId(
      const GURL& install_url,
      const WebAppManagement::Type source_type) const;

  bool IsSystemApp(const webapps::AppId& app_id) const;

  // Computes and returns the DisplayMode, accounting for user preference
  // to launch in a browser window and entries in the web app manifest.
  DisplayMode GetAppEffectiveDisplayMode(const webapps::AppId& app_id) const;

  // Computes and returns the DisplayMode only accounting for
  // entries in the web app manifest.
  DisplayMode GetEffectiveDisplayModeFromManifest(
      const webapps::AppId& app_id) const;

  // Computes and returns the unhashed app id from entries in the web app
  // manifest.
  GURL GetComputedManifestId(const webapps::AppId& app_id) const;

  // Returns whether the app should be opened in tabbed window mode.
  bool IsTabbedWindowModeEnabled(const webapps::AppId& app_id) const;

  GURL GetAppNewTabUrl(const webapps::AppId& app_id) const;

  // Returns the URL of the pinned home tab for tabbed apps which have this
  // enabled, otherwise returns nullopt.
  absl::optional<GURL> GetAppPinnedHomeTabUrl(
      const webapps::AppId& app_id) const;

  // Returns the current WebAppOsIntegrationState stored in the web_app DB.
  absl::optional<proto::WebAppOsIntegrationState>
  GetAppCurrentOsIntegrationState(const webapps::AppId& app_id) const;

  // Returns the StoragePartitionConfig of all StoragePartitions used by
  // |isolated_web_app_id|. Both the primary and any <controlledframe>
  // StoragePartitions will be returned.
  std::vector<content::StoragePartitionConfig>
  GetIsolatedWebAppStoragePartitionConfigs(
      const webapps::AppId& isolated_web_app_id) const;

  // Saves a record of the |partition_name| in
  // |isolated_web_app_in_memory_controlled_frame_partitions_|.
  // Then returns the StoragePartitionConfig of the in-memory
  // Controlled Frame partition.
  absl::optional<content::StoragePartitionConfig>
  SaveAndGetInMemoryControlledFramePartitionConfig(
      const IsolatedWebAppUrlInfo& url_info,
      const std::string& partition_name);

  // Returns true if a web app is set to be the default app to
  // capture links by the user. If an app is not locally installed or is a
  // shortcut, this returns false.
  bool CapturesLinksInScope(const webapps::AppId& app_id) const;

  // Searches for all apps that can control this url, and chooses the best one
  // that also captures links.
  absl::optional<webapps::AppId> FindAppThatCapturesLinksInScope(
      const GURL& url) const;

  // Returns true or false depending on whether the given `app` can be set as a
  // preferred app to capture the input URL. This returns false if:
  // 1. The app does not control the url, i.e. app scope has no match with
  //    `url`.
  // 2. There is another app in the DB that better controls `url`, i.e. has a
  //    higher scope score than `app`.
  // Note: This does NOT mean that `app` has user link capturing enabled.
  bool IsLinkCapturableByApp(const webapps::AppId& app, const GURL& url) const;

  // Returns a set of app ids that match the scope for user link capturing.
  std::vector<webapps::AppId> GetOverlappingAppsMatchingScope(
      const webapps::AppId& app_id) const;

  // Verifies if the scopes of 2 apps match for user link capturing.
  bool AppScopesMatchForUserLinkCapturing(const webapps::AppId& app_id1,
                                          const webapps::AppId& app_id2) const;

  // Returns information about apps that controls the input url, i.e. the app's
  // scope is a substring of the url passed to the API.
  base::flat_map<webapps::AppId, std::string> GetAllAppsControllingUrl(
      const GURL& url) const;

  bool IsPreferredAppForCapturingUrl(const GURL& url,
                                     const webapps::AppId& app_id);

#if BUILDFLAG(IS_MAC)
  bool AlwaysShowToolbarInFullscreen(const webapps::AppId& app_id) const;
  void NotifyAlwaysShowToolbarInFullscreenChanged(const webapps::AppId& app_id,
                                                  bool show);
#endif

  void AddObserver(WebAppRegistrarObserver* observer);
  void RemoveObserver(WebAppRegistrarObserver* observer);

  void NotifyWebAppProtocolSettingsChanged();
  void NotifyWebAppFileHandlerApprovalStateChanged(
      const webapps::AppId& app_id);
  void NotifyWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state);
  void NotifyWebAppDisabledStateChanged(const webapps::AppId& app_id,
                                        bool is_disabled);
  void NotifyWebAppsDisabledModeChanged();
  void NotifyWebAppLastBadgingTimeChanged(const webapps::AppId& app_id,
                                          const base::Time& time);
  void NotifyWebAppLastLaunchTimeChanged(const webapps::AppId& app_id,
                                         const base::Time& time);
  void NotifyWebAppFirstInstallTimeChanged(const webapps::AppId& app_id,
                                           const base::Time& time);
  void NotifyWebAppUserDisplayModeChanged(
      const webapps::AppId& app_id,
      mojom::UserDisplayMode user_display_mode);
  void NotifyWebAppRunOnOsLoginModeChanged(
      const webapps::AppId& app_id,
      RunOnOsLoginMode run_on_os_login_mode);
  void NotifyWebAppSettingsPolicyChanged();

#if !BUILDFLAG(IS_CHROMEOS)
  void NotifyWebAppUserLinkCapturingPreferencesChanged(
      const webapps::AppId& app_id,
      bool is_preferred);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(
      Profile* profile_to_be_deleted) override;
  void OnProfileManagerDestroying() override;

  // A filter must return false to skip the |web_app|.
  using Filter = bool (*)(const WebApp& web_app);

  // Only range-based |for| loop supported. Don't use AppSet directly.
  // Doesn't support registration and unregistration of WebApp while iterating.
  class AppSet {
   public:
    // An iterator class that can be used to access the list of apps.
    template <typename WebAppType>
    class Iter {
     public:
      using InternalIter = Registry::const_iterator;

      Iter(InternalIter&& internal_iter,
           InternalIter&& internal_end,
           Filter filter)
          : internal_iter_(std::move(internal_iter)),
            internal_end_(std::move(internal_end)),
            filter_(filter) {
        FilterAndSkipApps();
      }
      Iter(Iter&&) noexcept = default;
      Iter(const Iter&) = delete;
      Iter& operator=(const Iter&) = delete;
      ~Iter() = default;

      void operator++() {
        ++internal_iter_;
        FilterAndSkipApps();
      }
      WebAppType& operator*() const { return *internal_iter_->second; }
      bool operator!=(const Iter& iter) const {
        return internal_iter_ != iter.internal_iter_;
      }

     private:
      void FilterAndSkipApps() {
        while (internal_iter_ != internal_end_ && !filter_(**this))
          ++internal_iter_;
      }

      InternalIter internal_iter_;
      InternalIter internal_end_;
      Filter filter_;
    };

    AppSet(const WebAppRegistrar* registrar, Filter filter, bool empty);
    AppSet(AppSet&&) = default;
    AppSet(const AppSet&) = delete;
    AppSet& operator=(const AppSet&) = delete;
    ~AppSet();

    using iterator = Iter<WebApp>;
    using const_iterator = Iter<const WebApp>;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

   private:
    const raw_ptr<const WebAppRegistrar> registrar_;
    const Filter filter_;
    const bool empty_;
#if DCHECK_IS_ON()
    const size_t mutations_count_;
#endif
  };

  // Returns all apps in the registry (a superset) including stubs.
  AppSet GetAppsIncludingStubs() const;
  // Returns all apps excluding stubs for apps in sync install. Apps in sync
  // install are being installed and should be hidden for most subsystems. This
  // is a subset of GetAppsIncludingStubs().
  AppSet GetApps() const;

#if BUILDFLAG(IS_CHROMEOS)
  // Set (or replace existing) temporary experimental overrides for
  // UserDisplayMode. `overrides` maps app IDs to their overridden value.
  void SetUserDisplayModeOverridesForExperiment(
      base::flat_map<webapps::AppId, mojom::UserDisplayMode> overrides);
#endif

  // Returns a dict with debug values for each app in the registry, including
  // registrar-evaluated effective fields.
  base::Value AsDebugValue() const;

 protected:
  Profile* profile() const { return profile_; }

  void NotifyWebAppProfileWillBeDeleted(const webapps::AppId& app_id);

  Registry& registry() { return registry_; }
  void SetRegistry(Registry&& registry);

  void CountMutation();

  // Gets the IDs for all apps in `app_set`.
  std::vector<webapps::AppId> GetAppIdsForAppSet(const AppSet& app_set) const;

  bool registry_profile_being_deleted_ = false;

 private:
  const raw_ptr<Profile> profile_;
  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  base::ObserverList<WebAppRegistrarObserver, /*check_empty=*/true> observers_;

  Registry registry_;
#if DCHECK_IS_ON()
  size_t mutations_count_ = 0;
#endif

  base::flat_map<webapps::AppId, mojom::UserDisplayMode>
      user_display_mode_overrides_for_experiment_;

  // Keeps a record of in-memory (non-persistent) Storage Partitions created by
  // Isolated Web Apps' Controlled Frames. This table will expire on browser
  // shutdown same as in-memory Storage Partitions.
  base::flat_map<webapps::AppId, base::flat_set<std::string>>
      isolated_web_app_in_memory_controlled_frame_partitions_;

  base::WeakPtrFactory<WebAppRegistrar> weak_factory_{this};
};

// A writable API for the registry model. Mutable WebAppRegistrar must be used
// only by WebAppSyncBridge.
class WebAppRegistrarMutable : public WebAppRegistrar {
 public:
  explicit WebAppRegistrarMutable(Profile* profile);
  ~WebAppRegistrarMutable() override;

  void InitRegistry(Registry&& registry);

  WebApp* GetAppByIdMutable(const webapps::AppId& app_id);

  AppSet FilterAppsMutableForTesting(Filter filter);

  AppSet GetAppsIncludingStubsMutable();
  AppSet GetAppsMutable();

  using WebAppRegistrar::CountMutation;
  using WebAppRegistrar::registry;
};

// For testing and debug purposes.
bool IsRegistryEqual(const Registry& registry, const Registry& registry2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
