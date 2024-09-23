// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_rules.h"

#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_signals/core/common/signals_features.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

// The default formatter for a permission message. Simply displays the message
// with the given ID.
class DefaultPermissionMessageFormatter
    : public ChromePermissionMessageFormatter {
 public:
  explicit DefaultPermissionMessageFormatter(int message_id)
      : message_id_(message_id) {}

  DefaultPermissionMessageFormatter(const DefaultPermissionMessageFormatter&) =
      delete;
  DefaultPermissionMessageFormatter& operator=(
      const DefaultPermissionMessageFormatter&) = delete;

  ~DefaultPermissionMessageFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    return PermissionMessage(l10n_util::GetStringUTF16(message_id_),
                             permissions);
  }

 private:
  int message_id_;
};

// A formatter that substitutes the parameter into the message using string
// formatting.
// NOTE: Only one permission with the given ID is substituted using this rule.
class SingleParameterFormatter : public ChromePermissionMessageFormatter {
 public:
  explicit SingleParameterFormatter(int message_id) : message_id_(message_id) {}

  SingleParameterFormatter(const SingleParameterFormatter&) = delete;
  SingleParameterFormatter& operator=(const SingleParameterFormatter&) = delete;

  ~SingleParameterFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    DCHECK(permissions.size() > 0);
    std::vector<std::u16string> parameters =
        permissions.GetAllPermissionParameters();
    DCHECK_EQ(1U, parameters.size())
        << "Only one message with each ID can be parameterized.";
    return PermissionMessage(
        l10n_util::GetStringFUTF16(message_id_, parameters[0]), permissions);
  }

 private:
  int message_id_;
};

// Adds each parameter to a growing list, with the given |root_message_id| as
// the message at the top of the list.
class SimpleListFormatter : public ChromePermissionMessageFormatter {
 public:
  explicit SimpleListFormatter(int root_message_id)
      : root_message_id_(root_message_id) {}

  SimpleListFormatter(const SimpleListFormatter&) = delete;
  SimpleListFormatter& operator=(const SimpleListFormatter&) = delete;

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

  SpaceSeparatedListFormatter(const SpaceSeparatedListFormatter&) = delete;
  SpaceSeparatedListFormatter& operator=(const SpaceSeparatedListFormatter&) =
      delete;

  ~SpaceSeparatedListFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    DCHECK(permissions.size() > 0);
    std::vector<std::u16string> hostnames =
        permissions.GetAllPermissionParameters();
    std::u16string hosts_string = base::JoinString(hostnames, u" ");
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

  HostListFormatter(const HostListFormatter&) = delete;
  HostListFormatter& operator=(const HostListFormatter&) = delete;

  ~HostListFormatter() override {}

  PermissionMessage GetPermissionMessage(
      const PermissionIDSet& permissions) const override {
    DCHECK(!permissions.empty());
    std::vector<std::u16string> hostnames =
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

  std::vector<std::u16string> GetHostMessages(
      const std::vector<std::u16string>& hosts) const {
    int msg_id = hosts.size() <= kMaxHostsInMainMessage
                     ? IDS_EXTENSION_PROMPT_WARNING_HOST_AND_SUBDOMAIN
                     : IDS_EXTENSION_PROMPT_WARNING_HOST_AND_SUBDOMAIN_LIST;
    std::vector<std::u16string> messages;
    for (const std::u16string& host : hosts) {
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
};

class USBDevicesFormatter : public ChromePermissionMessageFormatter {
 public:
  USBDevicesFormatter() {}

  USBDevicesFormatter(const USBDevicesFormatter&) = delete;
  USBDevicesFormatter& operator=(const USBDevicesFormatter&) = delete;

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
    std::u16string msg;
    switch (permission.id()) {
      case APIPermissionID::kUsbDevice:
        msg = l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE, permission.parameter());
        break;
      case APIPermissionID::kUsbDeviceUnknownProduct:
        msg = l10n_util::GetStringFUTF16(
            IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_PRODUCT,
            permission.parameter());
        break;
      case APIPermissionID::kUsbDeviceUnknownVendor:
        msg = l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_VENDOR);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    return PermissionMessage(msg, permissions);
  }

  PermissionMessage GetMultiItemMessage(
      const PermissionIDSet& permissions) const {
    DCHECK(permissions.size() > 1);
    // Put all the individual items into submessages.
    std::vector<std::u16string> submessages =
        permissions.GetAllPermissionsWithID(APIPermissionID::kUsbDevice)
            .GetAllPermissionParameters();
    std::vector<std::u16string> vendors =
        permissions
            .GetAllPermissionsWithID(APIPermissionID::kUsbDeviceUnknownProduct)
            .GetAllPermissionParameters();
    for (const std::u16string& vendor : vendors) {
      submessages.push_back(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST_ITEM_UNKNOWN_PRODUCT,
          vendor));
    }
    if (permissions.ContainsID(APIPermissionID::kUsbDeviceUnknownVendor)) {
      submessages.push_back(l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST_ITEM_UNKNOWN_VENDOR));
    }

    return PermissionMessage(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST),
        permissions, submessages);
  }
};

int GetEnterpriseReportingPrivatePermissionMessageId() {
  if (!base::FeatureList::IsEnabled(
          enterprise_signals::features::kNewEvSignalsEnabled)) {
    return IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_REPORTING_PRIVATE;
  }
#if BUILDFLAG(IS_WIN)
  return IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_REPORTING_PRIVATE_ENABLED_WIN;
#elif BUILDFLAG(IS_LINUX) or BUILDFLAG(IS_MAC)
  return IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_REPORTING_PRIVATE_ENABLED_LINUX_AND_MACOS;
#else
  return IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_REPORTING_PRIVATE;
#endif
}

}  // namespace

ChromePermissionMessageRule::ChromePermissionMessageRule(
    int message_id,
    const std::initializer_list<APIPermissionID>& required,
    const std::initializer_list<APIPermissionID>& optional)
    : ChromePermissionMessageRule(
          std::make_unique<DefaultPermissionMessageFormatter>(message_id),
          required,
          optional) {}

ChromePermissionMessageRule::ChromePermissionMessageRule(
    std::unique_ptr<ChromePermissionMessageFormatter> formatter,
    const std::initializer_list<APIPermissionID>& required,
    const std::initializer_list<APIPermissionID>& optional)
    : required_permissions_(required),
      optional_permissions_(optional),
      formatter_(std::move(formatter)) {
  DCHECK(!required_permissions_.empty());
}

ChromePermissionMessageRule::ChromePermissionMessageRule(
    ChromePermissionMessageRule&& other) = default;

ChromePermissionMessageRule& ChromePermissionMessageRule::operator=(
    ChromePermissionMessageRule&& other) = default;

ChromePermissionMessageRule::~ChromePermissionMessageRule() {
}

std::set<APIPermissionID> ChromePermissionMessageRule::required_permissions()
    const {
  return required_permissions_;
}
std::set<APIPermissionID> ChromePermissionMessageRule::optional_permissions()
    const {
  return optional_permissions_;
}

std::set<APIPermissionID> ChromePermissionMessageRule::all_permissions() const {
  return base::STLSetUnion<std::set<APIPermissionID>>(required_permissions(),
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
      {IDS_EXTENSION_PROMPT_WARNING_DEBUGGER, {APIPermissionID::kDebugger}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
       {APIPermissionID::kFullAccess},
       {APIPermissionID::kDeclarativeWebRequest,
        APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kHostsAll,
        APIPermissionID::kHostsAllReadOnly, APIPermissionID::kProcesses,
        APIPermissionID::kTab, APIPermissionID::kTopSites,
        APIPermissionID::kWebNavigation,
        APIPermissionID::kDeclarativeNetRequest}},

      // Hosts permission messages.
      // Full host access already allows DeclarativeWebRequest, reading the list
      // of most frequently visited sites, and tab access.
      // The warning message for declarativeWebRequest permissions speaks about
      // blocking parts of pages, which is a subset of what the "<all_urls>"
      // access allows. Therefore we display only the "<all_urls>" warning
      // message if both permissions are required.
      {IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS,
       {APIPermissionID::kHostsAll},
       {APIPermissionID::kDeclarativeWebRequest,
        APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kHostsAllReadOnly,
        APIPermissionID::kHostReadOnly, APIPermissionID::kHostReadWrite,
        APIPermissionID::kProcesses, APIPermissionID::kTab,
        APIPermissionID::kTopSites, APIPermissionID::kWebNavigation,
        APIPermissionID::kDeclarativeNetRequest,
        APIPermissionID::kWebAuthenticationProxy}},
      {IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS,
       {APIPermissionID::kWebAuthenticationProxy},
       {APIPermissionID::kDeclarativeWebRequest,
        APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kHostsAllReadOnly,
        APIPermissionID::kHostReadOnly, APIPermissionID::kHostReadWrite,
        APIPermissionID::kProcesses, APIPermissionID::kTab,
        APIPermissionID::kTopSites, APIPermissionID::kWebNavigation,
        APIPermissionID::kDeclarativeNetRequest}},
      {IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS_READ_ONLY,
       {APIPermissionID::kHostsAllReadOnly},
       {APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kHostReadOnly,
        APIPermissionID::kProcesses, APIPermissionID::kTab,
        APIPermissionID::kTopSites, APIPermissionID::kWebNavigation}},

      {std::make_unique<HostListFormatter>(
           IDS_EXTENSION_PROMPT_WARNING_1_HOST,
           IDS_EXTENSION_PROMPT_WARNING_2_HOSTS,
           IDS_EXTENSION_PROMPT_WARNING_3_HOSTS,
           IDS_EXTENSION_PROMPT_WARNING_HOSTS_LIST),
       {APIPermissionID::kHostReadWrite},
       {}},
      {std::make_unique<HostListFormatter>(
           IDS_EXTENSION_PROMPT_WARNING_1_HOST_READ_ONLY,
           IDS_EXTENSION_PROMPT_WARNING_2_HOSTS_READ_ONLY,
           IDS_EXTENSION_PROMPT_WARNING_3_HOSTS_READ_ONLY,
           IDS_EXTENSION_PROMPT_WARNING_HOSTS_LIST_READ_ONLY),
       {APIPermissionID::kHostReadOnly},
       {}},

      // New tab page permission is fairly highly used so rank it quite highly.
      // Nothing should subsume it.
      {IDS_EXTENSION_PROMPT_WARNING_NEW_TAB_PAGE_OVERRIDE,
       {APIPermissionID::kNewTabPageOverride},
       {}},

      // Video and audio capture.
      {IDS_EXTENSION_PROMPT_WARNING_AUDIO_AND_VIDEO_CAPTURE,
       {APIPermissionID::kAudioCapture, APIPermissionID::kVideoCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
       {APIPermissionID::kAudioCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
       {APIPermissionID::kVideoCapture},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_SPEECH_RECOGNITION,
       {APIPermissionID::kSpeechRecognitionPrivate},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
       {APIPermissionID::kGeolocation},
       {}},

      // History-related permission messages.
      // History already allows reading favicons, tab access and accessing the
      // list of most frequently visited sites.
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE_ON_ALL_DEVICES,
       {APIPermissionID::kHistory},
       {APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kProcesses,
        APIPermissionID::kTab, APIPermissionID::kTopSites,
        APIPermissionID::kWebNavigation}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ_ON_ALL_DEVICES,
       {APIPermissionID::kTab, APIPermissionID::kSessions},
       {APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kProcesses,
        APIPermissionID::kTopSites, APIPermissionID::kWebNavigation}},
      // Note: kSessions allows reading history from other devices only if kTab
      // is also present. Therefore, there are no _ON_ALL_DEVICES versions of
      // the other rules that generate the HISTORY_READ warning.
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermissionID::kTab},
       {APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kProcesses,
        APIPermissionID::kTopSites, APIPermissionID::kWebNavigation}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermissionID::kProcesses},
       {APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kTopSites,
        APIPermissionID::kWebNavigation}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermissionID::kWebNavigation},
       {APIPermissionID::kDeclarativeNetRequestFeedback,
        APIPermissionID::kFavicon, APIPermissionID::kTopSites}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermissionID::kDeclarativeNetRequestFeedback},
       {APIPermissionID::kFavicon, APIPermissionID::kTopSites}},
      {IDS_EXTENSION_PROMPT_WARNING_FAVICON, {APIPermissionID::kFavicon}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_TOPSITES, {APIPermissionID::kTopSites}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_PRINTING, {APIPermissionID::kPrinting}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_PRINTING_METRICS,
       {APIPermissionID::kPrintingMetrics},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_DECLARATIVE_WEB_REQUEST,
       {APIPermissionID::kDeclarativeWebRequest},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DECLARATIVE_NET_REQUEST,
       {APIPermissionID::kDeclarativeNetRequest},
       {}},

      // Messages generated by the sockets permission.
      {IDS_EXTENSION_PROMPT_WARNING_SOCKET_ANY_HOST,
       {APIPermissionID::kSocketAnyHost},
       {APIPermissionID::kSocketDomainHosts,
        APIPermissionID::kSocketSpecificHosts}},
      {std::make_unique<SpaceSeparatedListFormatter>(
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_HOSTS_IN_DOMAIN,
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_HOSTS_IN_DOMAINS),
       {APIPermissionID::kSocketDomainHosts},
       {}},
      {std::make_unique<SpaceSeparatedListFormatter>(
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_SPECIFIC_HOST,
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_SPECIFIC_HOSTS),
       {APIPermissionID::kSocketSpecificHosts},
       {}},

      // Devices-related messages.
      // USB Device Permission rules. Think of these three rules as a single one
      // that applies when any of the three kUsb* IDs is there, and pulls them
      // all into a single formatter.
      {std::make_unique<USBDevicesFormatter>(),
       {APIPermissionID::kUsbDevice},
       {APIPermissionID::kUsbDeviceUnknownProduct,
        APIPermissionID::kUsbDeviceUnknownVendor}},
      {std::make_unique<USBDevicesFormatter>(),
       {APIPermissionID::kUsbDeviceUnknownProduct},
       {APIPermissionID::kUsbDeviceUnknownVendor}},
      {std::make_unique<USBDevicesFormatter>(),
       {APIPermissionID::kUsbDeviceUnknownVendor},
       {}},
      // Access to users' devices should provide a single warning message
      // specifying the transport method used; serial and/or Bluetooth.
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_SERIAL,
       {APIPermissionID::kBluetooth, APIPermissionID::kSerial},
       {APIPermissionID::kBluetoothDevices}},
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH,
       {APIPermissionID::kBluetooth},
       {APIPermissionID::kBluetoothDevices}},
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_DEVICES,
       {APIPermissionID::kBluetoothDevices},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_PRIVATE,
       {APIPermissionID::kBluetoothPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SERIAL, {APIPermissionID::kSerial}, {}},
      // Universal 2nd Factor devices.
      {IDS_EXTENSION_PROMPT_WARNING_U2F_DEVICES,
       {APIPermissionID::kU2fDevices},
       {}},
      // Notifications.
      {IDS_EXTENSION_PROMPT_WARNING_NOTIFICATIONS,
       {APIPermissionID::kNotifications},
       {}},

      // Accessibility features.
      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_READ_MODIFY,
       {APIPermissionID::kAccessibilityFeaturesModify,
        APIPermissionID::kAccessibilityFeaturesRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_MODIFY,
       {APIPermissionID::kAccessibilityFeaturesModify},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_READ,
       {APIPermissionID::kAccessibilityFeaturesRead},
       {}},

      // Media galleries permissions. We don't have strings for every possible
      // combination, e.g. we don't bother with a special string for "write, but
      // not read" - just show the "read and write" string instead, etc.
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE_DELETE,
       {APIPermissionID::kMediaGalleriesAllGalleriesCopyTo,
        APIPermissionID::kMediaGalleriesAllGalleriesDelete},
       {APIPermissionID::kMediaGalleriesAllGalleriesRead}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE,
       {APIPermissionID::kMediaGalleriesAllGalleriesCopyTo},
       {APIPermissionID::kMediaGalleriesAllGalleriesRead}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_DELETE,
       {APIPermissionID::kMediaGalleriesAllGalleriesDelete},
       {APIPermissionID::kMediaGalleriesAllGalleriesRead}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ,
       {APIPermissionID::kMediaGalleriesAllGalleriesRead},
       {}},

      // File system permissions. We only have permission strings for directory
      // access, and show a different message for read-only directory access
      // versus writable directory access. We don't warn for write-only access,
      // since the user must select the file and the chooser is considered
      // sufficient warning.
      {IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE_DIRECTORY,
       {APIPermissionID::kFileSystemWrite,
        APIPermissionID::kFileSystemDirectory},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_DIRECTORY,
       {APIPermissionID::kFileSystemDirectory},
       {}},

      // Network-related permissions.
      {IDS_EXTENSION_PROMPT_WARNING_NETWORKING_PRIVATE,
       {APIPermissionID::kNetworkingOnc},
       // Adding networkingPrivate as an optional permission for this rule so
       // the permission is removed from the available permission set when the
       // next rule (for networkingPrivate permission) is considered - without
       // this, IDS_EXTENSION_PROMPT_WARNING_NETWORK_PRIVATE would be duplicated
       // for manifests that have both networking.onc and networkingPrivate
       // permission.
       {APIPermissionID::kNetworkingPrivate}},
      {IDS_EXTENSION_PROMPT_WARNING_NETWORKING_PRIVATE,
       {APIPermissionID::kNetworkingPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_NETWORK_STATE,
       {APIPermissionID::kNetworkState},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_VPN, {APIPermissionID::kVpnProvider}, {}},
      {std::make_unique<SingleParameterFormatter>(
           IDS_EXTENSION_PROMPT_WARNING_HOME_PAGE_SETTING_OVERRIDE),
       {APIPermissionID::kHomepage},
       {}},
      {std::make_unique<SingleParameterFormatter>(
           IDS_EXTENSION_PROMPT_WARNING_SEARCH_SETTINGS_OVERRIDE),
       {APIPermissionID::kSearchProvider},
       {}},
      {std::make_unique<SingleParameterFormatter>(
           IDS_EXTENSION_PROMPT_WARNING_START_PAGE_SETTING_OVERRIDE),
       {APIPermissionID::kStartupPages},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS,
       {APIPermissionID::kBookmark},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_READING_LIST,
       {APIPermissionID::kReadingList},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD_READWRITE,
       {APIPermissionID::kClipboardRead, APIPermissionID::kClipboardWrite},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
       {APIPermissionID::kClipboardRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD_WRITE,
       {APIPermissionID::kClipboardWrite},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DESKTOP_CAPTURE,
       {APIPermissionID::kDesktopCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS,
       {APIPermissionID::kDownloads},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS_OPEN,
       {APIPermissionID::kDownloadsOpen},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_IDENTITY_EMAIL,
       {APIPermissionID::kIdentityEmail},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_SYSTEM_STORAGE,
       {APIPermissionID::kSystemStorage},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
       {APIPermissionID::kContentSettings},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOCUMENT_SCAN,
       {APIPermissionID::kDocumentScan},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_INPUT, {APIPermissionID::kInput}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
       {APIPermissionID::kManagement},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_MDNS, {APIPermissionID::kMDns}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_NATIVE_MESSAGING,
       {APIPermissionID::kNativeMessaging},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_PRIVACY, {APIPermissionID::kPrivacy}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_SYNCFILESYSTEM,
       {APIPermissionID::kSyncFileSystem},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_TAB_GROUPS,
       {APIPermissionID::kTabGroups},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_TTS_ENGINE,
       {APIPermissionID::kTtsEngine},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_WALLPAPER,
       {APIPermissionID::kWallpaper},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_PLATFORMKEYS,
       {APIPermissionID::kPlatformKeys},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CERTIFICATEPROVIDER,
       {APIPermissionID::kCertificateProvider},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_ACTIVITY_LOG_PRIVATE,
       {APIPermissionID::kActivityLogPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SETTINGS_PRIVATE,
       {APIPermissionID::kSettingsPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_AUTOFILL_PRIVATE,
       {APIPermissionID::kAutofillPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_PASSWORDS_PRIVATE,
       {APIPermissionID::kPasswordsPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_USERS_PRIVATE,
       {APIPermissionID::kUsersPrivate},
       {}},
      {GetEnterpriseReportingPrivatePermissionMessageId(),
       {APIPermissionID::kEnterpriseReportingPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_HARDWARE_PLATFORM,
       {APIPermissionID::kEnterpriseHardwarePlatform},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_DEVICE_ATTRIBUTES,
       {APIPermissionID::kEnterpriseDeviceAttributes},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_KIOSK_INPUT,
       {APIPermissionID::kEnterpriseKioskInput},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_NETWORKING_ATTRIBUTES,
       {APIPermissionID::kEnterpriseNetworkingAttributes},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_PLATFORMKEYS,
       {APIPermissionID::kEnterprisePlatformKeys},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_LOGIN, {APIPermissionID::kLogin}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_LOGIN_SCREEN_UI,
       {APIPermissionID::kLoginScreenUi},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_LOGIN_SCREEN_STORAGE,
       {APIPermissionID::kLoginScreenStorage},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ENTERPRISE_REMOTE_APPS,
       {APIPermissionID::kEnterpriseRemoteApps},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_TRANSIENT_BACKGROUND,
       {APIPermissionID::kTransientBackground},
       {}},

      // Telemetry System Extension permission messages.
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_ATTACHED_DEVICE_INFO,
       {APIPermissionID::kChromeOSAttachedDeviceInfo},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_BLUETOOTH_PERIPHERALS_INFO,
       {APIPermissionID::kChromeOSBluetoothPeripheralsInfo},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_DIAGNOSTICS,
       {APIPermissionID::kChromeOSDiagnostics},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_DIAGNOSTICS_NETWORK_INFO_FOR_MLAB,
       {APIPermissionID::kChromeOSDiagnosticsNetworkInfoForMlab},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_EVENTS,
       {APIPermissionID::kChromeOSEvents},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_MANAGEMENT_AUDIO,
       {APIPermissionID::kChromeOSManagementAudio},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_TELEMETRY,
       {APIPermissionID::kChromeOSTelemetry},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_TELEMETRY_SERIAL_NUMBER,
       {APIPermissionID::kChromeOSTelemetrySerialNumber},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_CHROMEOS_TELEMETRY_NETWORK_INFORMATION,
       {APIPermissionID::kChromeOSTelemetryNetworkInformation},
       {}}};

  return std::vector<ChromePermissionMessageRule>(
      std::make_move_iterator(std::begin(rules_arr)),
      std::make_move_iterator(std::end(rules_arr)));
}

}  // namespace extensions
