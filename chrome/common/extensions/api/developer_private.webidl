// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DEPRECATED: Use OpenDevTools.
dictionary InspectOptions {
  required DOMString extension_id;
  required (DOMString or long) render_process_id;
  required (DOMString or long) render_view_id;
  required boolean incognito;
};

enum ExtensionType {
  "HOSTED_APP",
  "PLATFORM_APP",
  "LEGACY_PACKAGED_APP",
  "EXTENSION",
  "THEME",
  "SHARED_MODULE"
};

enum Location {
  "FROM_STORE",
  "UNPACKED",
  "THIRD_PARTY",
  "INSTALLED_BY_DEFAULT",
  // "Unknown" includes crx's installed from chrome://extensions.
  "UNKNOWN"
};

enum ViewType {
  "APP_WINDOW",
  "BACKGROUND_CONTENTS",
  "COMPONENT",
  "EXTENSION_BACKGROUND_PAGE",
  "EXTENSION_GUEST",
  "EXTENSION_POPUP",
  "EXTENSION_SERVICE_WORKER_BACKGROUND",
  "TAB_CONTENTS",
  "OFFSCREEN_DOCUMENT",
  "EXTENSION_SIDE_PANEL",
  "DEVELOPER_TOOLS"
};

enum ErrorType {
  "MANIFEST",
  "RUNTIME"
};

enum ErrorLevel {
  "LOG",
  "WARN",
  "ERROR"
};

enum ExtensionState {
  "ENABLED",
  "DISABLED",
  "TERMINATED",
  "BLOCKLISTED"
};

enum CommandScope {
  "GLOBAL",
  "CHROME"
};

enum SafetyCheckWarningReason {
  "UNPUBLISHED",
  "POLICY",
  "MALWARE",
  "OFFSTORE",
  "UNWANTED",
  "NO_PRIVACY_PRACTICE"
};

dictionary AccessModifier {
  required boolean isEnabled;
  required boolean isActive;
};

dictionary StackFrame {
  required long lineNumber;
  required long columnNumber;
  required DOMString url;
  required DOMString functionName;
};

dictionary ManifestError {
  required ErrorType type;
  required DOMString extensionId;
  required boolean fromIncognito;
  required DOMString source;
  required DOMString message;
  required long id;
  required DOMString manifestKey;
  DOMString manifestSpecific;
};

dictionary RuntimeError {
  required ErrorType type;
  required DOMString extensionId;
  required boolean fromIncognito;
  required DOMString source;
  required DOMString message;
  required long id;
  required ErrorLevel severity;
  required DOMString contextUrl;
  required long occurrences;
  required long renderViewId;
  required long renderProcessId;
  required boolean canInspect;
  required boolean isServiceWorker;
  required sequence<StackFrame> stackTrace;
};

dictionary DisableReasons {
  required boolean suspiciousInstall;
  required boolean corruptInstall;
  required boolean updateRequired;
  required boolean publishedInStoreRequired;
  required boolean blockedByPolicy;
  required boolean reloading;
  required boolean custodianApprovalRequired;
  required boolean parentDisabledPermissions;
  required boolean unsupportedManifestVersion;
  required boolean unsupportedDeveloperExtension;
};

dictionary OptionsPage {
  required boolean openInTab;
  required DOMString url;
};

dictionary HomePage {
  required DOMString url;
  required boolean specified;
};

dictionary ExtensionView {
  required DOMString url;
  required long renderProcessId;
  // This actually refers to a render frame.
  required long renderViewId;
  required boolean incognito;
  required boolean isIframe;
  required ViewType type;
};

enum HostAccess {
  "ON_CLICK",
  "ON_SPECIFIC_SITES",
  "ON_ALL_SITES"
};

dictionary SafetyCheckStrings {
  DOMString panelString;
  DOMString detailString;
};

dictionary ControlledInfo {
  required DOMString text;
};

dictionary Command {
  required DOMString description;
  required DOMString keybinding;
  required DOMString name;
  required boolean isActive;
  required CommandScope scope;
  required boolean isExtensionAction;
};

dictionary DependentExtension {
  required DOMString id;
  required DOMString name;
};

dictionary Permission {
  required DOMString message;
  required sequence<DOMString> submessages;
};

dictionary SiteControl {
  // The host pattern for the site.
  required DOMString host;
  // Whether the pattern has been granted.
  required boolean granted;
};

dictionary RuntimeHostPermissions {
  // True if |hosts| contains an all hosts like pattern.
  required boolean hasAllHosts;

  // The current HostAccess setting for the extension.
  required HostAccess hostAccess;

  // The site controls for all granted and requested patterns.
  required sequence<SiteControl> hosts;
};

dictionary Permissions {
  required sequence<Permission> simplePermissions;

  // Only populated for extensions that can be affected by the runtime host
  // permissions feature.
  RuntimeHostPermissions runtimeHostPermissions;
};

dictionary ExtensionInfo {
  DOMString blocklistText;
  SafetyCheckStrings safetyCheckText;
  required sequence<Command> commands;
  ControlledInfo controlledInfo;
  required sequence<DependentExtension> dependentExtensions;
  required DOMString description;
  required DisableReasons disableReasons;
  required AccessModifier errorCollection;
  required AccessModifier fileAccess;
  required boolean fileAccessPendingChange;
  required HomePage homePage;
  required DOMString iconUrl;
  required DOMString id;
  required AccessModifier incognitoAccess;
  required AccessModifier userScriptsAccess;
  required boolean incognitoAccessPendingChange;
  required sequence<DOMString> installWarnings;
  required boolean isCommandRegistrationHandledExternally;
  DOMString launchUrl;
  required Location location;
  DOMString locationText;
  required sequence<ManifestError> manifestErrors;
  required DOMString manifestHomePageUrl;
  required boolean mustRemainInstalled;
  required DOMString name;
  required boolean offlineEnabled;
  OptionsPage optionsPage;
  DOMString path;
  required Permissions permissions;
  DOMString prettifiedPath;
  DOMString recommendationsUrl;
  required sequence<RuntimeError> runtimeErrors;
  required sequence<DOMString> runtimeWarnings;
  required ExtensionState state;
  required ExtensionType type;
  required DOMString updateUrl;
  required boolean userMayModify;
  required DOMString version;
  required sequence<ExtensionView> views;
  required DOMString webStoreUrl;
  required boolean showSafeBrowsingAllowlistWarning;
  SafetyCheckWarningReason safetyCheckWarningReason;
  required boolean showAccessRequestsInToolbar;
  boolean pinnedToToolbar;
  required boolean isAffectedByMV2Deprecation;
  required boolean didAcknowledgeMV2DeprecationNotice;
  required boolean canUploadAsAccountExtension;
};

dictionary ProfileInfo {
  required boolean canLoadUnpacked;
  required boolean inDeveloperMode;
  required boolean isDeveloperModeControlledByPolicy;
  required boolean isIncognitoAvailable;
  required boolean isChildAccount;
  required boolean isMv2DeprecationNoticeDismissed;
};

dictionary GetExtensionsInfoOptions {
  boolean includeDisabled;
  boolean includeTerminated;
};

dictionary ExtensionConfigurationUpdate {
  required DOMString extensionId;
  boolean fileAccess;
  boolean incognitoAccess;
  boolean userScriptsAccess;
  boolean errorCollection;
  HostAccess hostAccess;
  boolean showAccessRequestsInToolbar;
  SafetyCheckWarningReason acknowledgeSafetyCheckWarningReason;
  boolean acknowledgeSafetyCheckWarning;
  boolean pinnedToToolbar;
};

dictionary ProfileConfigurationUpdate {
  boolean inDeveloperMode;
  boolean isMv2DeprecationNoticeDismissed;
};

dictionary ExtensionCommandUpdate {
  required DOMString extensionId;
  required DOMString commandName;
  CommandScope scope;
  DOMString keybinding;
};

dictionary ReloadOptions {
  // If false, an alert dialog will show in the event of a reload error.
  // Defaults to false.
  boolean failQuietly;

  // If true, populates a LoadError for the response rather than setting
  // lastError. Only relevant for unpacked extensions; it will be ignored for
  // any other extension.
  boolean populateErrorForUnpacked;
};

dictionary LoadUnpackedOptions {
  // If false, an alert dialog will show in the event of a reload error.
  // Defaults to false.
  boolean failQuietly;

  // If true, populates a LoadError for the response rather than setting
  // lastError.
  boolean populateError;

  // A unique identifier for retrying a previous failed load. This should be
  // the identifier returned in the LoadError. If specified, the path
  // associated with the identifier will be loaded, and the file chooser
  // will be skipped.
  DOMString retryGuid;

  // True if the function should try to load an extension from the drop data
  // of the page. notifyDragInstallInProgress() needs to be called prior to
  // this being used. This cannot be used with |retryGuid|.
  boolean useDraggedPath;
};

// Describes which set of sites a given url/string is associated with. Note
// that a site can belong to multiple sets at the same time.
enum SiteSet {
  // The site is specified by the user to automatically grant access to all
  // extensions with matching host permissions. Mutually exclusive with
  // USER_RESTRICTED but takes precedence over EXTENSION_SPECIFIED.
  "USER_PERMITTED",
  // The site is specified by the user to disallow all extensions from running
  // on it. Mutually exclusive with USER_PERMITTED but takes precedence over
  // EXTENSION_SPECIFIED.
  "USER_RESTRICTED",
  // The site is specified by one or more extensions' set of host permissions.
  "EXTENSION_SPECIFIED"
};

dictionary UserSiteSettingsOptions {
  // Specifies which set of user specified sites that the host will be added
  // to or removed from.
  required SiteSet siteSet;
  // The sites to add/remove.
  required sequence<DOMString> hosts;
};

dictionary UserSiteSettings {
  // The list of origins where the user has allowed all extensions to run on.
  required sequence<DOMString> permittedSites;
  // The list of origins where the user has blocked all extensions from
  // running on.
  required sequence<DOMString> restrictedSites;
};

dictionary SiteInfo {
  // The site set that `site` belongs to.
  required SiteSet siteSet;
  // The number of extensions with access to `site`.
  // TODO(crbug.com/40227416): A tricky edge case is when one extension
  // specifies something like *.foo.com and another specifies foo.com.
  // Patterns which match all subdomains should be represented differently.
  required long numExtensions;
  // The site itself. This could either be a user specified site or an
  // extension host permission pattern.
  required DOMString site;
};

dictionary SiteGroup {
  // The common effective top level domain plus one (eTLD+1) for all sites in
  // `sites`.
  required DOMString etldPlusOne;
  // The number of extensions that can run on at least one site inside `sites`
  // for this eTLD+1.
  required long numExtensions;
  // The list of user or extension specified sites that share the same eTLD+1.
  required sequence<SiteInfo> sites;
};

dictionary MatchingExtensionInfo {
  // The id of the matching extension.
  required DOMString id;
  // Describes the extension's access to the queried site from
  // getMatchingExtensionsForSite. Note that the meaning is different from the
  // original enum:
  // - ON_CLICK: The extension requested access to the site but its access is
  //   withheld.
  // - ON_SPECIFIC_SITES: the extension is permitted to run on at least one
  //   site specified by the queried site but it does not request access to
  //   all sites or it has its access withheld on at least one site in its
  //   host permissions.
  // - ON_ALL_SITES: the extension is permitted to run on all sites.
  required HostAccess siteAccess;
  // Whether the matching extension requests access to all sites in its
  // host permissions.
  required boolean canRequestAllSites;
};

dictionary ExtensionSiteAccessUpdate {
  // The id of the extension to update its site access settings for.
  required DOMString id;
  // Describes the update made to the extension's site access for a given site
  // Note that this has a different meaning from the original enum:
  // - ON_CLICK: Withholds the extension's access to the given site,
  // - ON_SPECIFIC_SITES: Grants the extension access to the intersection of
  //   (given site, extension's specified host permissions.)
  // - ON_ALL_SITES: Grants access to all of the extension's specified host
  //   permissions.
  required HostAccess siteAccess;
};

enum PackStatus {
  "SUCCESS",
  "ERROR",
  "WARNING"
};

enum FileType {
  "LOAD",
  "PEM"
};

enum SelectType {
  "FILE",
  "FOLDER"
};

enum EventType {
  "INSTALLED",
  "UNINSTALLED",
  "LOADED",
  "UNLOADED",
  // New window / view opened.
  "VIEW_REGISTERED",
  // window / view closed.
  "VIEW_UNREGISTERED",
  "ERROR_ADDED",
  "ERRORS_REMOVED",
  "PREFS_CHANGED",
  "WARNINGS_CHANGED",
  "COMMAND_ADDED",
  "COMMAND_REMOVED",
  "PERMISSIONS_CHANGED",
  "SERVICE_WORKER_STARTED",
  "SERVICE_WORKER_STOPPED",
  "CONFIGURATION_CHANGED",
  "PINNED_ACTIONS_CHANGED"
};

dictionary PackDirectoryResponse {
  // The response message of success or error.
  required DOMString message;

  // Unpacked items's path.
  required DOMString item_path;

  // Permanent key path.
  required DOMString pem_path;

  required long override_flags;
  required PackStatus status;
};

dictionary ProjectInfo {
  required DOMString name;
};

dictionary EventData {
  required EventType event_type;
  required DOMString item_id;
  ExtensionInfo extensionInfo;
};

dictionary ErrorFileSource {
  // The region before the "highlight" portion.
  // If the region which threw the error was not found, the full contents of
  // the file will be in the "beforeHighlight" section.
  required DOMString beforeHighlight;

  // The region of the code which threw the error, and should be highlighted.
  required DOMString highlight;

  // The region after the "highlight" portion.
  required DOMString afterHighlight;
};

dictionary LoadError {
  // The error that occurred when trying to load the extension.
  required DOMString error;

  // The path to the extension.
  required DOMString path;

  // The file source for the error, if it could be retrieved.
  ErrorFileSource source;

  // A unique identifier to pass to developerPrivate.loadUnpacked to retry
  // loading the extension at the same path.
  required DOMString retryGuid;
};

dictionary RequestFileSourceProperties {
  // The ID of the extension owning the file.
  required DOMString extensionId;

  // The path of the file, relative to the extension; e.g., manifest.json,
  // script.js, or main.html.
  required DOMString pathSuffix;

  // The error message which was thrown as a result of the error in the file.
  required DOMString message;

  // The key in the manifest which caused the error (e.g., "permissions").
  // (Required for "manifest.json" files)
  DOMString manifestKey;

  // The specific portion of the manifest key which caused the error (e.g.,
  // "foo" in the "permissions" key). (Optional for "manifest.json" file).
  DOMString manifestSpecific;

  // The line number which caused the error (optional for non-manifest files).
  long lineNumber;
};

dictionary RequestFileSourceResponse {
  // The source code related to the request. Only populated if the file
  // was successfully read.
  ErrorFileSource source;

  // A title for the file in the form '<extension name>: <file name>'.
  required DOMString title;

  // The error message.
  required DOMString message;
};

dictionary OpenDevToolsProperties {
  // The ID of the extension. This is only needed if opening its background
  // page or its background service worker (where renderViewId and
  // renderProcessId are -1).
  DOMString extensionId;

  // The ID of the render frame in which the error occurred.
  // Despite being called renderViewId, this refers to a render frame.
  required long renderViewId;

  // The ID of the process in which the error occurred.
  required long renderProcessId;

  // Whether or not the background is service worker based.
  boolean isServiceWorker;

  boolean incognito;

  // The URL in which the error occurred.
  DOMString url;

  // The line to focus the devtools at.
  long lineNumber;

  // The column to focus the devtools at.
  long columnNumber;
};

dictionary DeleteExtensionErrorsProperties {
  required DOMString extensionId;
  sequence<long> errorIds;
  ErrorType type;
};

// Listener callback for the onItemStateChanged event.
callback OnItemStateChangedListener = undefined (EventData response);

interface OnItemStateChangedEvent : ExtensionEvent {
  static undefined addListener(OnItemStateChangedListener listener);
  static undefined removeListener(OnItemStateChangedListener listener);
  static boolean hasListener(OnItemStateChangedListener listener);
};

// Listener callback for the onProfileStateChanged event.
callback OnProfileStateChangedListener = undefined (ProfileInfo info);

interface OnProfileStateChangedEvent : ExtensionEvent {
  static undefined addListener(OnProfileStateChangedListener listener);
  static undefined removeListener(OnProfileStateChangedListener listener);
  static boolean hasListener(OnProfileStateChangedListener listener);
};

// Listener callback for the onUserSiteSettingChanged event.
callback OnUserSiteSettingsChangedListener =
    undefined (UserSiteSettings settings);

interface OnUserSiteSettingsChangedEvent : ExtensionEvent {
  static undefined addListener(OnUserSiteSettingsChangedListener listener);
  static undefined removeListener(OnUserSiteSettingsChangedListener listener);
  static boolean hasListener(OnUserSiteSettingsChangedListener listener);
};

[instanceOf=DirectoryEntry]
typedef object DirectoryEntry;

// developerPrivate API.
// This is a private API exposing developing and debugging functionalities for
// apps and extensions.
[implemented_in = "chrome/browser/extensions/api/developer_private/developer_private_functions.h"]
interface DeveloperPrivate {
  // Runs auto update for extensions and apps immediately.
  // |Returns|: Called after update check completes.
  static Promise<undefined> autoUpdate();

  // Returns information of all the extensions and apps installed.
  // |options|: Options to restrict the items returned.
  // |Returns|: Called with extensions info.
  // |PromiseValue|: result
  static Promise<sequence<ExtensionInfo>> getExtensionsInfo(
      optional GetExtensionsInfoOptions options);

  // Returns information of a particular extension.
  // |id|: The id of the extension.
  // |Returns|: Called with the result.
  // |PromiseValue|: result
  static Promise<ExtensionInfo> getExtensionInfo(DOMString id);

  // Returns the size of a particular extension on disk (already formatted).
  // |id|: The id of the extension.
  // |Returns|: Called with the result.
  // |PromiseValue|: string
  static Promise<DOMString> getExtensionSize(DOMString id);

  // Returns the current profile's configuration.
  // |PromiseValue|: info
  static Promise<ProfileInfo> getProfileConfiguration();

  // Updates the active profile.
  // |update|: The parameters for updating the profile's configuration.  Any
  //     properties omitted from |update| will not be changed.
  static Promise<undefined> updateProfileConfiguration(
      ProfileConfigurationUpdate update);

  // Reloads a given extension.
  // |extensionId|: The id of the extension to reload.
  // |options|: Additional configuration parameters.
  // |PromiseValue|: error
  static Promise<LoadError?> reload(
      DOMString extensionId,
      optional ReloadOptions options);

  // Modifies an extension's current configuration.
  // |update|: The parameters for updating the extension's configuration.
  //     Any properties omitted from |update| will not be changed.
  static Promise<undefined> updateExtensionConfiguration(
      ExtensionConfigurationUpdate update);

  // Loads a user-selected unpacked item.
  // |options|: Additional configuration parameters.
  // |PromiseValue|: error
  static Promise<LoadError?> loadUnpacked(optional LoadUnpackedOptions options);

  // Installs the file that was dragged and dropped onto the associated
  // page.
  static Promise<undefined> installDroppedFile();

  // Notifies the browser that a user began a drag in order to install an
  // extension.
  static undefined notifyDragInstallInProgress();

  // Loads an extension / app.
  // |directory|: The directory to load the extension from.
  // |PromiseValue|: string
  static Promise<DOMString> loadDirectory(DirectoryEntry directory);

  // Open Dialog to browse to an entry.
  // |selectType|: Select a file or a folder.
  // |fileType|: Required file type. For example, pem type is for private
  // key and load type is for an unpacked item.
  // |Returns|: called with selected item's path.
  // |PromiseValue|: string
  static Promise<DOMString> choosePath(
      SelectType selectType,
      FileType fileType);

  // Pack an extension.
  // |rootPath|: The path of the extension.
  // |privateKeyPath|: The path of the private key, if one is given.
  // |flags|: Special flags to apply to the loading process, if any.
  // |Returns|: called with the success result string.
  // |PromiseValue|: response
  static Promise<PackDirectoryResponse> packDirectory(
      DOMString path,
      optional DOMString privateKeyPath,
      optional long flags);

  // Reads and returns the contents of a file related to an extension which
  // caused an error.
  // |PromiseValue|: response
  static Promise<RequestFileSourceResponse> requestFileSource(
      RequestFileSourceProperties properties);

  // Open the developer tools to focus on a particular error.
  static Promise<undefined> openDevTools(OpenDevToolsProperties properties);

  // Delete reported extension errors.
  // |properties|: The properties specifying the errors to remove.
  static Promise<undefined> deleteExtensionErrors(
      DeleteExtensionErrorsProperties properties);

  // Repairs the extension specified.
  // |extensionId|: The id of the extension to repair.
  static Promise<undefined> repairExtension(DOMString extensionId);

  // Shows the options page for the extension specified.
  // |extensionId|: The id of the extension to show the options page for.
  static Promise<undefined> showOptions(DOMString extensionId);

  // Shows the path of the extension specified.
  // |extensionId|: The id of the extension to show the path for.
  static Promise<undefined> showPath(DOMString extensionId);

  // (Un)suspends global shortcut handling.
  // |isSuspended|: Whether or not shortcut handling should be suspended.
  static Promise<undefined> setShortcutHandlingSuspended(boolean isSuspended);

  // Updates an extension command.
  // |update|: The parameters for updating the extension command.
  static Promise<undefined> updateExtensionCommand(
      ExtensionCommandUpdate update);

  // Adds a new host permission to the extension. The extension will only
  // have access to the host if it is within the requested permissions.
  // |extensionId|: The id of the extension to modify.
  // |host|: The host to add.
  static Promise<undefined> addHostPermission(
      DOMString extensionId,
      DOMString host);

  // Removes a host permission from the extension. This should only be called
  // with a host that the extension has access to.
  // |extensionId|: The id of the extension to modify.
  // |host|: The host to remove.
  static Promise<undefined> removeHostPermission(
      DOMString extensionId,
      DOMString host);

  // Returns the user specified site settings (which origins can extensions
  // always/never run on) for the current profile.
  // |PromiseValue|: settings
  static Promise<UserSiteSettings> getUserSiteSettings();

  // Adds hosts to the set of user permitted or restricted sites. If any hosts
  // are in the other set than what's specified in `options`, then they are
  // removed from that set.
  static Promise<undefined> addUserSpecifiedSites(
      UserSiteSettingsOptions options);

  // Removes hosts from the specified set of user permitted or restricted
  // sites.
  static Promise<undefined> removeUserSpecifiedSites(
      UserSiteSettingsOptions options);

  // Returns all hosts specified by user site settings, grouped by each host's
  // eTLD+1.
  // |PromiseValue|: siteGroups
  static Promise<sequence<SiteGroup>> getUserAndExtensionSitesByEtld();

  // Returns a list of extensions which have at least one matching site in
  // common between its set of host permissions and `site`.
  // |PromiseValue|: matchingExtensions
  static Promise<sequence<MatchingExtensionInfo>> getMatchingExtensionsForSite(
      DOMString site);

  // Updates the site access settings for multiple extensions for the given
  // `site` and calls `callback` once all updates have been finished.
  // Each update species an extension id an a new HostAccess setting.
  static Promise<undefined> updateSiteAccess(
      DOMString site,
      sequence<ExtensionSiteAccessUpdate> updates);

  // Removes multiple installed extensions.
  static Promise<undefined> removeMultipleExtensions(
      sequence<DOMString> extensionIds);

  // Dismisses the menu notification for the extensions module in Safety Hub
  // if one is active.
  static undefined dismissSafetyHubExtensionsMenuNotification();

  // Triggers the dismissal of the mv2 deprecation notice for `extensionId`.
  static undefined dismissMv2DeprecationNoticeForExtension(
      DOMString extensionId);

  // Uploads an extension to the signed in user's account and returns whether
  // the extension is actually uploaded in `callback`. If the extension is not
  // eligible for upload or if there is no signed in user, returns an error.
  // |PromiseValue|: result
  static Promise<boolean> uploadExtensionToAccount(DOMString extensionId);

  // Shows the site settings for the extension.
  [platforms=("desktop_android")]
  static undefined showSiteSettings(DOMString extensionId);

  [nocompile, deprecated="Use openDevTools"]
  static Promise<undefined> inspect(InspectOptions options);

  // Fired when a item state is changed.
  static attribute OnItemStateChangedEvent onItemStateChanged;

  // Fired when the profile's state has changed.
  static attribute OnProfileStateChangedEvent onProfileStateChanged;

  // Fired when the lists of sites in the user's site settings have changed.
  static attribute OnUserSiteSettingsChangedEvent onUserSiteSettingsChanged;
};

partial interface Browser {
  static attribute DeveloperPrivate developerPrivate;
};
