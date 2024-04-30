// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/chrome_usb_delegate.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/usb_connection_tracker.h"
#include "chrome/browser/usb/usb_connection_tracker_factory.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "third_party/blink/public/common/features_generated.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/containers/fixed_flat_set.h"
#include "chrome/common/chrome_features.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#endif

namespace {

using ::content::UsbChooser;

UsbChooserContext* GetChooserContext(content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  return profile ? UsbChooserContextFactory::GetForProfile(profile) : nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
UsbConnectionTracker* GetConnectionTracker(
    content::BrowserContext* browser_context,
    bool create) {
  // |browser_context| might be null in a service worker case when the browser
  // context is destroyed before service worker's destruction.
  auto* profile = Profile::FromBrowserContext(browser_context);
  return profile ? UsbConnectionTrackerFactory::GetForProfile(profile, create)
                 : nullptr;
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
// These extensions can claim the smart card USB class and automatically gain
// permissions for devices that have an interface with this class.
constexpr auto kSmartCardPrivilegedExtensionIds =
    base::MakeFixedFlatSet<std::string_view>({
        // Smart Card Connector Extension and its Beta version, see
        // crbug.com/1233881.
        "khpfeaanjngmcnplbdlpegiifgpfgdco",
        "mockcojkppdndnhgonljagclgpkjbkek",
    });

bool DeviceHasInterfaceWithClass(
    const device::mojom::UsbDeviceInfo& device_info,
    uint8_t interface_class) {
  for (const auto& configuration : device_info.configurations) {
    for (const auto& interface : configuration->interfaces) {
      for (const auto& alternate : interface->alternates) {
        if (alternate->class_code == interface_class)
          return true;
      }
    }
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

bool IsDevicePermissionAutoGranted(
    const url::Origin& origin,
    const device::mojom::UsbDeviceInfo& device_info) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Note: The `DeviceHasInterfaceWithClass()` call is made after checking the
  // origin, since that method call is expensive.
  if (origin.scheme() == extensions::kExtensionScheme &&
      base::Contains(kSmartCardPrivilegedExtensionIds, origin.host()) &&
      DeviceHasInterfaceWithClass(device_info,
                                  device::mojom::kUsbSmartCardClass)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return false;
}

}  // namespace

// Manages the UsbDelegate observers for a single browser context.
class ChromeUsbDelegate::ContextObservation
    : public permissions::ObjectPermissionContextBase::PermissionObserver,
      public UsbChooserContext::DeviceObserver {
 public:
  ContextObservation(ChromeUsbDelegate* parent,
                     content::BrowserContext* browser_context)
      : parent_(parent), browser_context_(browser_context) {
    auto* chooser_context = GetChooserContext(browser_context_);
    device_observation_.Observe(chooser_context);
    permission_observation_.Observe(chooser_context);
  }
  ContextObservation(ContextObservation&) = delete;
  ContextObservation& operator=(ContextObservation&) = delete;
  ~ContextObservation() override = default;

  // permissions::ObjectPermissionContextBase::PermissionObserver:
  void OnPermissionRevoked(const url::Origin& origin) override {
    for (auto& observer : observer_list_)
      observer.OnPermissionRevoked(origin);
  }

  // UsbChooserContext::DeviceObserver:
  void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device_info) override {
    for (auto& observer : observer_list_)
      observer.OnDeviceAdded(device_info);
  }

  void OnDeviceRemoved(
      const device::mojom::UsbDeviceInfo& device_info) override {
    for (auto& observer : observer_list_)
      observer.OnDeviceRemoved(device_info);
  }

  void OnDeviceManagerConnectionError() override {
    for (auto& observer : observer_list_)
      observer.OnDeviceManagerConnectionError();
  }

  void OnBrowserContextShutdown() override {
    parent_->observations_.erase(browser_context_);
    // Return since `this` is now deleted.
  }

  void AddObserver(content::UsbDelegate::Observer* observer) {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(content::UsbDelegate::Observer* observer) {
    observer_list_.RemoveObserver(observer);
  }

 private:
  // Safe because `parent_` owns `this`.
  const raw_ptr<ChromeUsbDelegate> parent_;

  // Safe because `this` is destroyed when the context is lost.
  const raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<UsbChooserContext, UsbChooserContext::DeviceObserver>
      device_observation_{this};
  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      permission_observation_{this};
  base::ObserverList<content::UsbDelegate::Observer> observer_list_;
};

ChromeUsbDelegate::ChromeUsbDelegate() = default;

ChromeUsbDelegate::~ChromeUsbDelegate() = default;

void ChromeUsbDelegate::AdjustProtectedInterfaceClasses(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    content::RenderFrameHost* frame,
    std::vector<uint8_t>& classes) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // We only adjust interfaces for extensions here.
  if (origin.scheme() != extensions::kExtensionScheme) {
    return;
  }
  // Don't enforce protected interface classes for Chrome Apps since the
  // chrome.usb API has no such restriction.
    auto* extension_registry =
        extensions::ExtensionRegistry::Get(browser_context);
    if (extension_registry) {
      const extensions::Extension* extension =
          extension_registry->enabled_extensions().GetByID(origin.host());
      if (extension && extension->is_platform_app()) {
        classes.clear();
        return;
      }
    }

#if BUILDFLAG(IS_CHROMEOS)
  // These extensions can claim the protected HID interface class (example: used
  // as badge readers)
    static constexpr auto kHidPrivilegedExtensionIds =
        base::MakeFixedFlatSet<std::string_view>({
            // Imprivata Extensions, see crbug.com/1065112 and crbug.com/995294.
            "baobpecgllpajfeojepgedjdlnlfffde",
            "bnfoibgpjolimhppjmligmcgklpboloj",
            "cdgickkdpbekbnalbmpgochbninibkko",
            "cjakdianfealdjlapagfagpdpemoppba",
            "cokoeepjbmmnhgdhlkpahohdaiedfjgn",
            "dahgfgiifpnaoajmloofonkndaaafacp",
            "dbknmmkopacopifbkgookcdbhfnggjjh",
            "ddcjglpbfbibgepfffpklmpihphbcdco",
            "dhodapiemamlmhlhblgcibabhdkohlen",
            "dlahpllbhpbkfnoiedkgombmegnnjopi",
            "egfpnfjeaopimgpiioeedbpmojdapaip",
            "fnbibocngjnefolmcodjkkghijpdlnfm",
            "jcnflhjcfjkplgkcinikhbgbhfldkadl",
            "jkfjfbelolphkjckiolfcakgalloegek",
            "kmhpgpnbglclbaccjjgoioogjlnfgbne",
            "lpimkpkllnkdlcigdbgmabfplniahkgm",
            "odehonhhkcjnbeaomlodfkjaecbmhklm",
            "olnmflhcfkifkgbiegcoabineoknmbjc",
            "omificdfgpipkkpdhbjmefgfgbppehke",
            "phjobickjiififdadeoepbdaciefacfj",
            "pkeacbojooejnjolgjdecbpnloibpafm",
            "pllbepacblmgialkkpcceohmjakafnbb",
            "plpogimmgnkkiflhpidbibfmgpkaofec",
            "pmhiabnkkchjeaehcodceadhdpfejmmd",

            // Hotrod Extensions, see crbug.com/1220165
            "acdafoiapclbpdkhnighhilgampkglpc",
            "denipklgekfpcdmbahmbpnmokgajnhma",
            "hkamnlhnogggfddmjomgbdokdkgfelgg",
            "ikfcpmgefdpheiiomgmhlmmkihchmdlj",
            "jlgegmdnodfhciolbdjciihnlaljdbjo",
            "ldmpofkllgeicjiihkimgeccbhghhmfj",
            "lkbhffjfgpmpeppncnimiiikojibkhnm",
            "moklfjoegmpoolceggbebbmgbddlhdgp",
        });

    if (base::Contains(kHidPrivilegedExtensionIds, origin.host())) {
      std::erase(classes, device::mojom::kUsbHidClass);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (base::Contains(kSmartCardPrivilegedExtensionIds, origin.host())) {
    std::erase(classes, device::mojom::kUsbSmartCardClass);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

std::unique_ptr<UsbChooser> ChromeUsbDelegate::RunChooser(
    content::RenderFrameHost& frame,
    blink::mojom::WebUsbRequestDeviceOptionsPtr options,
    blink::mojom::WebUsbService::GetPermissionCallback callback) {
  auto controller = std::make_unique<UsbChooserController>(
      &frame, std::move(options), std::move(callback));
  return WebUsbChooser::Create(&frame, std::move(controller));
}

bool ChromeUsbDelegate::PageMayUseUsb(content::Page& page) {
  content::RenderFrameHost& main_rfh = page.GetMainDocument();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // WebViewGuests have no mechanism to show permission prompts and their
  // embedder can't grant USB access through its permissionrequest API. Also
  // since webviews use a separate StoragePartition, they must not gain access
  // through permissions granted in non-webview contexts.
  if (extensions::WebViewGuest::FromRenderFrameHost(&main_rfh)) {
    return false;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // USB permissions are scoped to a BrowserContext instead of a
  // StoragePartition, so we need to be careful about usage across
  // StoragePartitions. Until this is scoped correctly, we'll try to avoid
  // inappropriate sharing by restricting access to the API. We can't be as
  // strict as we'd like, as cases like extensions and Isolated Web Apps still
  // need USB access in non-default partitions, so we'll just guard against
  // HTTP(S) as that presents a clear risk for inappropriate sharing.
  // TODO(crbug.com/40068594): USB permissions should be explicitly scoped to
  // StoragePartitions.
  if (main_rfh.GetStoragePartition() !=
      main_rfh.GetBrowserContext()->GetDefaultStoragePartition()) {
    return !main_rfh.GetLastCommittedURL().SchemeIsHTTPOrHTTPS();
  }

  return true;
}

bool ChromeUsbDelegate::CanRequestDevicePermission(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  return browser_context &&
         GetChooserContext(browser_context)->CanRequestObjectPermission(origin);
}

void ChromeUsbDelegate::RevokeDevicePermissionWebInitiated(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const device::mojom::UsbDeviceInfo& device) {
  auto* chooser_context = GetChooserContext(browser_context);
  if (chooser_context) {
    chooser_context->RevokeDevicePermissionWebInitiated(origin, device);
  }
}

const device::mojom::UsbDeviceInfo* ChromeUsbDelegate::GetDeviceInfo(
    content::BrowserContext* browser_context,
    const std::string& guid) {
  auto* chooser_context = GetChooserContext(browser_context);
  if (!chooser_context) {
    return nullptr;
  }
  return chooser_context->GetDeviceInfo(guid);
}

bool ChromeUsbDelegate::HasDevicePermission(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    const url::Origin& origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  if (IsDevicePermissionAutoGranted(origin, device_info)) {
    return true;
  }

  // Isolated context with permission to access the policy-controlled feature
  // "usb-unrestricted" can bypass the USB blocklist.
  bool is_usb_unrestricted = false;
  if (base::FeatureList::IsEnabled(blink::features::kUnrestrictedUsb)) {
    is_usb_unrestricted =
        frame &&
        frame->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kUsbUnrestricted) &&
        content::HasIsolatedContextCapability(frame);
  }

  if (!is_usb_unrestricted && UsbBlocklist::Get().IsExcluded(device_info)) {
    return false;
  }

  return browser_context && GetChooserContext(browser_context)
                                ->HasDevicePermission(origin, device_info);
}

void ChromeUsbDelegate::GetDevices(
    content::BrowserContext* browser_context,
    blink::mojom::WebUsbService::GetDevicesCallback callback) {
  auto* chooser_context = GetChooserContext(browser_context);
  if (!chooser_context) {
    std::move(callback).Run(std::vector<device::mojom::UsbDeviceInfoPtr>());
    return;
  }
  chooser_context->GetDevices(std::move(callback));
}

void ChromeUsbDelegate::GetDevice(
    content::BrowserContext* browser_context,
    const std::string& guid,
    base::span<const uint8_t> blocked_interface_classes,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client) {
  auto* chooser_context = GetChooserContext(browser_context);
  if (chooser_context) {
    chooser_context->GetDevice(guid, blocked_interface_classes,
                               std::move(device_receiver),
                               std::move(device_client));
  }
}

void ChromeUsbDelegate::AddObserver(content::BrowserContext* browser_context,
                                    Observer* observer) {
  if (!browser_context) {
    return;
  }
  GetContextObserver(browser_context)->AddObserver(observer);
}

void ChromeUsbDelegate::RemoveObserver(content::BrowserContext* browser_context,
                                       Observer* observer) {
  if (!browser_context) {
    return;
  }
  GetContextObserver(browser_context)->RemoveObserver(observer);
}

ChromeUsbDelegate::ContextObservation* ChromeUsbDelegate::GetContextObserver(
    content::BrowserContext* browser_context) {
  CHECK(browser_context);
  if (!base::Contains(observations_, browser_context)) {
    observations_.emplace(browser_context, std::make_unique<ContextObservation>(
                                               this, browser_context));
  }
  return observations_[browser_context].get();
}

bool ChromeUsbDelegate::IsServiceWorkerAllowedForOrigin(
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // WebUSB is only available on extension service workers for now.
  if (base::FeatureList::IsEnabled(
          features::kEnableWebUsbOnExtensionServiceWorker) &&
      origin.scheme() == extensions::kExtensionScheme) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
}

void ChromeUsbDelegate::IncrementConnectionCount(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
// Don't track connection when the feature isn't enabled or the connection
// isn't made by an extension origin.
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          features::kEnableWebUsbOnExtensionServiceWorker) ||
      origin.scheme() != extensions::kExtensionScheme) {
    return;
  }

  auto* usb_connection_tracker =
      GetConnectionTracker(browser_context, /*create=*/true);
  if (usb_connection_tracker) {
    usb_connection_tracker->IncrementConnectionCount(origin);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeUsbDelegate::DecrementConnectionCount(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  // Don't track connection when the feature isn't enabled or the connection
  // isn't made by an extension origin.
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          features::kEnableWebUsbOnExtensionServiceWorker) ||
      origin.scheme() != extensions::kExtensionScheme) {
    return;
  }
  auto* usb_connection_tracker =
      GetConnectionTracker(browser_context, /*create=*/false);
  if (usb_connection_tracker) {
    usb_connection_tracker->DecrementConnectionCount(origin);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}
