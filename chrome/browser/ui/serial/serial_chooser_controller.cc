// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/serial/serial_blocklist.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

using ::device::BluetoothAdapter;
using ::device::BluetoothAdapterFactory;
using ::device::mojom::SerialPortType;

bool FilterMatchesPort(const blink::mojom::SerialPortFilter& filter,
                       const device::mojom::SerialPortInfo& port) {
  if (filter.bluetooth_service_class_id) {
    if (!port.bluetooth_service_class_id) {
      return false;
    }
    return device::BluetoothUUID(*port.bluetooth_service_class_id) ==
           device::BluetoothUUID(*filter.bluetooth_service_class_id);
  }
  if (!filter.has_vendor_id) {
    return true;
  }
  if (!port.has_vendor_id || port.vendor_id != filter.vendor_id) {
    return false;
  }
  if (!filter.has_product_id) {
    return true;
  }
  return port.has_product_id && port.product_id == filter.product_id;
}

bool BluetoothPortIsAllowed(
    const std::vector<::device::BluetoothUUID>& allowed_ids,
    const device::mojom::SerialPortInfo& port) {
  if (!port.bluetooth_service_class_id) {
    return true;
  }
  // Serial Port Profile is allowed by default.
  if (*port.bluetooth_service_class_id == device::GetSerialPortProfileUUID()) {
    return true;
  }
  return base::Contains(allowed_ids, port.bluetooth_service_class_id.value());
}

}  // namespace

SerialChooserController::SerialChooserController(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    std::vector<::device::BluetoothUUID> allowed_bluetooth_service_class_ids,
    content::SerialChooser::Callback callback)
    : ChooserController(CreateChooserTitle(render_frame_host,
                                           IDS_SERIAL_PORT_CHOOSER_PROMPT)),
      filters_(std::move(filters)),
      allowed_bluetooth_service_class_ids_(
          std::move(allowed_bluetooth_service_class_ids)),
      callback_(std::move(callback)),
      initiator_document_(render_frame_host->GetWeakDocumentPtr()) {
  origin_ = render_frame_host->GetMainFrame()->GetLastCommittedOrigin();

  auto* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  chooser_context_ =
      SerialChooserContextFactory::GetForProfile(profile)->AsWeakPtr();
  DCHECK(chooser_context_);

  // Post `GetDevices` to be run later after the view is set in the current
  // sequence, so that it will have a valid view when running `GetDevices`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SerialChooserController::GetDevices,
                                weak_factory_.GetWeakPtr()));

  observation_.Observe(chooser_context_.get());
}

SerialChooserController::~SerialChooserController() {
  if (callback_)
    RunCallback(/*port=*/nullptr);
}

const device::mojom::SerialPortInfo& SerialChooserController::GetPortForTest(
    size_t index) const {
  CHECK_LT(index, ports_.size());
  return *ports_[index];
}

void SerialChooserController::GetDevices() {
  CHECK(view());
  if (IsWirelessSerialPortOnly()) {
    if (!adapter_) {
      BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
          &SerialChooserController::OnGetAdapter, weak_factory_.GetWeakPtr(),
          base::BindOnce(&SerialChooserController::GetDevices,
                         weak_factory_.GetWeakPtr())));
      return;
    }

    if (adapter_->GetOsPermissionStatus() !=
        device::BluetoothAdapter::PermissionStatus::kAllowed) {
      view()->OnAdapterAuthorizationChanged(false);
      return;
    }

    if (!adapter_->IsPowered()) {
      view()->OnAdapterEnabledChanged(false);
      return;
    }
  }

  chooser_context_->GetPortManager()->GetDevices(base::BindOnce(
      &SerialChooserController::OnGetDevices, weak_factory_.GetWeakPtr()));
}

bool SerialChooserController::ShouldShowHelpButton() const {
  return true;
}

std::u16string SerialChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string SerialChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_SERIAL_PORT_CHOOSER_CONNECT_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
SerialChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_SERIAL_PORT_CHOOSER_LOADING_LABEL),
      l10n_util::GetStringUTF16(IDS_SERIAL_PORT_CHOOSER_LOADING_LABEL_TOOLTIP)};
}

size_t SerialChooserController::NumOptions() const {
  return ports_.size();
}

// Does the Bluetooth service class ID need to be displayed along with the
// display name for the provided `port`? The goal is to display the shortest
// name necessary to identify the port. When two (or more) ports from the same
// device are selected, the service class ID is added to disambiguate the two
// ports.
bool SerialChooserController::DisplayServiceClassId(
    const device::mojom::SerialPortInfo& port) const {
  CHECK_EQ(port.type, device::mojom::SerialPortType::BLUETOOTH_CLASSIC_RFCOMM);
  return base::ranges::any_of(
      ports_, [&port](const device::mojom::SerialPortInfoPtr& p) {
        return p->token != port.token &&
               p->type == SerialPortType::BLUETOOTH_CLASSIC_RFCOMM &&
               p->path == port.path;
      });
}

std::u16string SerialChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, ports_.size());
  const device::mojom::SerialPortInfo& port = *ports_[index];

  // Get the last component of the device path i.e. COM1 or ttyS0 to show the
  // user something similar to other applications that ask them to choose a
  // serial port and to differentiate between ports with similar display names.
  std::u16string display_path = port.path.BaseName().LossyDisplayName();

  if (!port.display_name || port.display_name->empty()) {
    return display_path;
  }

  if (port.type == device::mojom::SerialPortType::BLUETOOTH_CLASSIC_RFCOMM) {
    if (DisplayServiceClassId(port)) {
      // Using UUID in place of path is identical for translation purposes
      // so using IDS_SERIAL_PORT_CHOOSER_NAME_WITH_PATH is ok.
      device::BluetoothUUID device_uuid(*port.bluetooth_service_class_id);
      return l10n_util::GetStringFUTF16(
          IDS_SERIAL_PORT_CHOOSER_NAME_WITH_PATH,
          base::UTF8ToUTF16(*port.display_name),
          base::UTF8ToUTF16(device_uuid.canonical_value()));
    }
    return base::UTF8ToUTF16(*port.display_name);
  }

  return l10n_util::GetStringFUTF16(IDS_SERIAL_PORT_CHOOSER_NAME_WITH_PATH,
                                    base::UTF8ToUTF16(*port.display_name),
                                    display_path);
}

bool SerialChooserController::IsPaired(size_t index) const {
  DCHECK_LE(index, ports_.size());

  if (!chooser_context_)
    return false;

  return chooser_context_->HasPortPermission(origin_, *ports_[index]);
}

void SerialChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  DCHECK_LT(index, ports_.size());

  if (!chooser_context_) {
    RunCallback(/*port=*/nullptr);
    return;
  }

  chooser_context_->GrantPortPermission(origin_, *ports_[index]);
  RunCallback(ports_[index]->Clone());
}

void SerialChooserController::Cancel() {}

void SerialChooserController::Close() {}

// TODO(crbug.com/355570625): Shared impl with ChromeBluetoothChooserController.
void SerialChooserController::OpenAdapterOffHelpUrl() const {
  CHECK(chooser_context_);
  Profile* profile = chooser_context_->profile();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS can directly link to the OS setting to turn on the adapter.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      profile, chromeos::settings::mojom::kBluetoothDevicesSubpagePath);
#else
  // For other operating systems, show a help center page in a tab.
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(profile);
  CHECK(browser_displayer.browser());
  browser_displayer.browser()->OpenURL(
      content::OpenURLParams(GURL(chrome::kBluetoothAdapterOffHelpURL),
                             content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
#endif
}

void SerialChooserController::OpenHelpCenterUrl() const {
  auto* rfh = initiator_document_.AsRenderFrameHostIfValid();
  auto* web_contents = rfh && rfh->IsActive()
                           ? content::WebContents::FromRenderFrameHost(rfh)
                           : nullptr;
  if (!web_contents)
    return;

  web_contents->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kChooserSerialOverviewUrl), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false),
      /*navigation_handle_callback=*/{});
}

void SerialChooserController::OpenPermissionPreferences() const {
#if BUILDFLAG(IS_MAC)
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Bluetooth);
#else
  NOTREACHED();
#endif
}

bool SerialChooserController::ShouldShowAdapterOffView() const {
  return true;
}

int SerialChooserController::GetAdapterOffMessageId() const {
  return IDS_SERIAL_DEVICE_CHOOSER_ADAPTER_OFF;
}

int SerialChooserController::GetTurnAdapterOnLinkTextMessageId() const {
  return IDS_SERIAL_DEVICE_CHOOSER_TURN_ON_BLUETOOTH_LINK_TEXT;
}

bool SerialChooserController::ShouldShowAdapterUnauthorizedView() const {
  return true;
}

int SerialChooserController::GetBluetoothUnauthorizedMessageId() const {
  return IDS_SERIAL_DEVICE_CHOOSER_AUTHORIZE_BLUETOOTH;
}

int SerialChooserController::GetAuthorizeBluetoothLinkTextMessageId() const {
  return IDS_SERIAL_DEVICE_CHOOSER_AUTHORIZE_BLUETOOTH_LINK_TEXT;
}

void SerialChooserController::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                                    bool powered) {
  CHECK(view());
  view()->OnAdapterEnabledChanged(powered);
  if (powered) {
    GetDevices();
  }
}

void SerialChooserController::OnPortAdded(
    const device::mojom::SerialPortInfo& port) {
  if (!DisplayDevice(port))
    return;

  ports_.push_back(port.Clone());
  if (view())
    view()->OnOptionAdded(ports_.size() - 1);
}

void SerialChooserController::OnPortRemoved(
    const device::mojom::SerialPortInfo& port) {
  const auto it = base::ranges::find(ports_, port.token,
                                     &device::mojom::SerialPortInfo::token);
  if (it != ports_.end()) {
    const size_t index = it - ports_.begin();
    ports_.erase(it);
    if (view())
      view()->OnOptionRemoved(index);
  }
}

void SerialChooserController::OnPortManagerConnectionError() {
  observation_.Reset();
}

void SerialChooserController::OnGetDevices(
    std::vector<device::mojom::SerialPortInfoPtr> ports) {
  // Sort ports by file paths.
  std::sort(ports.begin(), ports.end(),
            [](const auto& port1, const auto& port2) {
              return port1->path.BaseName() < port2->path.BaseName();
            });

  ports_.clear();
  for (auto& port : ports) {
    if (DisplayDevice(*port))
      ports_.push_back(std::move(port));
  }

  if (view())
    view()->OnOptionsInitialized();
}

bool SerialChooserController::DisplayDevice(
    const device::mojom::SerialPortInfo& port) const {
  if (SerialBlocklist::Get().IsExcluded(port)) {
    if (port.has_vendor_id && port.has_product_id) {
      AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          base::StringPrintf(
              "Chooser dialog is not displaying a port blocked by "
              "the Serial blocklist: vendorId=%d, "
              "productId=%d, name='%s', serial='%s'",
              port.vendor_id, port.product_id,
              port.display_name ? port.display_name.value().c_str() : "",
              port.serial_number ? port.serial_number.value().c_str() : ""));
    } else if (port.bluetooth_service_class_id) {
      AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kInfo,
          base::StringPrintf(
              "Chooser dialog is not displaying a port blocked by "
              "the Serial blocklist: bluetoothServiceClassId=%s, "
              "name='%s'",
              port.bluetooth_service_class_id->value().c_str(),
              port.display_name ? port.display_name.value().c_str() : ""));
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    return false;
  }

  if (filters_.empty()) {
    return BluetoothPortIsAllowed(allowed_bluetooth_service_class_ids_, port);
  }

  for (const auto& filter : filters_) {
    if (FilterMatchesPort(*filter, port) &&
        BluetoothPortIsAllowed(allowed_bluetooth_service_class_ids_, port)) {
      return true;
    }
  }

  return false;
}

void SerialChooserController::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) const {
  if (content::RenderFrameHost* rfh =
          initiator_document_.AsRenderFrameHostIfValid()) {
    rfh->AddMessageToConsole(level, message);
  }
}

void SerialChooserController::RunCallback(
    device::mojom::SerialPortInfoPtr port) {
  auto outcome = ports_.empty() ? SerialChooserOutcome::kCancelledNoDevices
                                : SerialChooserOutcome::kCancelled;

  if (port) {
    outcome = SerialChooserContext::CanStorePersistentEntry(*port)
                  ? SerialChooserOutcome::kPermissionGranted
                  : SerialChooserOutcome::kEphemeralPermissionGranted;
  }

  UMA_HISTOGRAM_ENUMERATION("Permissions.Serial.ChooserClosed", outcome);
  std::move(callback_).Run(std::move(port));
}

void SerialChooserController::OnGetAdapter(
    base::OnceClosure callback,
    scoped_refptr<BluetoothAdapter> adapter) {
  CHECK(adapter);
  adapter_ = std::move(adapter);
  adapter_observation_.Observe(adapter_.get());
  std::move(callback).Run();
}

bool SerialChooserController::IsWirelessSerialPortOnly() {
  if (allowed_bluetooth_service_class_ids_.empty()) {
    return false;
  }

  // The system's wired and wireless serial ports can be shown if there is no
  // filter.
  if (filters_.empty()) {
    return false;
  }

  // Check if all the filters are meant for serial port from Bluetooth device.
  for (const auto& filter : filters_) {
    if (!filter->bluetooth_service_class_id) {
      return false;
    }
  }
  return true;
}
