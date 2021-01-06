// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/constants/chromeos_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace chromeos {
namespace features {
namespace {

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
const base::Feature kInstantTetheringBackgroundAdvertisementSupport{
    "InstantTetheringBackgroundAdvertisementSupport",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Enables redesign of account management flows.
// https://crbug.com/1132472
const base::Feature kAccountManagementFlowsV2{
    "AccountManagementFlowsV2", base::FEATURE_DISABLED_BY_DEFAULT};

// Shows settings for adjusting scroll acceleration/sensitivity for
// mouse/touchpad.
const base::Feature kAllowScrollSettings{"AllowScrollSettings",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable Ambient mode feature.
const base::Feature kAmbientModeFeature{"ChromeOSAmbientMode",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

constexpr base::FeatureParam<bool> kAmbientModeCapturedOnPixelAlbumEnabled{
    &kAmbientModeFeature, "CapturedOnPixelAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeFineArtAlbumEnabled{
    &kAmbientModeFeature, "FineArtAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeFeaturedPhotoAlbumEnabled{
    &kAmbientModeFeature, "FeaturedPhotoAlbumEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeEarthAndSpaceAlbumEnabled{
    &kAmbientModeFeature, "EarthAndSpaceAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeStreetArtAlbumEnabled{
    &kAmbientModeFeature, "StreetArtAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeDefaultFeedEnabled{
    &kAmbientModeFeature, "DefaultFeedEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModePersonalPhotosEnabled{
    &kAmbientModeFeature, "PersonalPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeFeaturedPhotosEnabled{
    &kAmbientModeFeature, "FeaturedPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeGeoPhotosEnabled{
    &kAmbientModeFeature, "GeoPhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeCulturalInstitutePhotosEnabled{
    &kAmbientModeFeature, "CulturalInstitutePhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeRssPhotosEnabled{
    &kAmbientModeFeature, "RssPhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeCapturedOnPixelPhotosEnabled{
    &kAmbientModeFeature, "CapturedOnPixelPhotosEnabled", false};

// Controls whether to enable Ambient mode album selection with photo previews.
const base::Feature kAmbientModePhotoPreviewFeature{
    "ChromeOSAmbientModePhotoPreview", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to allow Dev channel to use Prod server feature.
const base::Feature kAmbientModeDevUseProdFeature{
    "ChromeOSAmbientModeDevChannelUseProdServer",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable ARC ADB sideloading support.
const base::Feature kArcAdbSideloadingFeature{
    "ArcAdbSideloading", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable support for ARC ADB sideloading for managed
// accounts and/or devices.
const base::Feature kArcManagedAdbSideloadingSupport{
    "ArcManagedAdbSideloadingSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable support for View.onKeyPreIme() of ARC apps.
const base::Feature kArcPreImeKeyEventSupport{
    "ArcPreImeKeyEventSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
const base::Feature kAutoScreenBrightness{"AutoScreenBrightness",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable assistive autocorrect.
const base::Feature kAssistAutoCorrect{"AssistAutoCorrect",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assistive multi word suggestions.
const base::Feature kAssistMultiWord{"AssistMultiWord",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assistive personal information.
const base::Feature kAssistPersonalInfo{"AssistPersonalInfo",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to suggest addresses in assistive personal information. This
// is only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoAddress{
    "AssistPersonalInfoAddress", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest emails in assistive personal information. This is
// only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoEmail{"AssistPersonalInfoEmail",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest names in assistive personal information. This is
// only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoName{"AssistPersonalInfoName",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest phone numbers in assistive personal information.
// This is only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoPhoneNumber{
    "AssistPersonalInfoPhoneNumber", base::FEATURE_ENABLED_BY_DEFAULT};

// Displays the avatar toolbar button and the profile menu.
// https://crbug.com/1041472
extern const base::Feature kAvatarToolbarButton{
    "AvatarToolbarButton", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables better version of the UpdateScreen in OOBE. Displays low battery
// warnings during the update downloading and further stages. Provides time
// estimates of the update stages.
// https://crbug.com/1101317
const base::Feature kBetterUpdateScreen{"BetterUpdateScreen",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables more aggressive filtering out of Bluetooth devices with
// "appearances" that are less likely to be pairable or useful.
const base::Feature kBluetoothAggressiveAppearanceFilter{
    "BluetoothAggressiveAppearanceFilter", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Bluetooth WBS microphone be selected as default
// audio input option.
const base::Feature kBluetoothWbsDogfood{"BluetoothWbsDogfood",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the usage of fixed Bluetooth A2DP packet size to improve
// audio performance in noisy environment.
const base::Feature kBluetoothFixA2dpPacketSize{
    "BluetoothFixA2dpPacketSize", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables more filtering out of phones from the Bluetooth UI.
const base::Feature kBluetoothPhoneFilter{"BluetoothPhoneFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disables running the Camera App as a System Web App.
const base::Feature kCameraSystemWebApp{"CameraSystemWebApp",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the preference of using constant frame rate for camera
// when streaming.
const base::Feature kPreferConstantFrameRate{"PreferConstantFrameRate",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, will use the CDM in the Chrome OS daemon rather than loading the
// CDM using the library CDM interface.
const base::Feature kCdmFactoryDaemon{"CdmFactoryDaemon",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, send the LTE attach APN configuration to the modem.
const base::Feature kCellularUseAttachApn{"CellularUseAttachApn",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables entry point for child account sign in or creation.
const base::Feature kChildSpecificSignin{"ChildSpecificSignin",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables using the new Connectivity Diagnostics WebUI app.
const base::Feature kConnectivityDiagnosticsWebUi{
    "ConnectivityDiagnosticsWebUi", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, options page for each input method will be opened in ChromeOS
// settings. Otherwise it will be opened in a new web page in Chrome browser.
const base::Feature kImeOptionsInSettings{"ImeOptionsInSettings",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini Disk Resizing.
const base::Feature kCrostiniDiskResizing{"CrostiniDiskResizing",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini using Buster container images.
const base::Feature kCrostiniUseBusterImage{"CrostiniUseBusterImage",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini GPU support.
const base::Feature kCrostiniGpuSupport{"CrostiniGpuSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the new WebUI Crostini upgrader.
const base::Feature kCrostiniWebUIUpgrader{"CrostiniWebUIUpgrader",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Use DLC instead of component updater for managing the Termina image if set
// (and component updater instead of DLC if not).
const base::Feature kCrostiniUseDlc{"CrostiniUseDlc",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// DLC Service is available for use on the board, prerequisite for the UseDlc
// flag.
const base::Feature kCrostiniEnableDlc{"CrostiniEnableDlc",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables using Cryptauth's GetDevicesActivityStatus API.
const base::Feature kCryptAuthV2DeviceActivityStatus{
    "CryptAuthV2DeviceActivityStatus", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables use of the connectivity status from Cryptauth's
// GetDevicesActivityStatus API to sort devices.
const base::Feature kCryptAuthV2DeviceActivityStatusUseConnectivity{
    "CryptAuthV2DeviceActivityStatusUseConnectivity",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Disable idle sockets closing on memory pressure for NetworkContexts that
// belong to Profiles. It only applies to Profiles because the goal is to
// improve perceived performance of web browsing within the Chrome OS user
// session by avoiding re-estabshing TLS connections that require client
// certificates.
const base::Feature kDisableIdleSocketsCloseOnMemoryPressure{
    "disable_idle_sockets_close_on_memory_pressure",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 DeviceSync flow. Regardless of this
// flag, v1 DeviceSync will continue to operate until it is disabled via the
// feature flag kDisableCryptAuthV1DeviceSync.
const base::Feature kCryptAuthV2DeviceSync{"CryptAuthV2DeviceSync",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 Enrollment flow.
const base::Feature kCryptAuthV2Enrollment{"CryptAuthV2Enrollment",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the Diagnostics app.
const base::Feature kDiagnosticsApp{"DiagnosticsApp",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Disables the CryptAuth v1 DeviceSync flow. Note: During the first phase
// of the v2 DeviceSync rollout, v1 and v2 DeviceSync run in parallel. This flag
// is needed to disable the v1 service during the second phase of the rollout.
// kCryptAuthV2DeviceSync should be enabled before this flag is flipped.
const base::Feature kDisableCryptAuthV1DeviceSync{
    "DisableCryptAuthV1DeviceSync", base::FEATURE_DISABLED_BY_DEFAULT};

// Disables "Office Editing for Docs, Sheets & Slides" component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const base::Feature kDisableOfficeEditingComponentApp{
    "DisableOfficeEditingComponentApp", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables duplex native messaging between DriveFS and extensions.
const base::Feature kDriveFsBidirectionalNativeMessaging{
    "DriveFsBidirectionalNativeMessaging", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables DriveFS' experimental local files mirroring functionality.
const base::Feature kDriveFsMirroring{"DriveFsMirroring",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, emoji suggestion will be shown when user type "space".
const base::Feature kEmojiSuggestAddition{"EmojiSuggestAddition",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables setting the device hostname.
const base::Feature kEnableHostnameSetting{"EnableHostnameSetting",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables LocalSearchService to be initialized.
const base::Feature kEnableLocalSearchService{"EnableLocalSearchService",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the OOBE ChromeVox hint dialog and announcement feature.
const base::Feature kEnableOobeChromeVoxHint{"EnableOobeChromeVoxHint",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Device End Of Lifetime warning notifications.
const base::Feature kEolWarningNotifications{"EolWarningNotifications",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable use of ordinal (unaccelerated) motion by Exo clients.
const base::Feature kExoOrdinalMotion{"ExoOrdinalMotion",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable pointer lock for Crostini windows.
const base::Feature kExoPointerLock{"ExoPointerLock",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables policy that controls feature to allow Family Link accounts on school
// owned devices.
const base::Feature kFamilyLinkOnSchoolDevice{"FamilyLinkOnSchoolDevice",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the camera folder handling in files app.
const base::Feature kFilesCameraFolder{"FilesCameraFolder",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the next generation file manager.
const base::Feature kFilesNG{"FilesNG", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables JS modules for Files app.
const base::Feature kFilesJsModules{"FilesJsModules",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables JS modules for Audio Player.
const base::Feature kAudioPlayerJsModules{"AudioPlayerJsModules",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables JS modules for Video Player.
const base::Feature kVideoPlayerJsModules{"VideoPlayerJsModules",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables partitioning of removable disks in file manager.
const base::Feature kFilesSinglePartitionFormat{
    "FilesSinglePartitionFormat", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the System Web App (SWA) version of file manager.
const base::Feature kFilesSWA{"FilesSWA", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable files app trash.
const base::Feature kFilesTrash{"FilesTrash",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables filters in Files app Recents view.
const base::Feature kFiltersInRecents{"FiltersInRecents",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new ZIP archive handling in Files App.
// https://crbug.com/912236
const base::Feature kFilesZipMount{"FilesZipMount",
                                   base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFilesZipPack{"FilesZipPack",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFilesZipUnpack{"FilesZipUnpack",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Gaia reauth endpoint with deleted user customization page.
const base::Feature kGaiaReauthEndpoint{"GaiaReauthEndpoint",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls gamepad vibration in Exo.
const base::Feature kGamepadVibration{"ExoGamepadVibration",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of Mojo by Chrome-process code to communicate with Power
// Manager. In order to use mojo, this feature must be turned on and a callsite
// must use PowerManagerMojoClient::Get().
const base::Feature kMojoDBusRelay{"MojoDBusRelay",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables pasting a few recently copied items in a menu when pressing search +
// v.
const base::Feature kClipboardHistory{"ClipboardHistory",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the clipboard nudge shown prefs will be reset at the start of
// each new user session.
const base::Feature kClipboardHistoryNudgeSessionReset{
    "ClipboardHistoryNudgeSessionReset", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables rendering html in Clipboard History only if an img or table tag is
// present.
const base::Feature kClipboardHistorySimpleRender{
    "ClipboardHistorySimpleRender", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, a blue new nudge will show on the context menu option for
// clipboard history.
const base::Feature kClipboardHistoryContextMenuNudge{
    "ClipboardHistoryContextMenuNudge", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables copying an image to the system clipboard to support pasting onto
// different surfaces
const base::Feature kEnableFilesAppCopyImage{"EnableFilesAppCopyImage",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enable restriction of symlink traversal on user-supplied filesystems.
const base::Feature kFsNosymfollow{"FsNosymfollow",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a D-Bus service for accessing gesture properties.
const base::Feature kGesturePropertiesDBusService{
    "GesturePropertiesDBusService", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable Guest OS external protocol handling.
const base::Feature kGuestOsExternalProtocol{"GuestOsExternalProtocol",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables editing with handwriting gestures within the virtual keyboard.
const base::Feature kHandwritingGestureEditing{
    "HandwritingGestureEditing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the search service integration in the Help app.
const base::Feature kHelpAppSearchServiceIntegration{
    "HelpAppSearchServiceIntegration", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable IME decoder via Mojo connection on Chrome OS.
const base::Feature kImeMojoDecoder{"ImeMojoDecoder",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable system emoji picker.
const base::Feature kImeSystemEmojiPicker{"SystemEmojiPicker",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables view-based version of multiprofile login, as opposed to Web UI one.
const base::Feature kViewBasedMultiprofileLogin{
    "ViewBasedMultiprofileLogin", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable using the floating virtual keyboard as the default option
// on Chrome OS.
const base::Feature kVirtualKeyboardFloatingDefault{
    "VirtualKeyboardFloatingDefault", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Kerberos Section in ChromeOS settings. When disabled, Kerberos
// settings will stay under People Section. https:://crbug.com/983041
const base::Feature kKerberosSettingsSection{"KerberosSettingsSection",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables "Linux and Chrome OS" support. Allows a Linux version of Chrome
// ("lacros-chrome") to run as a Wayland client with this instance of Chrome
// ("ash-chrome") acting as the Wayland server and window manager.
// NOTE: Use crosapi::browser_util::IsLacrosEnabled() instead of checking the
// feature directly. Lacros is not allowed for certain user types and can be
// disabled by policy. These restrictions will be lifted when Lacros development
// is complete.
const base::Feature kLacrosSupport{"LacrosSupport",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables language settings update.
const base::Feature kLanguageSettingsUpdate{"LanguageSettingsUpdate",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables device management disclosure on login / lock screen.
const base::Feature kLoginDeviceManagementDisclosure{
    "LoginDeviceManagementDisclosure", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable the requirement of a minimum chrome version on the
// device through the policy DeviceMinimumVersion. If the requirement is
// not met and the warning time in the policy has expired, the user is
// restricted from using the session.
const base::Feature kMinimumChromeVersion{"MinimumChromeVersion",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether new OOBE layout is shown or not.
const base::Feature kNewOobeLayout{"NewOobeLayout",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// ChromeOS Media App. https://crbug.com/996088.
const base::Feature kMediaApp{"MediaApp", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether image annotation is enabled in the ChromeOS media app.
const base::Feature kMediaAppAnnotation{"MediaAppAnnotation",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Whether known extensions for RAW image formats are handled by the ChromeOS
// Media App.
const base::Feature kMediaAppHandlesRaw{"MediaAppHandlesRaw",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Whether Pdf files loading ink is enabled in the ChromeOS media app.
const base::Feature kMediaAppPdfInInk{"MediaAppPdfInInk",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support for multilingual assistive typing on Chrome OS.
const base::Feature kMultilingualTyping{"MultilingualTyping",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable on-device grammar check service.
const base::Feature kOnDeviceGrammarCheck{"OnDeviceGrammarCheck",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a unique URL for each path in CrOS settings.
// This allows deep linking to individual settings, i.e. in settings search.
const base::Feature kOsSettingsDeepLinking{"OsSettingsDeepLinking",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Flips chrome://os-settings to show Polymer 3 version
const base::Feature kOsSettingsPolymer3{"OsSettingsPolymer3",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Provides a UI for users to view information about their Android phone
// and perform phone-side actions within Chrome OS.
const base::Feature kPhoneHub{"PhoneHub", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether Phone Hub will exclusively use BLE for its connection with
// the user's phone.
const base::Feature kPhoneHubUseBle{"PhoneHubUseBle",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables PIN setup in OOBE for Family Link users on all devices supporting low
// entropy credentials regardless the form factor.
const base::Feature kPinSetupForFamilyLink{"PinSetupForFamilyLink",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the camera permissions should be shown in the Plugin
// VM app settings.
const base::Feature kPluginVmShowCameraPermissions{
    "PluginVmShowCameraPermissions", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the microphone permissions should be shown in the Plugin
// VM app settings.
const base::Feature kPluginVmShowMicrophonePermissions{
    "PluginVmShowMicrophonePermissions", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to show printer statuses.
const base::Feature kPrinterStatus{"PrinterStatus",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to show printer statuses on the Print Preview destination
// dialog.
const base::Feature kPrinterStatusDialog{"PrinterStatusDialog",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable the Print Job Management App.
const base::Feature kPrintJobManagementApp{"PrintJobManagementApp",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Changes Print Preview Save to Drive to use local Drive.
const base::Feature kPrintSaveToDrive{"PrintSaveToDrive",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Allows print servers to be selected when beyond a specified limit.
const base::Feature kPrintServerScaling{"PrintServerScaling",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers.
const base::Feature kQuickAnswers{"QuickAnswers",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether dogfood version of quick answers.
const base::Feature kQuickAnswersDogfood{"QuickAnswersDogfood",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers text annotator.
const base::Feature kQuickAnswersTextAnnotator{
    "QuickAnswersTextAnnotator", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers translation.
const base::Feature kQuickAnswersTranslation{"QuickAnswersTranslation",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers translation using Cloud API.
const base::Feature kQuickAnswersTranslationCloudAPI{
    "QuickAnswersTranslationCloudAPI", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to trigger quick answers on editable text selection.
const base::Feature kQuickAnswersOnEditableText{
    "QuickAnswersOnEditableText", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the PIN auto submit feature is enabled.
const base::Feature kQuickUnlockPinAutosubmit{"QuickUnlockPinAutosubmit",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(crbug.com/1104164) - Remove this once most
// users have their preferences backfilled.
// Controls whether the PIN auto submit backfill operation should be performed.
const base::Feature kQuickUnlockPinAutosubmitBackfill{
    "QuickUnlockPinAutosubmitBackfill", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes notifications on Chrome OS.
const base::Feature kReleaseNotesNotification{"ReleaseNotesNotification",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes notifications on non-stable Chrome OS
// channels. Used for testing.
const base::Feature kReleaseNotesNotificationAllChannels{
    "ReleaseNotesNotificationAllChannels", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Release Notes suggestion chip on Chrome OS.
const base::Feature kReleaseNotesSuggestionChip{
    "ReleaseNotesSuggestionChip", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables a scanning UI on Chrome OS.
const base::Feature kScanningUI{"ScanningUI", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables long kill timeout for session manager daemon. When
// enabled, session manager daemon waits for a longer time (e.g. 12s) for chrome
// to exit before sending SIGABRT. Otherwise, it uses the default time out
// (currently 3s).
const base::Feature kSessionManagerLongKillTimeout{
    "SessionManagerLongKillTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables a toggle to enable Bluetooth debug logs.
const base::Feature kShowBluetoothDebugLogToggle{
    "ShowBluetoothDebugLogToggle", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables verbose Bluetooth log collection for Googlers.
const base::Feature kEnableBluetoothVerboseLogsForGooglers{
    "EnableBluetoothVerboseLogsForGooglers", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables showing the battery level in the System Tray and Settings
// UI for supported Bluetooth Devices.
const base::Feature kShowBluetoothDeviceBattery{
    "ShowBluetoothDeviceBattery", base::FEATURE_ENABLED_BY_DEFAULT};

// Shows the Play Store icon in Demo Mode.
const base::Feature kShowPlayInDemoMode{"ShowPlayInDemoMode",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Shows individual steps during Demo Mode setup.
const base::Feature kShowStepsInDemoModeSetup{"ShowStepsInDemoModeSetup",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Uses experimental component version for smart dim.
const base::Feature kSmartDimExperimentalComponent{
    "SmartDimExperimentalComponent", base::FEATURE_DISABLED_BY_DEFAULT};

// Uses the smart dim component updater to provide smart dim model and
// preprocessor configuration.
const base::Feature kSmartDimNewMlAgent{"SmartDimNewMlAgent",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Uses the V3 (~2019-05 era) Smart Dim model instead of the default V2
// (~2018-11) model.
const base::Feature kSmartDimModelV3{"SmartDimModelV3",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// This feature:
// - Creates a new "Sync your settings" section in Chrome OS settings
// - Moves app, wallpaper and Wi-Fi sync to OS settings
// - Provides a separate toggle for OS preferences, distinct from browser
//   preferences
// - Makes the OS ModelTypes run in sync transport mode, controlled by a
//   master pref for the OS sync feature
// - Updates the OOBE sync consent screen
//
// NOTE: The feature is rolling out via a client-side Finch trial, so the actual
// state will vary. See config in
// chrome/browser/chromeos/sync/split_settings_sync_field_trial.cc
const base::Feature kSplitSettingsSync{"SplitSettingsSync",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a settings UI toggle that controls Suggested Content status. Also
// enables a corresponding notice in the Launcher about Suggested Content.
const base::Feature kSuggestedContentToggle{"SuggestedContentToggle",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables using the system input engine for physical typing in
// languages based on latin script.
const base::Feature kSystemLatinPhysicalTyping{
    "SystemLatinPhysicalTyping", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Chrome OS Telemetry Extension.
const base::Feature kTelemetryExtension{"TelemetryExtension",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables unified media view in Files app to browse recently-modified media
// files from local local, Google Drive, and Android.
const base::Feature kUnifiedMediaView{"UnifiedMediaView",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the updated cellular activation UI; see go/cros-cellular-design.
const base::Feature kUpdatedCellularActivationUi{
    "UpdatedCellularActivationUi", base::FEATURE_DISABLED_BY_DEFAULT};

// Uses the same browser sync consent dialog as Windows/Mac/Linux. Allows the
// user to fully opt-out of browser sync, including marking the IdentityManager
// primary account as unconsented. Requires SplitSettingsSync.
// NOTE: Call UseBrowserSyncConsent() to test the flag, see implementation.
const base::Feature kUseBrowserSyncConsent{"UseBrowserSyncConsent",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Use the staging server as part of the Wallpaper App to verify
// additions/removals of wallpapers.
const base::Feature kUseWallpaperStagingUrl{"UseWallpaperStagingUrl",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
const base::Feature kUseMessagesStagingUrl{"UseMessagesStagingUrl",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables user activity prediction for power management on
// Chrome OS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
const base::Feature kUserActivityPrediction{"UserActivityPrediction",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Remap search+click to right click instead of the legacy alt+click on
// Chrome OS.
const base::Feature kUseSearchClickForRightClick{
    "UseSearchClickForRightClick", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable bordered key for virtual keyboard on Chrome OS.
const base::Feature kVirtualKeyboardBorderedKey{
    "VirtualKeyboardBorderedKey", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable the camera/mic indicators/notifications for VMs.
const base::Feature kVmCameraMicIndicatorsAndNotifications{
    "VmCameraMicIndicatorsAndNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable syncing of Wi-Fi configurations between
// ChromeOS and a connected Android phone.
const base::Feature kWifiSyncAndroid{"WifiSyncAndroid",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable MOZC IME to use protobuf as interactive message format.
const base::Feature kImeMozcProto{"ImeMozcProto",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Force enable recreating the LXD DB at LXD launch.
const base::Feature kCrostiniResetLxdDb{"CrostiniResetLxdDb",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the camera privacy switch toasts and notification should be
// displayed.
const base::Feature kCameraPrivacySwitchNotifications{
    "CameraPrivacySwitchNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

////////////////////////////////////////////////////////////////////////////////

bool IsAccountManagementFlowsV2Enabled() {
  return base::FeatureList::IsEnabled(kAccountManagementFlowsV2);
}

bool IsAmbientModeEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeFeature);
}

bool IsAmbientModePhotoPreviewEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModePhotoPreviewFeature);
}

bool IsAmbientModeDevUseProdEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeDevUseProdFeature);
}

bool IsBetterUpdateEnabled() {
  return base::FeatureList::IsEnabled(kBetterUpdateScreen);
}

bool IsChildSpecificSigninEnabled() {
  return base::FeatureList::IsEnabled(kChildSpecificSignin);
}

bool IsDeepLinkingEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsDeepLinking);
}

bool IsDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kDiagnosticsApp);
}

bool IsHostnameSettingEnabled() {
  return base::FeatureList::IsEnabled(kEnableHostnameSetting);
}

bool IsLocalSearchServiceEnabled() {
  return base::FeatureList::IsEnabled(kEnableLocalSearchService);
}

bool IsFamilyLinkOnSchoolDeviceEnabled() {
  return base::FeatureList::IsEnabled(kFamilyLinkOnSchoolDevice);
}

bool IsGaiaReauthEndpointEnabled() {
  return base::FeatureList::IsEnabled(kGaiaReauthEndpoint);
}

bool IsInstantTetheringBackgroundAdvertisingSupported() {
  return base::FeatureList::IsEnabled(
      kInstantTetheringBackgroundAdvertisementSupport);
}

bool IsKerberosSettingsSectionEnabled() {
  return base::FeatureList::IsEnabled(kKerberosSettingsSection);
}

bool IsLoginDeviceManagementDisclosureEnabled() {
  return base::FeatureList::IsEnabled(kLoginDeviceManagementDisclosure);
}

bool IsMinimumChromeVersionEnabled() {
  return base::FeatureList::IsEnabled(kMinimumChromeVersion);
}

bool IsNewOobeLayoutEnabled() {
  return base::FeatureList::IsEnabled(kNewOobeLayout);
}

bool IsOobeChromeVoxHintEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeChromeVoxHint);
}

bool IsClipboardHistoryEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistory) ||
         base::FeatureList::IsEnabled(kClipboardHistorySimpleRender);
}

bool IsClipboardHistoryNudgeSessionResetEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryNudgeSessionReset);
}

bool IsClipboardHistorySimpleRenderEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistory) ||
         base::FeatureList::IsEnabled(kClipboardHistorySimpleRender);
}

bool IsClipboardHistoryContextMenuNudgeEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryContextMenuNudge);
}

bool IsPhoneHubEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHub);
}

bool IsPhoneHubUseBleEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHubUseBle) && IsPhoneHubEnabled();
}

bool IsPinSetupForFamilyLinkEnabled() {
  return base::FeatureList::IsEnabled(kPinSetupForFamilyLink);
}

bool IsPinAutosubmitFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmit);
}

bool IsPinAutosubmitBackfillFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmitBackfill);
}

bool IsQuickAnswersDogfood() {
  return base::FeatureList::IsEnabled(kQuickAnswersDogfood);
}

bool IsQuickAnswersEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswers);
}

bool IsQuickAnswersTextAnnotatorEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersTextAnnotator);
}

bool IsQuickAnswersTranslationEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersTranslation);
}

bool IsQuickAnswersTranslationCloudAPIEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersTranslationCloudAPI);
}

bool IsQuickAnswersOnEditableTextEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersOnEditableText);
}

bool IsSplitSettingsSyncEnabled() {
  return base::FeatureList::IsEnabled(kSplitSettingsSync);
}

bool IsViewBasedMultiprofileLoginEnabled() {
  return base::FeatureList::IsEnabled(kViewBasedMultiprofileLogin);
}

bool IsWifiSyncAndroidEnabled() {
  return base::FeatureList::IsEnabled(kWifiSyncAndroid);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

bool ShouldUseBrowserSyncConsent() {
  // UseBrowserSyncConsent requires SplitSettingsSync.
  return base::FeatureList::IsEnabled(kSplitSettingsSync) &&
         base::FeatureList::IsEnabled(kUseBrowserSyncConsent);
}

bool ShouldUseV1DeviceSync() {
  return !ShouldUseV2DeviceSync() ||
         !base::FeatureList::IsEnabled(
             chromeos::features::kDisableCryptAuthV1DeviceSync);
}

bool ShouldUseV2DeviceSync() {
  return base::FeatureList::IsEnabled(
             chromeos::features::kCryptAuthV2Enrollment) &&
         base::FeatureList::IsEnabled(
             chromeos::features::kCryptAuthV2DeviceSync);
}

}  // namespace features
}  // namespace chromeos
