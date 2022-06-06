// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/browser/usb/web_usb_chooser.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/containers/fixed_flat_set.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
// These extensions automatically gain permissions for the smart card USB class
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

std::unique_ptr<content::UsbChooser> RunChooser(
    content::RenderFrameHost& frame,
    std::vector<device::mojom::UsbDeviceFilterPtr> filters,
    WebUsbServiceImpl::GetPermissionCallback callback) {
  auto controller = std::make_unique<UsbChooserController>(
      &frame, std::move(filters), std::move(callback));
  return WebUsbChooser::Create(&frame, std::move(controller));
}

}  // namespace

// A UsbDeviceClient represents a UsbDevice pipe that has been passed to the
// renderer process. The UsbDeviceClient pipe allows the browser process to
// continue to monitor how the device is used and cause the connection to be
// closed at will.
class WebUsbServiceImpl::UsbDeviceClient
    : public device::mojom::UsbDeviceClient {
 public:
  UsbDeviceClient(
      WebUsbServiceImpl* service,
      const std::string& device_guid,
      mojo::PendingReceiver<device::mojom::UsbDeviceClient> receiver)
      : service_(service),
        device_guid_(device_guid),
        receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&WebUsbServiceImpl::RemoveDeviceClient,
                       base::Unretained(service_), base::Unretained(this)));
  }

  ~UsbDeviceClient() override {
    if (opened_) {
      // If the connection was opened destroying |receiver_| will cause it to
      // be closed but that event won't be dispatched here because the receiver
      // has been destroyed.
      OnDeviceClosed();
    }
  }

  const std::string& device_guid() const { return device_guid_; }

  // device::mojom::UsbDeviceClient implementation:
  void OnDeviceOpened() override {
    DCHECK(!opened_);
    opened_ = true;
    service_->IncrementConnectionCount();
  }

  void OnDeviceClosed() override {
    DCHECK(opened_);
    opened_ = false;
    service_->DecrementConnectionCount();
  }

 private:
  const raw_ptr<WebUsbServiceImpl> service_;
  const std::string device_guid_;
  bool opened_ = false;
  mojo::Receiver<device::mojom::UsbDeviceClient> receiver_;
};

WebUsbServiceImpl::WebUsbServiceImpl(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  DCHECK(render_frame_host_);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  // This class is destroyed on cross-origin navigations and so it is safe to
  // cache these values.
  origin_ = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chooser_context_ = UsbChooserContextFactory::GetForProfile(profile);
  DCHECK(chooser_context_);

  receivers_.set_disconnect_handler(base::BindRepeating(
      &WebUsbServiceImpl::OnConnectionError, base::Unretained(this)));

  chooser_factory_ = base::BindRepeating(&RunChooser);
}

WebUsbServiceImpl::~WebUsbServiceImpl() = default;

void WebUsbServiceImpl::BindReceiver(
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  receivers_.Add(this, std::move(receiver));

  // Listen to UsbChooserContext for add/remove device events from UsbService.
  // We can't set WebUsbServiceImpl as a UsbDeviceManagerClient because
  // the OnDeviceRemoved event will be delivered here after it is delivered
  // to UsbChooserContext, meaning that all ephemeral permission checks in
  // OnDeviceRemoved() will fail.
  if (!device_observation_.IsObserving())
    device_observation_.Observe(chooser_context_.get());
  if (!permission_observation_.IsObserving())
    permission_observation_.Observe(chooser_context_.get());
}

void WebUsbServiceImpl::SetChooserFactoryForTesting(
    ChooserFactoryCallback chooser_factory) {
  chooser_factory_ = std::move(chooser_factory);
}

bool WebUsbServiceImpl::HasDevicePermission(
    const device::mojom::UsbDeviceInfo& device_info) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host_);

  if (IsDevicePermissionAutoGranted(origin_, device_info))
    return true;

  if (!chooser_context_)
    return false;

  return chooser_context_->HasDevicePermission(origin_, device_info);
}

std::vector<uint8_t> WebUsbServiceImpl::GetProtectedInterfaceClasses() const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Don't enforce protected interface classes for Chrome Apps since the
  // chrome.usb API has no such restriction.
  if (origin_.scheme() == extensions::kExtensionScheme) {
    auto* extension_registry = extensions::ExtensionRegistry::Get(
        render_frame_host_->GetBrowserContext());
    if (extension_registry) {
      const extensions::Extension* extension =
          extension_registry->enabled_extensions().GetByID(origin_.host());
      if (extension && extension->is_platform_app()) {
        return {};
      }
    }
  }
#endif

  // Isolated Apps have unrestricted access to any USB interface class.
  if (render_frame_host_->GetWebExposedIsolationLevel() >=
      content::RenderFrameHost::WebExposedIsolationLevel::
          kMaybeIsolatedApplication) {
    // TODO(https://crbug.com/1236706): Should the list of interface classes the
    // app expects to claim be encoded in the Web App Manifest?
    return {};
  }

  // Specified in https://wicg.github.io/webusb#protected-interface-classes
  std::vector<uint8_t> classes = {
      device::mojom::kUsbAudioClass,       device::mojom::kUsbHidClass,
      device::mojom::kUsbMassStorageClass, device::mojom::kUsbSmartCardClass,
      device::mojom::kUsbVideoClass,       device::mojom::kUsbAudioVideoClass,
      device::mojom::kUsbWirelessClass,
  };

#if BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS)
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

  if (origin_.scheme() == extensions::kExtensionScheme &&
      base::Contains(kHidPrivilegedExtensionIds, origin_.host())) {
    base::Erase(classes, device::mojom::kUsbHidClass);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS) && BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin_.scheme() == extensions::kExtensionScheme &&
      base::Contains(kSmartCardPrivilegedExtensionIds, origin_.host())) {
    base::Erase(classes, device::mojom::kUsbSmartCardClass);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  return classes;
}

void WebUsbServiceImpl::GetDevices(GetDevicesCallback callback) {
  if (!chooser_context_) {
    std::move(callback).Run(std::vector<device::mojom::UsbDeviceInfoPtr>());
    return;
  }

  chooser_context_->GetDevices(base::BindOnce(&WebUsbServiceImpl::OnGetDevices,
                                              weak_factory_.GetWeakPtr(),
                                              std::move(callback)));
}

void WebUsbServiceImpl::OnGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list) {
  std::vector<device::mojom::UsbDeviceInfoPtr> device_infos;
  for (auto& device_info : device_info_list) {
    if (HasDevicePermission(*device_info))
      device_infos.push_back(device_info.Clone());
  }
  std::move(callback).Run(std::move(device_infos));
}

void WebUsbServiceImpl::GetDevice(
    const std::string& guid,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) {
  if (!chooser_context_)
    return;

  auto* device_info = chooser_context_->GetDeviceInfo(guid);
  if (!device_info || !HasDevicePermission(*device_info))
    return;

  // Connect Blink to the native device and keep a receiver to this for the
  // UsbDeviceClient interface so we can receive DeviceOpened/Closed events.
  // This receiver will also be closed to notify the device service to close
  // the connection if permission is revoked.
  mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client;
  device_clients_.push_back(std::make_unique<UsbDeviceClient>(
      this, guid, device_client.InitWithNewPipeAndPassReceiver()));

  chooser_context_->GetDevice(guid, GetProtectedInterfaceClasses(),
                              std::move(device_receiver),
                              std::move(device_client));
}

void WebUsbServiceImpl::GetPermission(
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    GetPermissionCallback callback) {
  if (!chooser_context_ ||
      !chooser_context_->CanRequestObjectPermission(origin_)) {
    std::move(callback).Run(nullptr);
    return;
  }

  usb_chooser_ = chooser_factory_.Run(
      *render_frame_host_, std::move(device_filters), std::move(callback));
}

void WebUsbServiceImpl::ForgetDevice(const std::string& guid,
                                     ForgetDeviceCallback callback) {
  if (chooser_context_) {
    auto* device_info = chooser_context_->GetDeviceInfo(guid);
    if (device_info && HasDevicePermission(*device_info)) {
      chooser_context_->RevokeDevicePermissionWebInitiated(origin_,
                                                           *device_info);
    }
  }
  std::move(callback).Run();
}

void WebUsbServiceImpl::SetClient(
    mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
        client) {
  DCHECK(client);
  clients_.Add(std::move(client));
}

void WebUsbServiceImpl::OnPermissionRevoked(const url::Origin& origin) {
  if (origin_ != origin) {
    return;
  }

  // Close the connection between Blink and the device if the device lost
  // permission.
  base::EraseIf(device_clients_, [this](const auto& client) {
    auto* device_info = chooser_context_->GetDeviceInfo(client->device_guid());
    if (!device_info)
      return true;

    return !HasDevicePermission(*device_info);
  });
}

void WebUsbServiceImpl::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (!HasDevicePermission(device_info))
    return;

  for (auto& client : clients_)
    client->OnDeviceAdded(device_info.Clone());
}

void WebUsbServiceImpl::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {
  base::EraseIf(device_clients_, [&device_info](const auto& client) {
    return device_info.guid == client->device_guid();
  });

  if (!HasDevicePermission(device_info))
    return;

  for (auto& client : clients_)
    client->OnDeviceRemoved(device_info.Clone());
}

void WebUsbServiceImpl::OnDeviceManagerConnectionError() {
  // Close the connection with blink.
  clients_.Clear();
  receivers_.Clear();

  // Remove itself from UsbChooserContext's ObserverList.
  device_observation_.Reset();
  permission_observation_.Reset();
}

// device::mojom::UsbDeviceClient implementation:
void WebUsbServiceImpl::IncrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->IncrementConnectionCount();
}

void WebUsbServiceImpl::DecrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  UsbTabHelper* tab_helper = UsbTabHelper::FromWebContents(web_contents);
  tab_helper->DecrementConnectionCount();
}

void WebUsbServiceImpl::RemoveDeviceClient(const UsbDeviceClient* client) {
  base::EraseIf(device_clients_, [client](const auto& this_client) {
    return client == this_client.get();
  });
}

void WebUsbServiceImpl::OnConnectionError() {
  if (receivers_.empty()) {
    device_observation_.Reset();
    permission_observation_.Reset();
  }
}
