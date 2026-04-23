// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// API for integration testing. To be used on test images with a test component
// extension.

enum ShelfAlignmentType {
  // BottomLocked not supported by shelf_prefs.
  "Bottom",
  "Left",
  "Right"
};

// A mapping of ash::ShelfItemType.
enum ShelfItemType {
  "PinnedApp",
  "BrowserShortcut",
  "App",
  "UnpinnedBrowserShortcut",
  "Dialog"
};

// A mapping of ash::ShelfItemStatus.
enum ShelfItemStatus {
  "Closed",
  "Running",
  "Attention"
};

// A mapping of apps::mojom::Type
enum AppType {
  "Arc",
  "Crostini",
  "Extension",
  "Web",
  "MacOS",
  "PluginVm",
  "StandaloneBrowser",
  "Remote",
  "Borealis",
  "Bruschetta"
};

// A mapping of apps::mojom::InstallSource
enum AppInstallSource {
  "Unknown",
  "System",
  "Policy",
  "Oem",
  "Default",
  "Sync",
  "User",
  "SubApp",
  "Kiosk",
  "CommandLine"
};

// A mapping of apps::mojom::Readiness
enum AppReadiness {
  "Ready",
  "DisabledByBlacklist",
  "DisabledByPolicy",
  "DisabledByUser",
  "Terminated",
  "UninstalledByUser",
  "Removed",
  "UninstalledByMigration",
  "DisabledByLocalSettings"
};

// A mapping of arc::mojom::WakefulnessMode
enum WakefulnessMode {
  "Unknown",
  "Asleep",
  "Awake",
  "Dreaming",
  "Dozing"
};

// A subset of Window State types in ash::WindowStateType. We may add more
// into the set in the future.
enum WindowStateType {
  "Normal",
  "Minimized",
  "Maximized",
  "Fullscreen",
  "PrimarySnapped",
  "SecondarySnapped",
  "Pinned",
  "TrustedPinned",
  "PIP",
  "Floated"
};

// A subset of WM event types in ash::WMEventType. We may add more in the
// set in the future.
enum WMEventType {
  "WMEventNormal",
  "WMEventMaximize",
  "WMEventMinimize",
  "WMEventFullscreen",
  "WMEventSnapPrimary",
  "WMEventSnapSecondary",
  "WMEventFloat"
};

// Display orientation type.
enum RotationType {
  // RotateAny is the auto-rotation status (not locked to a rotation) for
  // tablet mode. Not working in clamshell mode.
  "RotateAny",
  "Rotate0",
  "Rotate90",
  "Rotate180",
  "Rotate270"
};

enum LauncherStateType {
  "Closed",
  "FullscreenAllApps",
  "FullscreenSearch"
};

enum OverviewStateType {
  "Shown",
  "Hidden"
};

enum MouseButton {
  "Left",
  "Middle",
  "Right",
  "Back",
  "Forward"
};

// A paramter used in setArcAppWindowState() function.
dictionary WindowStateChangeDict {
  // The WM event to change the ARC window state.
  required WMEventType eventType;

  // If the initial state is already same as the expected state, should we
  // treat this case as a failure? Default value is false.
  boolean failIfNoChange;
};

dictionary LoginStatusDict {
  // Are we logged in?
  required boolean isLoggedIn;
  // Is the logged-in user the owner?
  required boolean isOwner;
  // Is the screen locked?
  required boolean isScreenLocked;
  // Is the wallpaper blur layer still animating in?
  required boolean isLockscreenWallpaperAnimating;
  // Is the screen ready for password?
  required boolean isReadyForPassword;
  // Are the avatar images loaded for all users?
  required boolean areAllUserImagesLoaded;

  // Is the logged-in user a regular user? Set only if `isLoggedIn`.
  boolean isRegularUser;
  // Are we logged into the guest account? Set only if `isLoggedIn`.
  boolean isGuest;
  // Are we logged into kiosk-app mode? Set only if `isLoggedIn`.
  boolean isKiosk;

  // User email. Set only if `isLoggedIn`.
  DOMString email;
  // User display email. Set only if `isLoggedIn`.
  DOMString displayEmail;
  // User display name. Set only if `isLoggedIn`.
  DOMString displayName;
  // User image: 'file', 'profile' or a number. Set only if `isLoggedIn`.
  DOMString userImage;
  // Whether the user has a valid oauth2 token. Only set for gaia user.
  boolean hasValidOauth2Token;
};

dictionary ExtensionInfoDict {
  required DOMString id;
  required DOMString version;
  required DOMString name;
  required DOMString publicKey;
  required DOMString description;
  required DOMString backgroundUrl;
  required DOMString optionsUrl;

  required sequence<DOMString> hostPermissions;
  required sequence<DOMString> effectiveHostPermissions;
  required sequence<DOMString> apiPermissions;

  required boolean isComponent;
  required boolean isInternal;
  required boolean isUserInstalled;
  required boolean isEnabled;
  required boolean allowedInIncognito;
  required boolean hasPageAction;
};

dictionary ExtensionsInfoArray {
  required sequence<ExtensionInfoDict> extensions;
};

dictionary Notification {
  required DOMString id;
  required DOMString type;
  required DOMString title;
  required DOMString message;
  required long priority;
  required long progress;
};

dictionary Printer {
  required DOMString printerName;
  DOMString printerId;
  DOMString printerType;
  DOMString printerDesc;
  DOMString printerMakeAndModel;
  DOMString printerUri;
  DOMString printerPpd;
};

dictionary ArcState {
  // Whether the ARC is provisioned.
  required boolean provisioned;
  // Whether ARC Terms of Service needs to be shown.
  required boolean tosNeeded;
  // ARC pre-start time (mini-ARC) or 0 if not pre-started.
  required double preStartTime;
  // ARC start time or 0 if not started.
  required double startTime;
};

dictionary PlayStoreState {
  // Whether the Play Store allowed for the current user.
  required boolean allowed;
  // Whether the Play Store currently enabled.
  boolean enabled;
  // Whether the Play Store managed by policy.
  boolean managed;
};

dictionary AssistantQueryResponse {
  // Text response returned from server.
  DOMString text;
  // HTML response returned from server.
  DOMString htmlResponse;
  // Open URL response returned from server.
  DOMString openUrl;
  // Open Android app response returned from server.
  DOMString openAppResponse;
};

dictionary AssistantQueryStatus {
  // Indicates whether this might be a voice interaction.
  required boolean isMicOpen;
  // Query text sent to Assistant. In the event of a voice interaction,
  // this field will be same as the speech recognition final result.
  required DOMString queryText;
  // Response for the current query.
  required AssistantQueryResponse queryResponse;
};

dictionary ArcAppDict {
  required DOMString name;
  required DOMString packageName;
  required DOMString activity;
  required DOMString intentUri;
  required DOMString iconResourceId;
  required double lastLaunchTime;
  required double installTime;
  required boolean sticky;
  required boolean notificationsEnabled;
  required boolean ready;
  required boolean suspended;
  required boolean showInLauncher;
  required boolean shortcut;
  required boolean launchable;
};

dictionary ArcAppKillsDict {
  required double oom;
  required double lmkdForeground;
  required double lmkdPerceptible;
  required double lmkdCached;
  required double pressureForeground;
  required double pressurePerceptible;
  required double pressureCached;
};

dictionary ArcPackageDict {
  required DOMString packageName;
  required long packageVersion;
  required DOMString lastBackupAndroidId;
  required double lastBackupTime;
  required boolean shouldSync;
  required boolean vpnProvider;
};

dictionary Location {
  required double x;
  required double y;
};

dictionary Bounds {
  required double left;
  required double top;
  required double width;
  required double height;
};

dictionary ArcAppTracingInfo {
  required boolean success;
  required double fps;
  required double perceivedFps;
  required double commitDeviation;
  required double presentDeviation;
  required double renderQuality;
  required double janksPerMinute;
  required double janksPercentage;
};

enum ThemeStyle {
  "TonalSpot",
  "Vibrant",
  "Expressive",
  "Spritz",
  "Rainbow",
  "FruitSalad"
};

dictionary App {
  required DOMString appId;
  required DOMString name;
  required DOMString shortName;
  required DOMString publisherId;
  AppType type;
  AppInstallSource installSource;
  AppReadiness readiness;
  required sequence<DOMString> additionalSearchTerms;
  boolean showInLauncher;
  boolean showInSearch;
};

dictionary SystemWebApp {
  // App's internal name. This isn't user-visible and should only be used
  // for logging.
  required DOMString internalName;

  // App's install URL. This is a placeholder for installation pipeline,
  // not used for anything else.
  required DOMString url;

  // App's visible name. This is defined in the Web App manifest, and shown
  // in Shelf and Launcher. This matches App's name attribute (see above).
  required DOMString name;

  // App's default start_url. This is the default URL that the App will be
  // launched to.
  required DOMString startUrl;
};

dictionary ShelfItem {
  required DOMString appId;
  required DOMString launchId;
  required DOMString title;
  ShelfItemType type;
  required ShelfItemStatus status;
  required boolean showsTooltip;
  required boolean pinnedByPolicy;
  required boolean pinStateForcedByType;
  required boolean hasNotification;
};

// A mapping of ash::AppType.
enum AppWindowType {
  "Browser",
  "ChromeApp",
  "ArcApp",
  "CrostiniApp",
  "SystemApp",
  "ExtensionApp",
  "Lacros"
};

// A mapping of HotseatState in ash/public/cpp/shelf_types.h.
enum HotseatState {
  "Hidden",
  "ShownClamShell",
  "ShownHomeLauncher",
  "Extended"
};

// The frame mode of a window. None if the window is framesless.
enum FrameMode {
  "Normal",
  "Immersive"
};

dictionary OverviewInfo {
  // Bounds in screen of an OverviewItem.
  required Bounds bounds;
  // Whether an OverviewItem is being dragged in overview.
  required boolean isDragged;
};

// Used to update an app's shelf pin state.
dictionary ShelfIconPinUpdateParam {
  // The identifier of the target app.
  required DOMString appId;

  // The target pin state for the app.
  required boolean pinned;
};

dictionary AppWindowInfo {
  // The identifier of the window. This shouldn't change across the time.
  required long id;

  // The name of the window object -- typically internal class name of the
  // window (like 'BrowserWidget').
  required DOMString name;

  required AppWindowType windowType;
  required WindowStateType stateType;

  // The bounds of the window, in the coordinate of the root window (i.e.
  // relative to the display where this window resides).
  required Bounds boundsInRoot;

  // The identifier of the display where this window resides.
  required DOMString displayId;

  required boolean isVisible;
  required boolean canFocus;

  // The title of the window; this can be seen in the window caption, or in
  // the overview mode. Typically, this provides the title of the webpage or
  // the title supplied by the application.
  required DOMString title;

  // Whether some animation is ongoing on the window or not.
  required boolean isAnimating;

  // The final bounds of the window when the animation completes. This should
  // be same as |boundsInRoot| when |isAnimating| is false.
  required Bounds targetBounds;

  // Whether or not the window is going to be visible after the animation
  // completes. This should be same as |isVisible| when |isAnimating| is
  // false.
  required boolean targetVisibility;

  // WM Releated states
  required boolean isActive;
  required boolean hasFocus;
  required boolean onActiveDesk;
  required boolean hasCapture;
  required boolean canResize;

  // Stacking order of the window in relation to its siblings. 0 indicates
  // that the window is topmost. -1 if stacking info is not available
  required long stackingOrder;

  // Window frame info
  required FrameMode frameMode;
  required boolean isFrameVisible;
  required long captionHeight;
  // The bitset of the enabled caption buttons. See
  // ui/views/window/caption_button_types.h.
  required long captionButtonEnabledStatus;
  // The bitset of the caption buttons which are visible on the frame.
  required long captionButtonVisibleStatus;

  DOMString arcPackageName;

  OverviewInfo overviewInfo;

  // The identifier of the app associated with the window that was launched
  // from full restore. This should be same as |appId| when the window was
  // restored from full restore, otherwise null.
  DOMString fullRestoreWindowAppId;

  // The identifier of the app associated with the window.
  DOMString appId;
};

dictionary Accelerator {
  required DOMString keyCode;
  required boolean shift;
  required boolean control;
  required boolean alt;
  required boolean search;
  required boolean pressed;
};

// Mapped to ScrollableShelfState in ash/public/cpp/shelf_ui_info.h.
// [deprecated="Use ShelfState"]
dictionary ScrollableShelfState {
  double scrollDistance;
};

// Mapped to ShelfState in ash/public/cpp/shelf_ui_info.h.
dictionary ShelfState {
  double scrollDistance;
};

// Mapped to ScrollableShelfInfo in ash/public/cpp/shelf_ui_info.h.
// |targetMainAxisOffset| is set when ShelfState used in query
// specifies the scroll distance.
dictionary ScrollableShelfInfo {
  required double mainAxisOffset;
  required double pageOffset;
  double targetMainAxisOffset;
  required Bounds leftArrowBounds;
  required Bounds rightArrowBounds;
  required boolean isAnimating;
  required boolean iconsUnderAnimation;
  required boolean isOverflow;
  required sequence<Bounds> iconsBoundsInScreen;
  required boolean isShelfWidgetAnimating;
};

// Mapped to HotseatSwipeDescriptor in ash/public/cpp/shelf_ui_info.h.
dictionary HotseatSwipeDescriptor {
  required Location swipeStartLocation;
  required Location swipeEndLocation;
};

// Mapped to HotseatInfo in ash/public/cpp/shelf_ui_info.h.
dictionary HotseatInfo {
  required HotseatSwipeDescriptor swipeUp;
  required HotseatState state;
  required boolean isAnimating;
  // Whether the shelf is hidden with auto-hide enabled.
  required boolean isAutoHidden;
};

// The ui information of shelf components, including hotseat and
// scrollable shelf.
dictionary ShelfUIInfo {
  required HotseatInfo hotseatInfo;
  required ScrollableShelfInfo scrollableShelfInfo;
};

// Information about all desks.
dictionary DesksInfo {
  required long activeDeskIndex;
  required long numDesks;
  required boolean isAnimating;
  required sequence<DOMString> deskContainers;
};

// Information about launcher's search box.
dictionary LauncherSearchBoxState {
  required DOMString ghostText;
};

// Frame counting record for one frame sink/compositor.
dictionary FrameCountingPerSinkData {
  // Type of the frame sink. This corresponds to CompositorFrameSinkType.
  required DOMString sinkType;
  // Whether the frame sink is the root.
  required boolean isRoot;
  // Debug label of the frame sink.
  required DOMString debugLabel;

  // Number of presented frames grouped using `bucketSizeInSeconds` arg in
  // startFrameCounting call. It would be fps if the `bucketSizeInSeconds` is
  // 1s.
  required sequence<long> presentedFrames;
};

dictionary OverdrawData {
  // Average overdraw as percentage of the display size grouped by
  // `bucketSizeInSeconds` arg of `startOverdrawTracking` call.
  required sequence<double> averageOverdraws;
};

// Result of calling setWindowBounds, which returns the actual bounds and
// display the window was set to. This may be different than the requested
// bounds and display, for example if the window is showing an ARC app and
// Android modifies the bounds request. Further, this result may never be
// returned in some situations (e.g. Android ignoring a bounds request),
// causing a timeout.
dictionary SetWindowBoundsResult {
  // Bounds of the window.
  required Bounds bounds;
  // Display ID of the display the window is on.
  required DOMString displayId;
};

// Collected DisplaySmoothness data between startSmoothnessTracking and
// stopSmoothnessTracking calls.
dictionary DisplaySmoothnessData {
  // Number of frames expected to be shown for this animation.
  required long framesExpected;
  // Number of frames actually shown for this animation.
  required long framesProduced;
  // Number of janks during this animation. A jank is counted when the current
  // frame latency is larger than previous ones.
  required long jankCount;
  // Display throughput percentage at fixed intervals.
  required sequence<long> throughput;
  // The timestamps of the janks during this animation in milllisecond.
  required sequence<long> jankTimestamps;
  // The durations of the janks during this animation in millisecond.
  required sequence<long> jankDurations;
};

// Collected ui::ThroughputTracker data for one animation. It is based on
// cc::FrameSequenceMetrics::ThroughputData.
dictionary ThroughputTrackerAnimationData {
  // Animation start time in milliseconds, relative to when
  // `startThroughputTrackerDataCollection` is called.
  required long startOffsetMs;
  // Animation stop time in milliseconds, relative to when
  // `startThroughputTrackerDataCollection` is called.
  required long stopOffsetMs;
  // Number of frames expected to be shown for this animation.
  required long framesExpected;
  // Number of frames actually shown for this animation.
  required long framesProduced;
  // Number of janks during this animation. A jank is counted when the current
  // frame latency is larger than previous ones.
  required long jankCount;
};

// Options for resetting the holding space.
dictionary ResetHoldingSpaceOptions {
  // Whether to call `ash::holding_space_prefs::MarkTimeOfFirstAdd()` after
  // reset. Used to show the holding space tray in tests, since it's otherwise
  // hidden after OOBE.
  required boolean markTimeOfFirstAdd;
};

// Collected ash::LoginEventRecorder data.
dictionary LoginEventRecorderData {
  // Event name
  required DOMString name;
  // Number of frames actually shown for this animation.
  required double microsecnods_since_unix_epoch;
};

// Request parameters for <code>getAccessToken</code>.
dictionary GetAccessTokenParams {
   // An email associated with the account to get a token for.
   required DOMString email;
   // A list of OAuth scopes to request.
   required sequence<DOMString> scopes;
   // An optional timeout in milliseconds for the request.
   // Default: 90 seconds
   long timeoutMs;
};

// Response data for <code>getAccessToken</code>.
dictionary GetAccessTokenData {
  // The access token
  required DOMString accessToken;
  // The time the access token will expire as a unix timestamp in
  // milliseconds.
  required DOMString expirationTimeUnixMs;
};

// Response data for <code>makeFuseboxTempDir</code>.
dictionary MakeFuseboxTempDirData {
  required DOMString fuseboxFilePath;
  required DOMString underlyingFilePath;
};

// Response data for <code>getCurrentInputMethodDescriptor</code>.
// Add more fields from ash/input_method/InputMethodDescriptor as needed.
dictionary GetCurrentInputMethodDescriptorData {
  required DOMString keyboardLayout;
};

// Request data containing the mock responses from
// overrideOrcaResponseForTesting.
dictionary OrcaResponseArray {
  required sequence<DOMString> responses;
};

// Request data containing the mock responses from
// overrideScannerResponsesForTesting.
dictionary ScannerResponseArray {
  required sequence<DOMString> responses;
};

callback OnClipboardDataChangedListener = undefined ();

interface OnClipboardDataChangedEvent : ExtensionEvent {
  static undefined addListener(OnClipboardDataChangedListener listener);
  static undefined removeListener(OnClipboardDataChangedListener listener);
  static boolean hasListener(OnClipboardDataChangedListener listener);
};

// API for integration testing. To be used on test images with a test component
// extension.
[platforms=("chromeos"),
 implemented_in="chrome/browser/ash/extensions/autotest_private/autotest_private_api.h"]
interface AutotestPrivate {
  // Must be called to allow autotestPrivateAPI events to be fired.
  static undefined initializeEvents();

  // Logout of a user session.
  static undefined logout();

  // Restart the browser.
  static undefined restart();

  // Shutdown the browser.
  // |force|: if set, ignore ongoing downloads and onunbeforeunload handlers.
  static undefined shutdown(boolean force);

  // Get login status.
  // |PromiseValue|: status
  [requiredCallback] static Promise<LoginStatusDict> loginStatus();

  // Waits for the post login animation to be complete and then triggers the
  // callback.
  [requiredCallback] static Promise<undefined> waitForLoginAnimationEnd();

  // Locks the screen.
  static undefined lockScreen();

  // Get info about installed extensions.
  // |PromiseValue|: info
  [requiredCallback] static Promise<ExtensionsInfoArray> getExtensionsInfo();

  // Get state of the policies.
  // Will contain device policies and policies from the active profile.
  // The policy values are formatted as they would be for exporting in
  // chrome://policy.
  // |Returns|: `allPolicies` will be the full list of policies as returned by
  // the DictionaryPolicyConversions.ToValue function.
  // |PromiseValue|: allPolicies
  [requiredCallback] static Promise<any> getAllEnterprisePolicies();

  // Refreshes the Enterprise Policies.
  [requiredCallback] static Promise<undefined> refreshEnterprisePolicies();

  // Refreshes the remote commands.
  [requiredCallback] static Promise<undefined> refreshRemoteCommands();

  // Simulates a memory access bug for asan testing.
  static undefined simulateAsanMemoryBug();

  // Set the touchpad pointer sensitivity setting.
  // |value|: the pointer sensitivity setting index.
  static undefined setTouchpadSensitivity(long value);

  // Turn on/off tap-to-click for the touchpad.
  // |enabled|: if set, enable tap-to-click.
  static undefined setTapToClick(boolean enabled);

  // Turn on/off three finger click for the touchpad.
  // |enabled|: if set, enable three finger click.
  static undefined setThreeFingerClick(boolean enabled);

  // Turn on/off tap dragging for the touchpad.
  // |enabled|: if set, enable tap dragging.
  static undefined setTapDragging(boolean enabled);

  // Turn on/off Australian scrolling for devices other than wheel mouse.
  // |enabled|: if set, enable Australian scrolling.
  static undefined setNaturalScroll(boolean enabled);

  // Set the mouse pointer sensitivity setting.
  // |value|: the pointer sensitivity setting index.
  static undefined setMouseSensitivity(long value);

  // Swap the primary mouse button for left click.
  // |right|: if set, swap the primary mouse button.
  static undefined setPrimaryButtonRight(boolean right);

  // Turn on/off reverse scrolling for mice.
  // |enabled|: if set, enable reverse scrolling.
  static undefined setMouseReverseScroll(boolean enabled);

  // Get visible notifications on the system.
  // |PromiseValue|: notifications
  [requiredCallback]
  static Promise<sequence<Notification>> getVisibleNotifications();

  // Remove all notifications.
  [requiredCallback] static Promise<undefined> removeAllNotifications();

  // Get ARC start time in ticks.
  // |PromiseValue|: startTicks
  [requiredCallback] static Promise<double> getArcStartTime();

  // Get state of the ARC session.
  // |PromiseValue|: result
  [requiredCallback] static Promise<ArcState> getArcState();

  // Get state of the Play Store.
  // |PromiseValue|: result
  [requiredCallback] static Promise<PlayStoreState> getPlayStoreState();

  // Get list of available printers
  // |PromiseValue|: printers
  [requiredCallback] static Promise<sequence<Printer>> getPrinterList();

  // Returns true if requested app is shown in Chrome.
  // |PromiseValue|: appShown
  [requiredCallback] static Promise<boolean> isAppShown(DOMString appId);

  // Returns true if ARC is provisioned. [deprecated="Use getArcState()"]
  // |PromiseValue|: arcProvisioned
  [requiredCallback] static Promise<boolean> isArcProvisioned();

  // Gets information about the requested ARC app.
  // |PromiseValue|: package
  [requiredCallback] static Promise<ArcAppDict> getArcApp(DOMString appId);

  // Gets counts of how many ARC apps have been killed, by priority.
  // |PromiseValue|: counts
  [requiredCallback] static Promise<ArcAppKillsDict> getArcAppKills();

  // Gets information about requested ARC package.
  // |PromiseValue|: package
  [requiredCallback]
  static Promise<ArcPackageDict> getArcPackage(DOMString packageName);

  // Waits for system web apps to complete the installation.
  [requiredCallback] static Promise<undefined> waitForSystemWebAppsInstall();

  // Gets all the default pinned shelf app IDs, these may not be installed.
  // |PromiseValue|: items
  [requiredCallback]
  static Promise<sequence<DOMString>> getDefaultPinnedAppIds();

  // Returns the number of system web apps that should be installed.
  // |Returns|: Promise that resolves with the number of system web apps that
  // should be installed.
  // |PromiseValue|: systemWebApps
  [requiredCallback]
  static Promise<sequence<SystemWebApp>> getRegisteredSystemWebApps();

  // Returns whether the system web app is currently open or not.
  // |PromiseValue|: isOpen
  [requiredCallback]
  static Promise<boolean> isSystemWebAppOpen(DOMString appId);

  // Launches an application from the launcher with the given appId.
  [requiredCallback] static Promise<undefined> launchApp(DOMString appId);

  // Launches an system web app from the launcher with the given app name and
  // url.
  [requiredCallback]
  static Promise<undefined> launchSystemWebApp(DOMString appName,
                                               DOMString url);

  // Launches Files app directly to absolutePath, if the path does not exist, it
  // will launch to the default opening location (i.e. MyFiles). If the supplied
  // path is a file (and it exists) it will open Files app to the parent folder
  // instead.
  [requiredCallback]
  static Promise<undefined> launchFilesAppToPath(DOMString absolutePath);

  // Closes an application the given appId in case it was running.
  [requiredCallback] static Promise<undefined> closeApp(DOMString appId);

  // Update printer. Printer with empty ID is considered new.
  static undefined updatePrinter(Printer printer);

  // Remove printer.
  static undefined removePrinter(DOMString printerId);

  // Enable/disable the Play Store.
  // |enabled|: if set, enable the Play Store.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> setPlayStoreEnabled(boolean enabled);

  // Get text from ui::Clipboard.
  // |Returns|: Promise that resolves with the result.
  // |PromiseValue|: data
  [requiredCallback] static Promise<DOMString> getClipboardTextData();

  // Set text in ui::Clipbaord.
  // |Returns|: Promise that resolves when operation is complete.
  [requiredCallback]
  static Promise<undefined> setClipboardTextData(DOMString data);

  // Run the crostini installer GUI to install the default crostini
  // vm / container and create sshfs mount.  The installer launches the
  // crostini terminal app on completion.  The installer expects that
  // crostini is not already installed.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback] static Promise<undefined> runCrostiniInstaller();

  // Run the crostini uninstaller GUI to remove the default crostini
  // vm / container. The callback is invoked upon completion.
  [requiredCallback] static Promise<undefined> runCrostiniUninstaller();

  // Enable/disable Crostini in preferences.
  // |enabled|: Enable Crostini.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> setCrostiniEnabled(boolean enabled);

  // Export the crostini container.
  // |path|: The path in Downloads to save the export.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback] static Promise<undefined> exportCrostini(DOMString path);

  // Import the crostini container.
  // |path|: The path in Downloads to read the import.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback] static Promise<undefined> importCrostini(DOMString path);

  // Returns whether crostini could ever be allowed.
  // |Returns|: Promise that resolves with a boolean indicating if crostini can
  // ever be allowed in the current profile.
  // |PromiseValue|: canBeAllowed
  [requiredCallback] static Promise<boolean> couldAllowCrostini();

  // Sets mock Plugin VM policy.
  // |imageUrl|: URL to the image to install.
  // |imageHash|: Hash for the provided image.
  // |licenseKey|: License key for Plugin VM.
  static undefined setPluginVMPolicy(DOMString imageUrl,
                                     DOMString imageHash,
                                     DOMString licenseKey);

  // Shows the Plugin VM installer. Does not start installation.
  static undefined showPluginVMInstaller();

  // Installs Borealis without showing the normal installer UI.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback] static Promise<undefined> installBorealis();

  // Register a component with ComponentManagerAsh.
  // |name|: The name of the component.
  // |path|: Path to the component.
  static undefined registerComponent(DOMString name, DOMString path);

  // Takes a screenshot and returns the data in base64 encoded PNG format.
  // |PromiseValue|: base64Png
  [requiredCallback] static Promise<DOMString> takeScreenshot();

  // Tasks a screenshot for a display.
  // |displayId|: the display id of the display.
  // |Returns|: Promise that resolves when the operation has completed.
  // |PromiseValue|: base64Png
  [requiredCallback]
  static Promise<DOMString> takeScreenshotForDisplay(DOMString displayId);

  // Triggers an on-demand update of smart dim component and checks whether
  // it's successfully loaded by smart dim ml_agent.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback] static Promise<undefined> loadSmartDimComponent();

  // Whether the local list of installed ARC packages has been refreshed for
  // the first time after user login.
  // |PromiseValue|: refreshed
  [requiredCallback] static Promise<boolean> isArcPackageListInitialRefreshed();

  // Set value for the specified user pref in the pref tree.
  [requiredCallback]
  static Promise<undefined> setAllowedPref(DOMString prefName, any value);

  // Clears value for the specified user pref in the pref tree.
  [requiredCallback]
  static Promise<undefined> clearAllowedPref(DOMString prefName);

  // DEPRECATED: use SetAllowedPref instead, see crbug/1262034
  // Set value for the specified user pref in the pref tree.
  [requiredCallback]
  static Promise<undefined> setWhitelistedPref(DOMString prefName, any value);

  // Enable/disable a Crostini app's "scaled" property.
  // |appId|: The Crostini application ID.
  // |scaled|: The app is "scaled" when shown, which means it uses low display
  //           density.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> setCrostiniAppScaled(DOMString appId,
                                                 boolean scaled);

  // Get the primary display scale factor. |callback| is invoked with the scale
  // factor.
  // |PromiseValue|: scaleFactor
  [requiredCallback] static Promise<double> getPrimaryDisplayScaleFactor();

  // Get the tablet mode enabled status. |callback| is invoked with the tablet
  // mode enablement status.
  // |PromiseValue|: enabled
  [requiredCallback] static Promise<boolean> isTabletModeEnabled();

  // Enable/disable tablet mode. After calling this function, it won't be
  // possible to physically switch to/from tablet mode since that
  // functionality will be disabled.
  // |enabled|: if set, enable tablet mode.
  // |Returns|: Promise that resolves when the operation has completed.
  // |PromiseValue|: enabled
  [requiredCallback]
  static Promise<boolean> setTabletModeEnabled(boolean enabled);

  // Get the list of all installed applications
  // |PromiseValue|: apps
  [requiredCallback] static Promise<sequence<App>> getAllInstalledApps();

  // Get the list of all shelf items
  // |PromiseValue|: items
  [requiredCallback] static Promise<sequence<ShelfItem>> getShelfItems();

  // Get the launcher search box search state.
  // |PromiseValue|: state
  [requiredCallback]
  static Promise<LauncherSearchBoxState> getLauncherSearchBoxState();

  // Get the shelf auto hide behavior.
  // |displayId|: display that contains the shelf. |callback| is invoked with
  // the shelf auto hide behavior. Possible behavior values are:
  // "always", "never" or "hidden".
  // |PromiseValue|: behavior
  [requiredCallback]
  static Promise<DOMString> getShelfAutoHideBehavior(DOMString displayId);

  // Set the shelf auto hide behavior.
  // |displayId|: display that contains the shelf.
  // |behavior|: an enum of "always", "never" or "hidden".
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> setShelfAutoHideBehavior(DOMString displayId,
                                                     DOMString behavior);

  // Get the shelf alignment.
  // |displayId|: display that contains the shelf. |callback| is invoked with
  // the shelf alignment type.
  // |PromiseValue|: alignment
  [requiredCallback]
  static Promise<ShelfAlignmentType> getShelfAlignment(DOMString displayId);

  // Set the shelf alignment.
  // |displayId|: display that contains the shelf.
  // |alignment|: the type of alignment to set.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> setShelfAlignment(DOMString displayId,
                                              ShelfAlignmentType alignment);

  // Create a pin on shelf for the app specified by |appId|.
  // Deprecated. Use setShelfIconPin() instead.
  [requiredCallback] static Promise<undefined> pinShelfIcon(DOMString appId);

  // Update pin states of the shelf apps based on |updateParams|. Return a
  // list of app ids whose pin state changed. Pin states will not be changed
  // if the method fails.
  // |PromiseValue|: results
  static Promise<sequence<DOMString>> setShelfIconPin(
      sequence<ShelfIconPinUpdateParam> updateParams);

  // Enter or exit the overview mode.
  // |start|: whether entering to or exiting from the overview mode.
  // |Returns|: Promise that resolves after the overview mode switch finishes.
  // |PromiseValue|: finished
  [requiredCallback]
  static Promise<boolean> setOverviewModeState(boolean start);

  // Show virtual keyboard of the current input method if it's available.
  static undefined showVirtualKeyboardIfEnabled();

  // Sends the overlay color and theme to Android and changes the Android system
  // color and theme to these values.
  // |color|: the int color of the system ui.
  // |theme|: the theme of the system ui.
  // |Returns|: Promise that resolves with the sendArcOverlayColor result.
  // |PromiseValue|: result
  [requiredCallback]
  static Promise<boolean> sendArcOverlayColor(long color, ThemeStyle theme);

  // Start ARC performance tracing for the active ARC app window.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback] static Promise<undefined> arcAppTracingStart();

  // Stop ARC performance tracing if it was started and analyze results.
  // |Returns|: Promise that resolves with tracing results.
  // |PromiseValue|: info
  [requiredCallback]
  static Promise<ArcAppTracingInfo> arcAppTracingStopAndAnalyze();

  // Swap the windows in the split view.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback] static Promise<undefined> swapWindowsInSplitView();

  // Set ARC app window focused.
  // |packageName|: the package name of the ARC app window.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> setArcAppWindowFocus(DOMString packageName);

  // Invokes the callback when the display rotation animation is finished, or
  // invokes it immediately if it is not animating. The callback argument is
  // true if the display's rotation is same as |rotation|, or false otherwise.
  // |displayId|: display that contains the shelf.
  // |rotation|: the target rotation.
  // |Returns|: Promise that resolves when the operation has completed.
  // |PromiseValue|: success
  [requiredCallback]
  static Promise<boolean> waitForDisplayRotation(DOMString displayId,
                                                 RotationType rotation);

  // Get information on all application windows. Callback will be called
  // with the list of |AppWindowInfo| dictionary.
  // |Returns|: Promise that resolves with window list.
  // |PromiseValue|: windowList
  [requiredCallback] static Promise<sequence<AppWindowInfo>> getAppWindowList();

  // Send WM event to change the app window's window state.
  // |id|: the id of the window
  // |change|: WM event type to send to the app window.
  // |wait|: whether the method should wait for the window state to change
  // before returning.
  // |Returns|: Promise that resolves when the window state is changed if |wait|
  // is true. Otherwise, resolves right after the WM event is sent.
  // |PromiseValue|: currentType
  [requiredCallback]
  static Promise<WindowStateType> setAppWindowState(
      long id,
      WindowStateChangeDict change,
      optional boolean wait);

  // Activate app window given by "id".
  // |id|: the id of the window
  // |Returns|: Promise that resolves when the window is requested to activate.
  [requiredCallback] static Promise<undefined> activateAppWindow(long id);

  // Closes an app window given by "id".
  // |id|: the id of the window
  // |Returns|: Promise that resolves when the window is requested to close.
  [requiredCallback] static Promise<undefined> closeAppWindow(long id);

  // Installs the Progressive Web App (PWA) that is in the current URL.
  // |timeoutMs|: Timeout in milliseconds for the operation to complete.
  // |Returns|: Promise that resolves when the operation has completed with the
  // app Id of the recently installed PWA.
  // |PromiseValue|: appId
  [requiredCallback]
  static Promise<DOMString> installPWAForCurrentURL(long timeoutMs);

  // Activates shortcut.
  // |accelerator|: the accelerator to activate.
  // |Returns|: Promise that resolves when the operation has completed.
  // |PromiseValue|: success
  [requiredCallback]
  static Promise<boolean> activateAccelerator(Accelerator accelerator);

  // Wwait until the launcher is transitionto the |launcherState|, if it's not
  // in that state.
  // |launcherState|: the target launcher state.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> waitForLauncherState(
      LauncherStateType launcherState);

  // Wait until overview has transitioned to |overviewState|, if it is not in
  // that state.
  // |overviewState|: the target overview state.
  // |Returns|: Promise that resolves when overview has reached |overviewState|.
  [requiredCallback]
  static Promise<undefined> waitForOverviewState(
      OverviewStateType overviewState);

  // Creates a new desk if the maximum number of desks has not been reached.
  // |Returns|: Promise that resolves with a boolean to indicate success or
  // failure.
  // |PromiseValue|: success
  [requiredCallback] static Promise<boolean> createNewDesk();

  // Activates the desk at the given |index| triggering the activate-desk
  // animation.
  // |index|: the zero-based index of the desk desired to be activated.
  // |Returns|: Promise that resolves with a boolean indicating success when the
  // animation completes, or failure when the desk at |index| is already the
  // active desk.
  // |PromiseValue|: success
  [requiredCallback] static Promise<boolean> activateDeskAtIndex(long index);

  // Removes the currently active desk and triggers the remove-desk animation.
  // |Returns|: Promise that resolves with a boolean indicating success when the
  // animation completes, or failure if the currently active desk is the last
  // available desk which cannot be removed.
  // |PromiseValue|: success
  [requiredCallback] static Promise<boolean> removeActiveDesk();

  // Activates the desk at the given |index| by chaining multiple
  // activate-desk animations.
  // |index|: the zero-based index of the desk desired to be activated.
  // |Returns|: Promise that resolves with a boolean indicating success when the
  // animation completes, or failure when the desk at |index| is already the
  // active desk.
  // |PromiseValue|: success
  [requiredCallback]
  static Promise<boolean> activateAdjacentDesksToTargetIndex(long index);

  // Fetches the number of open desks in the `DesksController` at the time of
  // call. `callback`: callback that is passed the number of open desks.
  // |PromiseValue|: count
  [requiredCallback] static Promise<long> getDeskCount();

  // Fetches info about the open desks at the time of the call. `callback`:
  // callback that is passed desks information.
  // |PromiseValue|: desks
  [requiredCallback] static Promise<DesksInfo> getDesksInfo();

  // Create mouse events to cause a mouse click.
  // |button|: the mouse button for the click event.
  // |Returns|: Promise that resolves after the mouse click finishes.
  [requiredCallback] static Promise<undefined> mouseClick(MouseButton button);

  // Create a mouse event to cause mouse pressing. The mouse button stays
  // in the pressed state.
  // |button|: the mouse button to be pressed.
  // |Returns|: Promise that resolves after the mouse pressed event is handled.
  [requiredCallback] static Promise<undefined> mousePress(MouseButton button);

  // Create a mouse event to release a mouse button. This does nothing and
  // returns immediately if the specified button is not pressed.
  // |button|: the mouse button to be released.
  // |Returns|: Promise that resolves after the mouse is released.
  [requiredCallback] static Promise<undefined> mouseRelease(MouseButton button);

  // Create mouse events to move a mouse cursor to the location. This can
  // cause a dragging if a button is pressed. It starts from the last mouse
  // location.
  // |location|: the target location (in screen coordinate).
  // |durationInMs|: the duration (in milliseconds) for the mouse movement.
  //    The mouse will move linearly. 0 means moving immediately.
  // |Returns|: Promise that resolves after the mouse move finishes.
  [requiredCallback]
  static Promise<undefined> mouseMove(Location location,
                                      double durationInMs);

  // Enable/disable metrics reporting in preferences.
  // |enabled|: Enable metrics reporting.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> setMetricsEnabled(boolean enabled);

  // Sends ARC touch mode enabled or disabled.
  // |enable|: whether enabled touch mode.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback] static Promise<undefined> setArcTouchMode(boolean enabled);

  // Fetches ui information of scrollable shelf view for the given shelf state.
  // This function does not change scrollable shelf.
  // [deprecated="Use getShelfUIInfoForState()"]
  // |PromiseValue|: info
  [requiredCallback]
  static Promise<ScrollableShelfInfo> getScrollableShelfInfoForState(
      ScrollableShelfState state);

  // Fetches UI information of shelf (including scrollable shelf and hotseat)
  // for the given shelf state. This function does not change any shelf
  // component.
  // |PromiseValue|: info
  [requiredCallback]
  static Promise<ShelfUIInfo> getShelfUIInfoForState(ShelfState state);

  // Sends a WM event to change a window's bounds and/or the display it is on.
  // |id|: the id of the window.
  // |bounds|: bounds the window should be set to.
  // |displayId|: id of display to move the window to.
  // |Returns|: Promise that resolves when the window bounds are changed.
  // |PromiseValue|: result
  [requiredCallback]
  static Promise<SetWindowBoundsResult> setWindowBounds(long id,
                                                        Bounds bounds,
                                                        DOMString displayId);

  // Starts smoothness tracking for a display. If the display id is not
  // specified, the primary display is used. Otherwise, the display specified
  // by the display id is used. If `throughputIntervalMs` is not specified,
  // default 5 seconds interval is used to collect throughput data.
  [requiredCallback]
  static Promise<undefined> startSmoothnessTracking(
      optional DOMString displayId,
      optional long throughputIntervalMs);

  // Stops smoothness tracking for a display and report the smoothness. If the
  // display id is not specified, the primary display is used. Otherwise, the
  // display specified by the display id is used.
  // |Returns|: Promise that resolves with the smoothness after
  // StopSmoothnessTracking is called.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<DisplaySmoothnessData> stopSmoothnessTracking(
      optional DOMString displayId);

  // When neccesary, disables showing the dialog when Switch Access is disabled.
  static undefined disableSwitchAccessDialog();

  // Waits for the completion of photo transition animation in ambient mode.
  // |numCompletions|: number of completions of the animation.
  // |timeout|: the timeout in seconds.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> waitForAmbientPhotoAnimation(long numCompletions,
                                                         long timeout);

  // Waits for ambient video to successfully start playback.
  // |timeout|: the timeout in seconds.
  // |Returns|: Promise that resolves when the operation has completed.
  [requiredCallback]
  static Promise<undefined> waitForAmbientVideo(long timeout);

  // Disables the automation feature. Note that the event handlers and caches
  // of automation nodes still remain in the test extension and so the next
  // automation.getDesktop will miss initialization. The caller should ensure
  // invalidation of those information (i.e. reloading the entire background
  // page).
  [requiredCallback] static Promise<undefined> disableAutomation();

  // Starts to ui::ThroughputTracker data collection for tracked animations.
  [requiredCallback]
  static Promise<undefined> startThroughputTrackerDataCollection();

  // Stops ui::ThroughputTracker data collection and reports the collected data
  // since the start or the last GetThroughtputTrackerData call.
  // |Returns|: Promise that resolves with the collection ui::ThroughputTracker
  // data after stopThroughputTrackerDataCollection is called.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<sequence<ThroughputTrackerAnimationData>>
  stopThroughputTrackerDataCollection();

  // Reports the currently collected animation data.
  // |Returns|: Promise that resolves with the currently collected
  // ui::ThroughputTracker animation data. Note that the data reported is
  // removed to avoid reporting duplicated data.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<sequence<ThroughputTrackerAnimationData>>
  getThroughputTrackerData();

  // Gets the smoothness of a display. If the display id is not specified, the
  // primary display is used.
  // |Returns|: Promise that resolves with the smoothness percentage after
  // getDisplaySmoothness is called.
  // |PromiseValue|: smoothness
  [requiredCallback]
  static Promise<long> getDisplaySmoothness(optional DOMString displayId);

  // Resets the holding space by removing all items and clearing the prefs.
  [requiredCallback]
  static Promise<undefined> resetHoldingSpace(
      optional ResetHoldingSpaceOptions options);

  // Starts collection of ui::LoginEventRecorder data.
  [requiredCallback]
  static Promise<undefined> startLoginEventRecorderDataCollection();

  // Stops ui::LoginEventRecorder data collection and reports all the collected
  // data.
  // |Returns|: Promise that resolves with the collection ui::LoginEventRecorder
  // data after getLoginEventRecorderLoginEvents is called.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<sequence<LoginEventRecorderData>>
  getLoginEventRecorderLoginEvents();

  // Adds login event to test LoginEventRecorderDataCollection API.
  [requiredCallback] static Promise<undefined> addLoginEventForTesting();

  // Force auto theme mode in dark mode or light mode for testing.
  [requiredCallback] static Promise<undefined> forceAutoThemeMode(
      boolean darkModeEnabled);

  // Fetches an access token from Chrome.
  // |Returns|: Promise that resolves with the access token data.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<GetAccessTokenData> getAccessToken(
      GetAccessTokenParams accessTokenParams);

  // Returns whether the current input method is ready to accept key events.
  // |Returns|: Promise that resolves with whether the current input method is
  // ready to accept key events from the test.
  // |PromiseValue|: isReady
  [requiredCallback] static Promise<boolean> isInputMethodReadyForTesting();

  // Creates a temporary directory visible under the Fusebox mount point.
  // |Returns|: Promise that resolves when the temporary directory was made.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<MakeFuseboxTempDirData> makeFuseboxTempDir();

  // Removes a temporary directory visible under the Fusebox mount point. The
  // fuseboxFilePath argument was returned by the MakeFuseboxTempDirCallback.
  // |Returns|: Promise that resolves when the temporary directory was removed.
  [requiredCallback]
  static Promise<undefined> removeFuseboxTempDir(DOMString fuseboxFilePath);

  // Remove the specified component extension.
  [requiredCallback]
  static Promise<undefined> removeComponentExtension(DOMString extensionId);

  // Starts frame counting in viz. `bucketSizeInSeconds` decides the bucket
  // size of the frame count records. If it is X seconds, each record is
  // the number of presented frames in X seconds.
  [requiredCallback]
  static Promise<undefined> startFrameCounting(long bucketSizeInSeconds);

  // Ends frame counting in viz and return the collected data.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<sequence<FrameCountingPerSinkData>> stopFrameCounting();

  // Starts overdraw tracking for the display associated with
  // `displayId` in viz. `bucketSizeInSeconds` decides the bucket size
  // of the overdraw records.
  // If it is X seconds, each record is the average overdraw of the
  // frames presented on the display in X seconds.
  [requiredCallback]
  static Promise<undefined> startOverdrawTracking(
      long bucketSizeInSeconds,
      optional DOMString displayId);

  // Ends overdraw tracking in viz and return the collected data.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<OverdrawData> stopOverdrawTracking(
      optional DOMString displayId);

  // Install a bruschetta VM.
  [requiredCallback]
  static Promise<undefined> installBruschetta(DOMString vmName);

  // Delete a bruschetta VM.
  [requiredCallback]
  static Promise<undefined> removeBruschetta(DOMString vmName);

  // Returns whether a base::Feature is enabled. The state may change because
  // a Chrome uprev into ChromeOS changed the default feature state.
  // |PromiseValue|: enabled
  [requiredCallback]
  static Promise<boolean> isFeatureEnabled(DOMString featureName);

  // Returns keyboard layout used for current input method.
  // |Returns|: Promise that resolves with the current input method keyboard
  // layout.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<GetCurrentInputMethodDescriptorData>
  getCurrentInputMethodDescriptor();

  // Overrides the response from Lobster Fetcher and returns the boolean value
  // that indicates if the overriding is successful or not.
  // |PromiseValue|: success
  [requiredCallback]
  static Promise<boolean> overrideLobsterResponseForTesting();

  // Overrides the response from Orca Provider and returns the boolean value
  // that indicates if the overriding is successful or not.
  // |PromiseValue|: success
  [requiredCallback]
  static Promise<boolean> overrideOrcaResponseForTesting(
      OrcaResponseArray array);

  // Overrides the response from Scanner Provider and returns the boolean
  // value that indicates if the overriding is successful or not.
  // |PromiseValue|: success
  [requiredCallback]
  static Promise<boolean> overrideScannerResponsesForTesting(
      ScannerResponseArray array);

  // ARC set interactive enable/disable state.
  // |enabled|: Enable ARC interactive.
  // |Returns|: Promise that resolves when the operation is sent to ARC by mojo.
  [requiredCallback]
  static Promise<undefined> setArcInteractiveState(boolean enabled);

  // Returns whether a field trial exists and has been activated.
  // |Returns|: Promise that resolves with a boolean indicating if a field trial
  // exists and has been activated.
  // |PromiseValue|: active
  [requiredCallback]
  static Promise<boolean> isFieldTrialActive(DOMString trialName,
                                             DOMString groupName);

  // ARC get wakefulness mode.
  // |PromiseValue|: mode
  [requiredCallback] static Promise<WakefulnessMode> getArcWakefulnessMode();

  // Sets the default device language.
  // A restart is required for this change to take effect.
  [requiredCallback]
  static Promise<undefined> setDeviceLanguage(DOMString locale);

  // Gets the chrome://device-log entries for a given type or all types.
  // |type|: A string like "printer" to fetch a specific type, or an empty
  // string to fetch all entries.
  // |Returns|: Promose that resolves with the logs as a single string.
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<DOMString> getDeviceEventLog(DOMString type);

  // Fired when the data in ui::Clipboard is changed.
  static attribute OnClipboardDataChangedEvent onClipboardDataChanged;
};

partial interface Browser {
  static attribute AutotestPrivate autotestPrivate;
};

