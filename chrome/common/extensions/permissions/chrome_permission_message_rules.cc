// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_rules.h"

#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// The default formatter for a permission message. Simply displays the message
// with the given ID.
class DefaultPermissionMessageFormatter
    : public ChromePermissionMessageFormatter {
 public:
  explicit DefaultPermissionMessageFormatter(int message_id)
      : message_id_(message_id) {}
  ~DefaultPermissionMessageFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    return PermissionMessage(l10n_util::GetStringUTF16(message_id_),
                             permissions);
  }

 private:
  int message_id_;

  DISALLOW_COPY_AND_ASSIGN(DefaultPermissionMessageFormatter);
};

// A formatter that substitutes the parameter into the message using string
// formatting.
// NOTE: Only one permission with the given ID is substituted using this rule.
class SingleParameterFormatter : public ChromePermissionMessageFormatter {
 public:
  explicit SingleParameterFormatter(int message_id) : message_id_(message_id) {}
  ~SingleParameterFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    DCHECK(permissions.size() > 0);
    std::vector<base::string16> parameters =
        permissions.GetAllPermissionParameters();
    DCHECK_EQ(1U, parameters.size())
        << "Only one message with each ID can be parameterized.";
    return PermissionMessage(
        l10n_util::GetStringFUTF16(message_id_, parameters[0]), permissions);
  }

 private:
  int message_id_;

  DISALLOW_COPY_AND_ASSIGN(SingleParameterFormatter);
};

// Adds each parameter to a growing list, with the given |root_message_id| as
// the message at the top of the list.
class SimpleListFormatter : public ChromePermissionMessageFormatter {
 public:
  explicit SimpleListFormatter(int root_message_id)
      : root_message_id_(root_message_id) {}
  ~SimpleListFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    DCHECK(permissions.size() > 0);
    return PermissionMessage(l10n_util::GetStringUTF16(root_message_id_),
                             permissions,
                             permissions.GetAllPermissionParameters());
  }

 private:
  int root_message_id_;

  DISALLOW_COPY_AND_ASSIGN(SimpleListFormatter);
};

// Creates a space-separated list of permissions with the given PermissionID.
// The list is inserted into the messages with the given IDs: one for the case
// where there is a single permission, and the other for the case where there
// are multiple.
// TODO(sashab): Extend this to pluralize correctly in all languages.
class SpaceSeparatedListFormatter : public ChromePermissionMessageFormatter {
 public:
  SpaceSeparatedListFormatter(int message_id_for_one_host,
                              int message_id_for_multiple_hosts)
      : message_id_for_one_host_(message_id_for_one_host),
        message_id_for_multiple_hosts_(message_id_for_multiple_hosts) {}
  ~SpaceSeparatedListFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    DCHECK(permissions.size() > 0);
    std::vector<base::string16> hostnames =
        permissions.GetAllPermissionParameters();
    base::string16 hosts_string =
        base::JoinString(hostnames, base::ASCIIToUTF16(" "));
    return PermissionMessage(
        l10n_util::GetStringFUTF16(hostnames.size() == 1
                                       ? message_id_for_one_host_
                                       : message_id_for_multiple_hosts_,
                                   hosts_string),
        permissions);
  }

 private:
  int message_id_for_one_host_;
  int message_id_for_multiple_hosts_;

  DISALLOW_COPY_AND_ASSIGN(SpaceSeparatedListFormatter);
};

// Creates a list of host permissions. If there are 1-3 hosts, they are inserted
// directly into a message with a given ID. If there are more than 3, they are
// displayed as list of submessages instead.
class HostListFormatter : public ChromePermissionMessageFormatter {
 public:
  HostListFormatter(int message_id_for_one_host,
                    int message_id_for_two_hosts,
                    int message_id_for_three_hosts,
                    int message_id_for_many_hosts)
      : message_id_for_one_host_(message_id_for_one_host),
        message_id_for_two_hosts_(message_id_for_two_hosts),
        message_id_for_three_hosts_(message_id_for_three_hosts),
        message_id_for_many_hosts_(message_id_for_many_hosts) {}
  ~HostListFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    DCHECK(!permissions.empty());
    std::vector<base::string16> hostnames =
        GetHostMessages(permissions.GetAllPermissionParameters());
    int message_id = message_id_for_hosts(hostnames.size());
    if (hostnames.size() <= kMaxHostsInMainMessage) {
      return PermissionMessage(
          l10n_util::GetStringFUTF16(message_id, hostnames, nullptr),
          permissions);
    }
    return PermissionMessage(l10n_util::GetStringUTF16(message_id), permissions,
                             hostnames);
  }

 private:
  int message_id_for_hosts(int number_of_hosts) const {
    switch (number_of_hosts) {
      case 1:
        return message_id_for_one_host_;
      case 2:
        return message_id_for_two_hosts_;
      case 3:
        return message_id_for_three_hosts_;
      default:
        return message_id_for_many_hosts_;
    }
  }

  std::vector<base::string16> GetHostMessages(
      const std::vector<base::string16>& hosts) const {
    int msg_id = hosts.size() <= kMaxHostsInMainMessage
                     ? IDS_EXTENSION_PROMPT_WARNING_HOST_AND_SUBDOMAIN
                     : IDS_EXTENSION_PROMPT_WARNING_HOST_AND_SUBDOMAIN_LIST;
    std::vector<base::string16> messages;
    for (const base::string16& host : hosts) {
      messages.push_back(
          host[0] == '*' && host[1] == '.'
              ? l10n_util::GetStringFUTF16(msg_id, host.substr(2))
              : host);
    }
    return messages;
  }

  static const int kMaxHostsInMainMessage = 3;

  int message_id_for_one_host_;
  int message_id_for_two_hosts_;
  int message_id_for_three_hosts_;
  int message_id_for_many_hosts_;

  DISALLOW_COPY_AND_ASSIGN(HostListFormatter);
};

class USBDevicesFormatter : public ChromePermissionMessageFormatter {
 public:
  USBDevicesFormatter() {}
  ~USBDevicesFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    DCHECK(permissions.size() > 0);
    return permissions.size() == 1 ? GetItemMessage(permissions)
                                   : GetMultiItemMessage(permissions);
  }

 private:
  PermissionMessage GetItemMessage(const PermissionIDSet& permissions) const {
    DCHECK(permissions.size() == 1);
    const PermissionID& permission = *permissions.begin();
    base::string16 msg;
    switch (permission.id()) {
      case APIPermission::kUsbDevice:
        msg = l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE, permission.parameter());
        break;
      case APIPermission::kUsbDeviceUnknownProduct:
        msg = l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_PRODUCT,
            permission.parameter());
        break;
      case APIPermission::kUsbDeviceUnknownVendor:
        msg = l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_VENDOR);
        break;
      default:
        NOTREACHED();
    }
    return PermissionMessage(msg, permissions);
  }

  PermissionMessage GetMultiItemMessage(
      const PermissionIDSet& permissions) const {
    DCHECK(permissions.size() > 1);
    // Put all the individual items into submessages.
    std::vector<base::string16> submessages =
        permissions.GetAllPermissionsWithID(APIPermission::kUsbDevice)
            .GetAllPermissionParameters();
    std::vector<base::string16> vendors =
        permissions.GetAllPermissionsWithID(
                       APIPermission::kUsbDeviceUnknownProduct)
            .GetAllPermissionParameters();
    for (const base::string16& vendor : vendors) {
      submessages.push_back(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST_ITEM_UNKNOWN_PRODUCT,
          vendor));
    }
    if (permissions.ContainsID(APIPermission::kUsbDeviceUnknownVendor)) {
      submessages.push_back(l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST_ITEM_UNKNOWN_VENDOR));
    }

    return PermissionMessage(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST),
        permissions, submessages);
  }

  DISALLOW_COPY_AND_ASSIGN(USBDevicesFormatter);
};

}  // namespace

ChromePermissionMessageRule::ChromePermissionMessageRule(
    int message_id,
    const std::initializer_list<APIPermission::ID>& required,
    const std::initializer_list<APIPermission::ID>& optional)
    : ChromePermissionMessageRule(
          new DefaultPermissionMessageFormatter(message_id),
          required,
          optional) {}

ChromePermissionMessageRule::ChromePermissionMessageRule(
    ChromePermissionMessageFormatter* formatter,
    const std::initializer_list<APIPermission::ID>& required,
    const std::initializer_list<APIPermission::ID>& optional)
    : required_permissions_(required),
      optional_permissions_(optional),
      formatter_(formatter) {
  DCHECK(!required_permissions_.empty());
}

ChromePermissionMessageRule::ChromePermissionMessageRule(
    const ChromePermissionMessageRule& other) = default;

ChromePermissionMessageRule::~ChromePermissionMessageRule() {
}

std::set<APIPermission::ID> ChromePermissionMessageRule::required_permissions()
    const {
  return required_permissions_;
}
std::set<APIPermission::ID> ChromePermissionMessageRule::optional_permissions()
    const {
  return optional_permissions_;
}

std::set<APIPermission::ID> ChromePermissionMessageRule::all_permissions()
    const {
  return base::STLSetUnion<std::set<APIPermission::ID>>(required_permissions(),
                                                        optional_permissions());
}

PermissionMessage ChromePermissionMessageRule::GetPermissionMessage(
    const PermissionIDSet& permissions) const {
  return formatter_->GetPermissionMessage(permissions);
}

// static
std::vector<ChromePermissionMessageRule>
ChromePermissionMessageRule::GetAllRules() {
  // The rules for generating messages from permissions. Any new rules should be
  // added directly to this list, not elsewhere in the code, so that all the
  // logic of generating and coalescing permission messages happens here.
  //
  // Each rule has 3 components:
  // 1. The message itself
  // 2. The permissions that need to be present for the message to appear
  // 3. Permissions that, if present, also contribute to the message, but do not
  //    form the message on their own
  //
  // Rules are applied in precedence order: rules that come first consume
  // permissions (both required and optional) so they can not be used in later
  // rules.
  // NOTE: The order of this list matters - be careful when adding new rules!
  // If unsure, add them near related rules and add tests to
  // permission_message_combinations_unittest.cc (or elsewhere) to ensure your
  // messages are being generated/coalesced correctly.
  //
  // Rules are not transitive: This means that if the kTab permission 'absorbs'
  // (suppresses) the messages for kTopSites and kFavicon, and the kHistory
  // permission suppresses kTab, be careful to also add kTopSites and kFavicon
  // to the kHistory absorb list. Ideally, the rules system should be simple
  // enough that rules like this should not occur; the visibility of the rules
  // system should allow us to design a system that is simple enough to explain
  // yet powerful enough to encapsulate all the messages we want to display.
  ChromePermissionMessageRule rules_arr[] = {
      // Full access permission messages.
      {IDS_EXTENSION_PROMPT_WARNING_DEBUGGER, {APIPermission::kDebugger}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
       {APIPermission::kFullAccess},
       {APIPermission::kDeclarativeWebRequest, APIPermission::kFavicon,
        APIPermission::kHostsAll, APIPermission::kHostsAllReadOnly,
        APIPermission::kProcesses, APIPermission::kTab,
        APIPermission::kTopSites, APIPermission::kWebNavigation,
        APIPermission::kDeclarativeNetRequest}},

      // Hosts permission messages.
      // Full host access already allows DeclarativeWebRequest, reading the list
      // of most frequently visited sites, and tab access.
      // The warning message for declarativeWebRequest permissions speaks about
      // blocking parts of pages, which is a subset of what the "<all_urls>"
      // access allows. Therefore we display only the "<all_urls>" warning
      // message if both permissions are required.
      {IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS,
       {APIPermission::kHostsAll},
       {APIPermission::kDeclarativeWebRequest, APIPermission::kFavicon,
        APIPermission::kHostsAllReadOnly, APIPermission::kHostReadOnly,
        APIPermission::kHostReadWrite, APIPermission::kProcesses,
        APIPermission::kTab, APIPermission::kTopSites,
        APIPermission::kWebNavigation, APIPermission::kDeclarativeNetRequest}},
      {IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS_READ_ONLY,
       {APIPermission::kHostsAllReadOnly},
       {APIPermission::kFavicon, APIPermission::kHostReadOnly,
        APIPermission::kProcesses, APIPermission::kTab,
        APIPermission::kTopSites, APIPermission::kWebNavigation}},

      {new HostListFormatter(IDS_EXTENSION_PROMPT_WARNING_1_HOST,
                             IDS_EXTENSION_PROMPT_WARNING_2_HOSTS,
                             IDS_EXTENSION_PROMPT_WARNING_3_HOSTS,
                             IDS_EXTENSION_PROMPT_WARNING_HOSTS_LIST),
       {APIPermission::kHostReadWrite},
       {}},
      {new HostListFormatter(IDS_EXTENSION_PROMPT_WARNING_1_HOST_READ_ONLY,
                             IDS_EXTENSION_PROMPT_WARNING_2_HOSTS_READ_ONLY,
                             IDS_EXTENSION_PROMPT_WARNING_3_HOSTS_READ_ONLY,
                             IDS_EXTENSION_PROMPT_WARNING_HOSTS_LIST_READ_ONLY),
       {APIPermission::kHostReadOnly},
       {}},

      // New tab page permission is fairly highly used so rank it quite highly.
      // Nothing should subsume it.
      {IDS_EXTENSION_PROMPT_WARNING_NEW_TAB_PAGE_OVERRIDE,
       {APIPermission::kNewTabPageOverride},
       {}},

      // History-related permission messages.
      // History already allows reading favicons, tab access and accessing the
      // list of most frequently visited sites.
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE_AND_SESSIONS,
       {APIPermission::kHistory, APIPermission::kSessions},
       {APIPermission::kFavicon, APIPermission::kProcesses, APIPermission::kTab,
        APIPermission::kTopSites, APIPermission::kWebNavigation}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ_AND_SESSIONS,
       {APIPermission::kTab, APIPermission::kSessions},
       {APIPermission::kFavicon, APIPermission::kProcesses,
        APIPermission::kTopSites, APIPermission::kWebNavigation}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE,
       {APIPermission::kHistory},
       {APIPermission::kFavicon, APIPermission::kProcesses, APIPermission::kTab,
        APIPermission::kTopSites, APIPermission::kWebNavigation}},
      // Note: kSessions allows reading history from other devices only if kTab
      // is also present. Therefore, there are no _AND_SESSIONS versions of
      // the other rules that generate the HISTORY_READ warning.
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermission::kTab},
       {APIPermission::kFavicon, APIPermission::kProcesses,
        APIPermission::kTopSites, APIPermission::kWebNavigation}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermission::kProcesses},
       {APIPermission::kFavicon, APIPermission::kTopSites,
        APIPermission::kWebNavigation}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermission::kWebNavigation},
       {APIPermission::kFavicon, APIPermission::kTopSites}},
      {IDS_EXTENSION_PROMPT_WARNING_FAVICON, {APIPermission::kFavicon}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_TOPSITES, {APIPermission::kTopSites}, {}},

      {IDS_EXTENSION_PROMPT_WARNING_DECLARATIVE_WEB_REQUEST,
       {APIPermission::kDeclarativeWebRequest},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DECLARATIVE_NET_REQUEST,
       {APIPermission::kDeclarativeNetRequest},
       {}},

      // Messages generated by the sockets permission.
      {IDS_EXTENSION_PROMPT_WARNING_SOCKET_ANY_HOST,
       {APIPermission::kSocketAnyHost},
       {APIPermission::kSocketDomainHosts,
        APIPermission::kSocketSpecificHosts}},
      {new SpaceSeparatedListFormatter(
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_HOSTS_IN_DOMAIN,
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_HOSTS_IN_DOMAINS),
       {APIPermission::kSocketDomainHosts},
       {}},
      {new SpaceSeparatedListFormatter(
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_SPECIFIC_HOST,
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_SPECIFIC_HOSTS),
       {APIPermission::kSocketSpecificHosts},
       {}},

      // Devices-related messages.
      // USB Device Permission rules. Think of these three rules as a single one
      // that applies when any of the three kUsb* IDs is there, and pulls them
      // all into a single formatter.
      {new USBDevicesFormatter,
       {APIPermission::kUsbDevice},
       {APIPermission::kUsbDeviceUnknownProduct,
        APIPermission::kUsbDeviceUnknownVendor}},
      {new USBDevicesFormatter,
       {APIPermission::kUsbDeviceUnknownProduct},
       {APIPermission::kUsbDeviceUnknownVendor}},
      {new USBDevicesFormatter, {APIPermission::kUsbDeviceUnknownVendor}, {}},
      // Access to users' devices should provide a single warning message
      // specifying the transport method used; serial and/or Bluetooth.
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_SERIAL,
       {APIPermission::kBluetooth, APIPermission::kSerial},
       {APIPermission::kBluetoothDevices}},
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH,
       {APIPermission::kBluetooth},
       {APIPermission::kBluetoothDevices}},
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_DEVICES,
       {APIPermission::kBluetoothDevices},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_PRIVATE,
       {APIPermission::kBluetoothPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SERIAL, {APIPermission::kSerial}, {}},
      // Universal 2nd Factor devices.
      {IDS_EXTENSION_PROMPT_WARNING_U2F_DEVICES,
       {APIPermission::kU2fDevices},
       {}},
      // Notifications.
      {IDS_EXTENSION_PROMPT_WARNING_NOTIFICATIONS,
       {APIPermission::kNotifications},
       {}},

      // Accessibility features.
      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_READ_MODIFY,
       {APIPermission::kAccessibilityFeaturesModify,
        APIPermission::kAccessibilityFeaturesRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_MODIFY,
       {APIPermission::kAccessibilityFeaturesModify},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_READ,
       {APIPermission::kAccessibilityFeaturesRead},
       {}},

      // Media galleries permissions. We don't have strings for every possible
      // combination, e.g. we don't bother with a special string for "write, but
      // not read" - just show the "read and write" string instead, etc.
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE_DELETE,
       {APIPermission::kMediaGalleriesAllGalleriesCopyTo,
        APIPermission::kMediaGalleriesAllGalleriesDelete},
       {APIPermission::kMediaGalleriesAllGalleriesRead}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE,
       {APIPermission::kMediaGalleriesAllGalleriesCopyTo},
       {APIPermission::kMediaGalleriesAllGalleriesRead}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_DELETE,
       {APIPermission::kMediaGalleriesAllGalleriesDelete},
       {APIPermission::kMediaGalleriesAllGalleriesRead}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ,
       {APIPermission::kMediaGalleriesAllGalleriesRead},
       {}},

      // File system permissions. We only have permission strings for directory
      // access, and show a different message for read-only directory access
      // versus writable directory access. We don't warn for write-only access,
      // since the user must select the file and the chooser is considered
      // sufficient warning.
      {IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE_DIRECTORY,
       {APIPermission::kFileSystemWrite, APIPermission::kFileSystemDirectory},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_DIRECTORY,
       {APIPermission::kFileSystemDirectory},
       {}},

      // Video and audio capture.
      {IDS_EXTENSION_PROMPT_WARNING_AUDIO_AND_VIDEO_CAPTURE,
       {APIPermission::kAudioCapture, APIPermission::kVideoCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
       {APIPermission::kAudioCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
       {APIPermission::kVideoCapture},
       {}},

      // Network-related permissions.
      {IDS_EXTENSION_PROMPT_WARNING_NETWORKING_PRIVATE,
       {APIPermission::kNetworkingOnc},
       // Adding networkingPrivate as an optional permission for this rule so
       // the permission is removed from the available permission set when the
       // next rule (for networkingPrivate permission) is considered - without
       // this, IDS_EXTENSION_PROMPT_WARNING_NETWORK_PRIVATE would be duplicated
       // for manifests that have both networking.onc and networkingPrivate
       // permission.
       {APIPermission::kNetworkingPrivate}},
      {IDS_EXTENSION_PROMPT_WARNING_NETWORKING_PRIVATE,
       {APIPermission::kNetworkingPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_NETWORKING_CONFIG,
       {APIPermission::kNetworkingConfig},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_NETWORK_STATE,
       {APIPermission::kNetworkState},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_VPN, {APIPermission::kVpnProvider}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_WEB_CONNECTABLE,
       {APIPermission::kWebConnectable},
       {}},
      {new SingleParameterFormatter(
           IDS_EXTENSION_PROMPT_WARNING_HOME_PAGE_SETTING_OVERRIDE),
       {APIPermission::kHomepage},
       {}},
      {new SingleParameterFormatter(
           IDS_EXTENSION_PROMPT_WARNING_SEARCH_SETTINGS_OVERRIDE),
       {APIPermission::kSearchProvider},
       {}},
      {new SingleParameterFormatter(
           IDS_EXTENSION_PROMPT_WARNING_START_PAGE_SETTING_OVERRIDE),
       {APIPermission::kStartupPages},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS,
       {APIPermission::kBookmark},
       {APIPermission::kOverrideBookmarksUI}},
      {IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD_READWRITE,
       {APIPermission::kClipboardRead, APIPermission::kClipboardWrite},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
       {APIPermission::kClipboardRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD_WRITE,
       {APIPermission::kClipboardWrite},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DESKTOP_CAPTURE,
       {APIPermission::kDesktopCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS, {APIPermission::kDownloads}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS_OPEN,
       {APIPermission::kDownloadsOpen},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_IDENTITY_EMAIL,
       {APIPermission::kIdentityEmail},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
       {APIPermission::kGeolocation},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_SYSTEM_STORAGE,
       {APIPermission::kSystemStorage},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
       {APIPermission::kContentSettings},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOCUMENT_SCAN,
       {APIPermission::kDocumentScan},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_INPUT, {APIPermission::kInput}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
       {APIPermission::kManagement},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_MDNS, {APIPermission::kMDns}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_NATIVE_MESSAGING,
       {APIPermission::kNativeMessaging},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_PRIVACY, {APIPermission::kPrivacy}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_SIGNED_IN_DEVICES,
       {APIPermission::kSignedInDevices},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SYNCFILESYSTEM,
       {APIPermission::kSyncFileSystem},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_TTS_ENGINE,
       {APIPermission::kTtsEngine},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_WALLPAPER, {APIPermission::kWallpaper}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_PLATFORMKEYS,
       {APIPermission::kPlatformKeys},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CERTIFICATEPROVIDER,
       {APIPermission::kCertificateProvider},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_SCREENLOCK_PRIVATE,
       {APIPermission::kScreenlockPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ACTIVITY_LOG_PRIVATE,
       {APIPermission::kActivityLogPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_MUSIC_MANAGER_PRIVATE,
       {APIPermission::kMusicManagerPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SETTINGS_PRIVATE,
       {APIPermission::kSettingsPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_AUTOFILL_PRIVATE,
       {APIPermission::kAutofillPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_PASSWORDS_PRIVATE,
       {APIPermission::kPasswordsPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_USERS_PRIVATE,
       {APIPermission::kUsersPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DISPLAY_SOURCE,
       {APIPermission::kDisplaySource},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_HARDWARE_PLATFORM,
       {APIPermission::kEnterpriseHardwarePlatform},
       {}},
  };

  return std::vector<ChromePermissionMessageRule>(
      rules_arr, rules_arr + arraysize(rules_arr));
}

}  // namespace extensions
