// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/chrome_usb_delegate.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/containers/fixed_flat_set.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#endif

namespace {

using ::content::RenderFrameHost;
using ::content::UsbChooser;

UsbChooserContext* GetChooserContext(RenderFrameHost& frame) {
  return UsbChooserContextFactory::GetForProfile(
      Profile::FromBrowserContext(frame.GetBrowserContext()));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// These extensions can claim the smart card USB class and automatically gain
// permissions for devices that have an interface with this class.
constexpr auto kSmartCardPrivilegedExtensionIds =
    base::MakeFixedFlatSet<base::StringPiece>({
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

ChromeUsbDelegate::ChromeUsbDelegate() = default;

ChromeUsbDelegate::~ChromeUsbDelegate() = default;

void ChromeUsbDelegate::AdjustProtectedInterfaceClasses(
    RenderFrameHost& frame,
    std::vector<uint8_t>& classes) {
  // Isolated Apps have unrestricted access to any USB interface class.
  if (frame.GetWebExposedIsolationLevel() >=
      content::RenderFrameHost::WebExposedIsolationLevel::
          kMaybeIsolatedApplication) {
    // TODO(https://crbug.com/1236706): Should the list of interface classes the
    // app expects to claim be encoded in the Web App Manifest?
    classes.clear();
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  const auto origin = frame.GetMainFrame()->GetLastCommittedOrigin();

  // Don't enforce protected interface classes for Chrome Apps since the
  // chrome.usb API has no such restriction.
  if (origin.scheme() == extensions::kExtensionScheme) {
    auto* extension_registry =
        extensions::ExtensionRegistry::Get(frame.GetBrowserContext());
    if (extension_registry) {
      const extensions::Extension* extension =
          extension_registry->enabled_extensions().GetByID(origin.host());
      if (extension && extension->is_platform_app()) {
        classes.clear();
        return;
      }
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  // These extensions can claim the protected HID interface class (example: used
  // as badge readers)
  static constexpr auto kHidPrivilegedExtensionIds =
      base::MakeFixedFlatSet<base::StringPiece>({
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

  if (origin.scheme() == extensions::kExtensionScheme &&
      base::Contains(kHidPrivilegedExtensionIds, origin.host())) {
    base::Erase(classes, device::mojom::kUsbHidClass);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (origin.scheme() == extensions::kExtensionScheme &&
      base::Contains(kSmartCardPrivilegedExtensionIds, origin.host())) {
    base::Erase(classes, device::mojom::kUsbSmartCardClass);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

std::unique_ptr<UsbChooser> ChromeUsbDelegate::RunChooser(
    RenderFrameHost& frame,
    std::vector<device::mojom::UsbDeviceFilterPtr> filters,
    blink::mojom::WebUsbService::GetPermissionCallback callback) {
  auto* chooser_context = GetChooserContext(frame);
  if (!device_observation_.IsObserving())
    device_observation_.Observe(chooser_context);
  if (!permission_observation_.IsObserving())
    permission_observation_.Observe(chooser_context);

  auto controller = std::make_unique<UsbChooserController>(
      &frame, std::move(filters), std::move(callback));
  return WebUsbChooser::Create(&frame, std::move(controller));
}

bool ChromeUsbDelegate::CanRequestDevicePermission(RenderFrameHost& frame) {
  return GetChooserContext(frame)->CanRequestObjectPermission(
      frame.GetMainFrame()->GetLastCommittedOrigin());
}

void ChromeUsbDelegate::RevokeDevicePermissionWebInitiated(
    content::RenderFrameHost& frame,
    const device::mojom::UsbDeviceInfo& device) {
  GetChooserContext(frame)->RevokeDevicePermissionWebInitiated(
      frame.GetMainFrame()->GetLastCommittedOrigin(), device);
}

const device::mojom::UsbDeviceInfo* ChromeUsbDelegate::GetDeviceInfo(
    RenderFrameHost& frame,
    const std::string& guid) {
  return GetChooserContext(frame)->GetDeviceInfo(guid);
}

void ChromeUsbDelegate::OnPermissionRevoked(const url::Origin& origin) {
  for (auto& observer : observer_list_)
    observer.OnPermissionRevoked(origin);
}

void ChromeUsbDelegate::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device) {
  for (auto& observer : observer_list_)
    observer.OnDeviceAdded(device);
}

void ChromeUsbDelegate::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device) {
  for (auto& observer : observer_list_)
    observer.OnDeviceRemoved(device);
}

void ChromeUsbDelegate::OnDeviceManagerConnectionError() {
  for (auto& observer : observer_list_)
    observer.OnDeviceManagerConnectionError();
}

bool ChromeUsbDelegate::HasDevicePermission(
    RenderFrameHost& frame,
    const device::mojom::UsbDeviceInfo& device) {
  const auto origin = frame.GetMainFrame()->GetLastCommittedOrigin();
  if (IsDevicePermissionAutoGranted(origin, device))
    return true;

  return GetChooserContext(frame)->HasDevicePermission(origin, device);
}

void ChromeUsbDelegate::GetDevices(
    RenderFrameHost& frame,
    blink::mojom::WebUsbService::GetDevicesCallback callback) {
  GetChooserContext(frame)->GetDevices(std::move(callback));
}

void ChromeUsbDelegate::GetDevice(
    RenderFrameHost& frame,
    const std::string& guid,
    base::span<const uint8_t> blocked_interface_classes,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
    mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client) {
  GetChooserContext(frame)->GetDevice(guid, blocked_interface_classes,
                                      std::move(device_receiver),
                                      std::move(device_client));
}

void ChromeUsbDelegate::AddObserver(RenderFrameHost& frame,
                                    Observer* observer) {
  observer_list_.AddObserver(observer);
  auto* chooser_context = GetChooserContext(frame);
  if (!device_observation_.IsObserving())
    device_observation_.Observe(chooser_context);
  if (!permission_observation_.IsObserving())
    permission_observation_.Observe(chooser_context);
}

void ChromeUsbDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
