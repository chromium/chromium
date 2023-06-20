// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_

#include <iosfwd>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_sources.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/sync/model/string_ordinal.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#endif

namespace web_app {

class WebApp {
 public:
  explicit WebApp(const AppId& app_id);
  ~WebApp();

  // Copyable and move-assignable to support Copy-on-Write with Commit.
  WebApp(const WebApp& web_app);
  WebApp& operator=(WebApp&& web_app);

  // Explicitly disallow other copy ctors and assign operators.
  WebApp(WebApp&&) = delete;
  WebApp& operator=(const WebApp&) = delete;

  const AppId& app_id() const { return app_id_; }

  // UTF8 encoded application name. This name is not translated, use
  // WebAppRegistrar.GetAppShortName to get the translated name.
  const std::string& untranslated_name() const { return name_; }
  // UTF8 encoded long application description (a full application name). This
  // description is not translated, use WebAppRegistrar.GetAppDescription to get
  // the translated description.
  const std::string& untranslated_description() const { return description_; }

  const GURL& start_url() const { return start_url_; }

  const std::string* launch_query_params() const {
    return launch_query_params_ ? &launch_query_params_.value() : nullptr;
  }

  const GURL& scope() const { return scope_; }

  const absl::optional<SkColor>& theme_color() const { return theme_color_; }
  const absl::optional<SkColor>& dark_mode_theme_color() const {
    return dark_mode_theme_color_;
  }

  const absl::optional<SkColor>& background_color() const {
    return background_color_;
  }

  const absl::optional<SkColor>& dark_mode_background_color() const {
    return dark_mode_background_color_;
  }

  DisplayMode display_mode() const { return display_mode_; }

  absl::optional<mojom::UserDisplayMode> user_display_mode() const {
    return user_display_mode_;
  }

  const std::vector<DisplayMode>& display_mode_override() const {
    return display_mode_override_;
  }

  syncer::StringOrdinal user_page_ordinal() const { return user_page_ordinal_; }
  syncer::StringOrdinal user_launch_ordinal() const {
    return user_launch_ordinal_;
  }

  const absl::optional<WebAppChromeOsData>& chromeos_data() const {
    return chromeos_data_;
  }

  struct ClientData {
    ClientData();
    ~ClientData();
    ClientData(const ClientData& client_data);
    base::Value AsDebugValue() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    absl::optional<ash::SystemWebAppData> system_web_app_data;
#endif
  };

  const ClientData& client_data() const { return client_data_; }

  ClientData* client_data() { return &client_data_; }

  // Locally installed apps have shortcuts installed on various UI surfaces.
  // If app isn't locally installed, it is excluded from UIs and only listed as
  // a part of user's app library.
  bool is_locally_installed() const { return is_locally_installed_; }
  // Sync-initiated installation produces a stub app awaiting for full
  // installation process. The |is_from_sync_and_pending_installation| app has
  // only app_id, launch_url and sync_fallback_data fields defined, no icons. If
  // online install succeeds, icons get downloaded and all the fields get their
  // values. If online install fails, we do the fallback installation to
  // generate icons using |sync_fallback_data| fields.
  bool is_from_sync_and_pending_installation() const {
    return is_from_sync_and_pending_installation_;
  }

  // Represents whether the web app is being uninstalled.
  bool is_uninstalling() const { return is_uninstalling_; }

  // Represents the last time the Badging API was used.
  const base::Time& last_badging_time() const { return last_badging_time_; }
  // Represents the last time this app is launched.
  const base::Time& last_launch_time() const { return last_launch_time_; }
  // Represents the time when this app is installed.
  const base::Time& install_time() const { return install_time_; }
  // Represents the time when this app is updated.
  const base::Time& manifest_update_time() const {
    return manifest_update_time_;
  }

  // Represents the "icons" field in the manifest.
  const std::vector<apps::IconInfo>& manifest_icons() const {
    return manifest_icons_;
  }

  // Represents which icon sizes we successfully downloaded from the
  // |manifest_icons| for the given |purpose|.
  const SortedSizesPx& downloaded_icon_sizes(IconPurpose purpose) const;

  // Represents whether the icons for the web app are generated by Chrome due to
  // no suitable icons being available.
  bool is_generated_icon() const { return is_generated_icon_; }

  const apps::FileHandlers& file_handlers() const { return file_handlers_; }

  ApiApprovalState file_handler_approval_state() const {
    return file_handler_approval_state_;
  }

  OsIntegrationState file_handler_os_integration_state() const {
    return file_handler_os_integration_state_;
  }

  const absl::optional<apps::ShareTarget>& share_target() const {
    return share_target_;
  }

  const std::vector<std::string>& additional_search_terms() const {
    return additional_search_terms_;
  }

  const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers() const {
    return protocol_handlers_;
  }

  const base::flat_set<std::string>& allowed_launch_protocols() const {
    return allowed_launch_protocols_;
  }

  const base::flat_set<std::string>& disallowed_launch_protocols() const {
    return disallowed_launch_protocols_;
  }

  // URL within scope to launch for a "show on lock screen" action. Valid iff
  // this is considered a lock-screen-capable app.
  const GURL& lock_screen_start_url() const { return lock_screen_start_url_; }

  // URL within scope to launch for a "new note" action. Valid iff this is
  // considered a note-taking app.
  const GURL& note_taking_new_note_url() const {
    return note_taking_new_note_url_;
  }

  const apps::UrlHandlers& url_handlers() const { return url_handlers_; }

  const base::flat_set<ScopeExtensionInfo>& scope_extensions() const {
    return scope_extensions_;
  }

  const base::flat_set<ScopeExtensionInfo>& validated_scope_extensions() const {
    return validated_scope_extensions_;
  }

  RunOnOsLoginMode run_on_os_login_mode() const {
    return run_on_os_login_mode_;
  }

  absl::optional<RunOnOsLoginMode> run_on_os_login_os_integration_state()
      const {
    return run_on_os_login_os_integration_state_;
  }

  bool window_controls_overlay_enabled() const {
    return window_controls_overlay_enabled_;
  }

  // While local |name| and |theme_color| may vary from device to device, the
  // synced copies of these fields are replicated to all devices. The synced
  // copies are read by a device to generate a placeholder icon (if needed). Any
  // device may write new values to |sync_fallback_data|, random last update
  // wins.
  struct SyncFallbackData {
    SyncFallbackData();
    ~SyncFallbackData();
    // Copyable and move-assignable to support Copy-on-Write with Commit.
    SyncFallbackData(const SyncFallbackData& sync_fallback_data);
    SyncFallbackData(SyncFallbackData&& sync_fallback_data) noexcept;
    SyncFallbackData& operator=(SyncFallbackData&& sync_fallback_data);

    base::Value AsDebugValue() const;

    std::string name;
    absl::optional<SkColor> theme_color;
    GURL scope;
    std::vector<apps::IconInfo> icon_infos;
  };
  const SyncFallbackData& sync_fallback_data() const {
    return sync_fallback_data_;
  }

  // Represents the "shortcuts" field in the manifest.
  const std::vector<WebAppShortcutsMenuItemInfo>& shortcuts_menu_item_infos()
      const {
    return shortcuts_menu_item_infos_;
  }

  // Represents which shortcuts menu icon sizes we successfully downloaded for
  // each WebAppShortcutsMenuItemInfo.shortcuts_menu_manifest_icons.
  const std::vector<IconSizes>& downloaded_shortcuts_menu_icons_sizes() const {
    return downloaded_shortcuts_menu_icons_sizes_;
  }

  blink::mojom::CaptureLinks capture_links() const { return capture_links_; }

  const GURL& manifest_url() const { return manifest_url_; }

  ManifestId manifest_id() const;

  const absl::optional<LaunchHandler>& launch_handler() const {
    return launch_handler_;
  }

  const absl::optional<AppId>& parent_app_id() const { return parent_app_id_; }

  const blink::ParsedPermissionsPolicy& permissions_policy() const {
    return permissions_policy_;
  }

  absl::optional<webapps::WebappInstallSource> latest_install_source() const {
    return latest_install_source_;
  }

  const absl::optional<int64_t>& app_size_in_bytes() const {
    return app_size_in_bytes_;
  }
  const absl::optional<int64_t>& data_size_in_bytes() const {
    return data_size_in_bytes_;
  }

  struct ExternalManagementConfig {
    ExternalManagementConfig();
    ExternalManagementConfig(
        bool is_placeholder,
        const base::flat_set<GURL>& install_urls,
        const base::flat_set<std::string>& additional_policy_ids);
    ~ExternalManagementConfig();
    ExternalManagementConfig(
        const ExternalManagementConfig& external_management_config);
    ExternalManagementConfig& operator=(
        ExternalManagementConfig&& external_management_config);

    base::Value::Dict AsDebugValue() const;

    bool is_placeholder = false;
    base::flat_set<GURL> install_urls;

    // A list of additional terms to use when matching this app against
    // identifiers in admin policies (for shelf pinning, default file handlers,
    // etc).
    // Note that list is not meant to be an exhaustive enumeration of all
    // possible policy_ids but rather just a supplement for tricky cases.
    base::flat_set<std::string> additional_policy_ids;

    // Any new fields added should consider adding config merge logic to
    // BuildOperationsToDedupeInstallUrlConfigsIntoSelectedApp().
  };

  using ExternalConfigMap =
      base::flat_map<WebAppManagement::Type, ExternalManagementConfig>;

  const ExternalConfigMap& management_to_external_config_map() const {
    return management_to_external_config_map_;
  }

  const absl::optional<blink::Manifest::TabStrip> tab_strip() const {
    return tab_strip_;
  }

  // Only used on Mac.
  bool always_show_toolbar_in_fullscreen() const {
    return always_show_toolbar_in_fullscreen_;
  }

  const proto::WebAppOsIntegrationState& current_os_integration_states() const {
    return current_os_integration_states_;
  }

  // If present, signals that this app is an Isolated Web App, and contains
  // IWA-specific information like bundle location.
  struct IsolationData {
    // If present, signals that an update for this app is available locally and
    // waiting to be applied.
    struct PendingUpdateInfo {
      PendingUpdateInfo(IsolatedWebAppLocation location, base::Version version);
      ~PendingUpdateInfo();
      PendingUpdateInfo(const PendingUpdateInfo&);
      PendingUpdateInfo& operator=(const PendingUpdateInfo&);

      bool operator==(const PendingUpdateInfo&) const;
      bool operator!=(const PendingUpdateInfo&) const;

      base::Value AsDebugValue() const;
      friend std::ostream& operator<<(std::ostream& os,
                                      const PendingUpdateInfo& update_info) {
        return os << update_info.AsDebugValue();
      }

      IsolatedWebAppLocation location;
      base::Version version;

      // TODO(cmfcmf): Add further information about the update here, such as
      // whether it should be applied immediately, or only once the IWA is
      // closed.
    };

    IsolationData(IsolatedWebAppLocation location, base::Version version);
    IsolationData(IsolatedWebAppLocation location,
                  base::Version version,
                  const std::set<std::string>& controlled_frame_partitions,
                  const absl::optional<PendingUpdateInfo>& pending_update_info);
    ~IsolationData();
    IsolationData(const IsolationData&);
    IsolationData& operator=(const IsolationData&);
    IsolationData(IsolationData&&);
    IsolationData& operator=(IsolationData&&);

    bool operator==(const IsolationData&) const;
    bool operator!=(const IsolationData&) const;

    base::Value AsDebugValue() const;
    friend std::ostream& operator<<(std::ostream& os,
                                    const IsolationData& isolation_data) {
      return os << isolation_data.AsDebugValue();
    }

    // Sets the pending update info. Will `CHECK` if the type of
    // `pending_update_info.location` is not the same as `location`. In other
    // words, a `DevModeBundle` app cannot be updated to, e.g.,
    // `InstalledBundle`.
    void SetPendingUpdateInfo(
        const absl::optional<PendingUpdateInfo>& pending_update_info);

    const absl::optional<PendingUpdateInfo>& pending_update_info() const {
      return pending_update_info_;
    }

    IsolatedWebAppLocation location;
    base::Version version;
    std::set<std::string> controlled_frame_partitions;

   private:
    absl::optional<PendingUpdateInfo> pending_update_info_;
  };
  const absl::optional<IsolationData>& isolation_data() const {
    return isolation_data_;
  }

  // A Web App can be installed from multiple sources simultaneously. Installs
  // add a source to the app. Uninstalls remove a source from the app.
  void AddSource(WebAppManagement::Type source);
  void RemoveSource(WebAppManagement::Type source);
  bool HasAnySources() const;
  bool HasOnlySource(WebAppManagement::Type source) const;
  WebAppSources GetSources() const;

  bool IsSynced() const;
  bool IsPreinstalledApp() const;
  bool IsPolicyInstalledApp() const;
  bool IsSystemApp() const;
  bool IsWebAppStoreInstalledApp() const;
  bool IsSubAppInstalledApp() const;
  bool IsKioskInstalledApp() const;
  bool CanUserUninstallWebApp() const;
  bool WasInstalledByUser() const;
  // Returns the highest priority source. AppService assumes that every app has
  // just one install source.
  WebAppManagement::Type GetHighestPrioritySource() const;

  void SetName(const std::string& name);
  void SetDescription(const std::string& description);
  void SetStartUrl(const GURL& start_url);
  void SetLaunchQueryParams(absl::optional<std::string> launch_query_params);
  void SetScope(const GURL& scope);
  void SetThemeColor(absl::optional<SkColor> theme_color);
  void SetDarkModeThemeColor(absl::optional<SkColor> theme_color);
  void SetBackgroundColor(absl::optional<SkColor> background_color);
  void SetDarkModeBackgroundColor(absl::optional<SkColor> background_color);
  void SetDisplayMode(DisplayMode display_mode);
  void SetUserDisplayMode(mojom::UserDisplayMode user_display_mode);
  void SetDisplayModeOverride(std::vector<DisplayMode> display_mode_override);
  void SetUserPageOrdinal(syncer::StringOrdinal page_ordinal);
  void SetUserLaunchOrdinal(syncer::StringOrdinal launch_ordinal);
  void SetWebAppChromeOsData(absl::optional<WebAppChromeOsData> chromeos_data);
  void SetIsLocallyInstalled(bool is_locally_installed);
  void SetIsFromSyncAndPendingInstallation(
      bool is_from_sync_and_pending_installation);
  void SetIsUninstalling(bool is_uninstalling);
  void SetManifestIcons(std::vector<apps::IconInfo> manifest_icons);
  // Performs sorting and uniquifying of |sizes| if passed as vector.
  void SetDownloadedIconSizes(IconPurpose purpose, SortedSizesPx sizes);
  void SetIsGeneratedIcon(bool is_generated_icon);
  // Sets information about the shortcuts menu for the app.
  // `shortcuts_menu_item_infos` and `downloaded_sizes` must be the same length.
  void SetShortcutsMenuInfo(
      std::vector<WebAppShortcutsMenuItemInfo> shortcuts_menu_item_infos,
      std::vector<IconSizes> downloaded_sizes);
  void SetFileHandlers(apps::FileHandlers file_handlers);
  void SetFileHandlerApprovalState(ApiApprovalState approval_state);
  void SetFileHandlerOsIntegrationState(
      OsIntegrationState os_integration_state);
  void SetShareTarget(absl::optional<apps::ShareTarget> share_target);
  void SetAdditionalSearchTerms(
      std::vector<std::string> additional_search_terms);
  void SetProtocolHandlers(
      std::vector<apps::ProtocolHandlerInfo> protocol_handlers);
  void SetAllowedLaunchProtocols(
      base::flat_set<std::string> allowed_launch_protocols);
  void SetDisallowedLaunchProtocols(
      base::flat_set<std::string> disallowed_launch_protocols);
  void SetUrlHandlers(apps::UrlHandlers url_handlers);
  void SetScopeExtensions(base::flat_set<ScopeExtensionInfo> scope_extensions);
  void SetValidatedScopeExtensions(
      base::flat_set<ScopeExtensionInfo> validated_scope_extensions);
  void SetLockScreenStartUrl(const GURL& lock_screen_start_url);
  void SetNoteTakingNewNoteUrl(const GURL& note_taking_new_note_url);
  void SetLastBadgingTime(const base::Time& time);
  void SetLastLaunchTime(const base::Time& time);
  void SetInstallTime(const base::Time& time);
  void SetManifestUpdateTime(const base::Time& time);
  void SetRunOnOsLoginMode(RunOnOsLoginMode mode);
  void SetRunOnOsLoginOsIntegrationState(RunOnOsLoginMode os_integration_state);
  void SetSyncFallbackData(SyncFallbackData sync_fallback_data);
  void SetCaptureLinks(blink::mojom::CaptureLinks capture_links);
  void SetManifestUrl(const GURL& manifest_url);
  void SetManifestId(const ManifestId& manifest_id);
  void SetWindowControlsOverlayEnabled(bool enabled);
  void SetLaunchHandler(absl::optional<LaunchHandler> launch_handler);
  void SetParentAppId(const absl::optional<AppId>& parent_app_id);
  void SetPermissionsPolicy(blink::ParsedPermissionsPolicy permissions_policy);
  void SetLatestInstallSource(
      absl::optional<webapps::WebappInstallSource> latest_install_source);
  void SetAppSizeInBytes(absl::optional<int64_t> app_size_in_bytes);
  void SetDataSizeInBytes(absl::optional<int64_t> data_size_in_bytes);
  void SetWebAppManagementExternalConfigMap(
      ExternalConfigMap management_to_external_config_map);
  void SetTabStrip(absl::optional<blink::Manifest::TabStrip> tab_strip);
  void SetCurrentOsIntegrationStates(
      proto::WebAppOsIntegrationState current_os_integration_states);
  void SetIsolationData(IsolationData isolation_data);

  void AddPlaceholderInfoToManagementExternalConfigMap(
      WebAppManagement::Type source_type,
      bool is_placeholder);

  // This adds an install_url per management type (source) for the
  // ExternalConfigMap.
  void AddInstallURLToManagementExternalConfigMap(WebAppManagement::Type type,
                                                  GURL install_url);

  // This adds a policy_id per management type (source) for the
  // ExternalConfigMap.
  void AddPolicyIdToManagementExternalConfigMap(WebAppManagement::Type type,
                                                std::string policy_id);

  // Encapsulate the addition of install_url and is_placeholder information
  // for cases where both need to be added.
  void AddExternalSourceInformation(WebAppManagement::Type source_type,
                                    GURL install_url,
                                    bool is_placeholder);

  bool RemoveInstallUrlForSource(WebAppManagement::Type type,
                                 const GURL& install_url);

  // Only used on Mac, determines if the toolbar should be permanently shown
  // when in fullscreen.
  void SetAlwaysShowToolbarInFullscreen(bool show);

  // For logging and debug purposes.
  bool operator==(const WebApp&) const;
  bool operator!=(const WebApp&) const;
  // Used by the WebAppTest suite to cover only platform agnostic fields to
  // avoid needing multiple platform specific expectation files per test.
  // Otherwise, the same as AsDebugValue().
  base::Value AsDebugValueWithOnlyPlatformAgnosticFields() const;
  base::Value AsDebugValue() const;

 private:
  friend class WebAppDatabase;
  friend std::ostream& operator<<(std::ostream&, const WebApp&);

  AppId app_id_;

  // This set always contains at least one source.
  WebAppSources sources_;

  std::string name_;
  std::string description_;
  GURL start_url_;
  absl::optional<std::string> launch_query_params_;
  GURL scope_;
  absl::optional<SkColor> theme_color_;
  absl::optional<SkColor> dark_mode_theme_color_;
  absl::optional<SkColor> background_color_;
  absl::optional<SkColor> dark_mode_background_color_;
  DisplayMode display_mode_ = DisplayMode::kUndefined;
  absl::optional<mojom::UserDisplayMode> user_display_mode_ = absl::nullopt;
  std::vector<DisplayMode> display_mode_override_;
  syncer::StringOrdinal user_page_ordinal_;
  syncer::StringOrdinal user_launch_ordinal_;
  absl::optional<WebAppChromeOsData> chromeos_data_;
  bool is_locally_installed_ = false;
  bool is_from_sync_and_pending_installation_ = false;
  // Note: This field is not persisted in the database.
  // TODO(crbug.com/1162477): Add this field to the protocol buffer file and
  // other places to save it to the database, and then make sure to continue
  // uninstallation on startup if any web apps have this field set to true.
  bool is_uninstalling_ = false;
  std::vector<apps::IconInfo> manifest_icons_;
  SortedSizesPx downloaded_icon_sizes_any_;
  SortedSizesPx downloaded_icon_sizes_monochrome_;
  SortedSizesPx downloaded_icon_sizes_maskable_;
  bool is_generated_icon_ = false;
  std::vector<WebAppShortcutsMenuItemInfo> shortcuts_menu_item_infos_;
  std::vector<IconSizes> downloaded_shortcuts_menu_icons_sizes_;
  apps::FileHandlers file_handlers_;
  absl::optional<apps::ShareTarget> share_target_;
  std::vector<std::string> additional_search_terms_;
  std::vector<apps::ProtocolHandlerInfo> protocol_handlers_;
  base::flat_set<std::string> allowed_launch_protocols_;
  base::flat_set<std::string> disallowed_launch_protocols_;
  // TODO(crbug.com/1072058): No longer aiming to ship, remove.
  apps::UrlHandlers url_handlers_;
  base::flat_set<ScopeExtensionInfo> scope_extensions_;
  base::flat_set<ScopeExtensionInfo> validated_scope_extensions_;
  GURL lock_screen_start_url_;
  GURL note_taking_new_note_url_;
  base::Time last_badging_time_;
  base::Time last_launch_time_;
  base::Time install_time_;
  base::Time manifest_update_time_;
  RunOnOsLoginMode run_on_os_login_mode_ = RunOnOsLoginMode::kNotRun;
  // Tracks if the app run on os login mode has been registered with the OS.
  // This might go out of sync with actual OS integration status, as Chrome does
  // not actively monitor OS registries.
  // TODO(crbug.com/1401125): Remove after all OS Integration sub managers have
  // been implemented and Synchronize() is running fine.
  absl::optional<RunOnOsLoginMode> run_on_os_login_os_integration_state_;
  SyncFallbackData sync_fallback_data_;
  blink::mojom::CaptureLinks capture_links_ =
      blink::mojom::CaptureLinks::kUndefined;
  ClientData client_data_;
  GURL manifest_url_;
  ManifestId manifest_id_;
  // The state of the user's approval of the app's use of the File Handler API.
  ApiApprovalState file_handler_approval_state_ =
      ApiApprovalState::kRequiresPrompt;
  // Tracks whether file handling has been or should be enabled at the OS level.
  // This might go out of sync with actual OS integration status, as Chrome does
  // not actively monitor OS registries.
  OsIntegrationState file_handler_os_integration_state_ =
      OsIntegrationState::kDisabled;
  bool window_controls_overlay_enabled_ = false;
  absl::optional<LaunchHandler> launch_handler_;
  absl::optional<AppId> parent_app_id_;
  blink::ParsedPermissionsPolicy permissions_policy_;
  // The source of the latest install. WebAppRegistrar provides range
  // validation. Optional only to support legacy installations, since this used
  // to be tracked as a pref. It might also be null if the value read from the
  // database is not recognized by this client.
  absl::optional<webapps::WebappInstallSource> latest_install_source_;

  absl::optional<int64_t> app_size_in_bytes_;
  absl::optional<int64_t> data_size_in_bytes_;

  // Maps WebAppManagement::Type to config values for externally installed apps,
  // like is_placeholder and install URLs.
  ExternalConfigMap management_to_external_config_map_;

  absl::optional<blink::Manifest::TabStrip> tab_strip_;

  // Only used on Mac.
  bool always_show_toolbar_in_fullscreen_ = true;

  proto::WebAppOsIntegrationState current_os_integration_states_ =
      proto::WebAppOsIntegrationState();

  absl::optional<IsolationData> isolation_data_;

  // New fields must be added to:
  //  - |operator==|
  //  - AsDebugValue()
  //  - WebAppDatabase::CreateWebApp()
  //  - WebAppDatabase::CreateWebAppProto()
  //  - CreateRandomWebApp()
  //  - WebAppTest.EmptyAppAsDebugValue
  //  - WebAppTest.SampleAppAsDebugValue
  //  - web_app.proto
  // If parsed from manifest, also add to:
  //  - GetManifestDataChanges() inside manifest_update_utils.h
  //  - SetWebAppManifestFields()
  // If the field relates to the app icons, add revert logic for it in:
  // - ManifestUpdateCheckCommand::RevertAppIconChanges()
};

// For logging and debug purposes.
std::ostream& operator<<(std::ostream& out, const WebApp& app);

bool operator==(const WebApp::SyncFallbackData& sync_fallback_data1,
                const WebApp::SyncFallbackData& sync_fallback_data2);
bool operator!=(const WebApp::SyncFallbackData& sync_fallback_data1,
                const WebApp::SyncFallbackData& sync_fallback_data2);

std::ostream& operator<<(
    std::ostream& out,
    const WebApp::ExternalManagementConfig& management_config);
bool operator==(const WebApp::ExternalManagementConfig& management_config1,
                const WebApp::ExternalManagementConfig& management_config2);
bool operator!=(const WebApp::ExternalManagementConfig& management_config1,
                const WebApp::ExternalManagementConfig& management_config2);

namespace proto {

bool operator==(const WebAppOsIntegrationState& os_integration_state1,
                const WebAppOsIntegrationState& os_integration_state2);

bool operator!=(const WebAppOsIntegrationState& os_integration_state1,
                const WebAppOsIntegrationState& os_integration_state2);

}  // namespace proto

std::vector<std::string> GetSerializedAllowedOrigins(
    const blink::ParsedPermissionsPolicyDeclaration
        permissions_policy_declaration);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_H_
