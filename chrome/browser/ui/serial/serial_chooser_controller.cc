// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_blocklist.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/serial_chooser_histograms.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

SerialChooserController::SerialChooserController(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    content::SerialChooser::Callback callback)
    : ChooserController(CreateChooserTitle(render_frame_host,
                                           IDS_SERIAL_PORT_CHOOSER_PROMPT)),
      filters_(std::move(filters)),
      callback_(std::move(callback)),
      initiator_document_(render_frame_host->GetWeakDocumentPtr()) {
  origin_ = render_frame_host->GetMainFrame()->GetLastCommittedOrigin();

  auto* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  chooser_context_ =
      SerialChooserContextFactory::GetForProfile(profile)->AsWeakPtr();
  DCHECK(chooser_context_);

  chooser_context_->GetPortManager()->GetDevices(base::BindOnce(
      &SerialChooserController::OnGetDevices, weak_factory_.GetWeakPtr()));
  observation_.Observe(chooser_context_.get());
}

SerialChooserController::~SerialChooserController() {
  if (callback_)
    RunCallback(/*port=*/nullptr);
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

std::u16string SerialChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, ports_.size());
  const device::mojom::SerialPortInfo& port = *ports_[index];

  // Get the last component of the device path i.e. COM1 or ttyS0 to show the
  // user something similar to other applications that ask them to choose a
  // serial port and to differentiate between ports with similar display names.
  std::u16string display_path = port.path.BaseName().LossyDisplayName();

  if (port.display_name && !port.display_name->empty()) {
    return l10n_util::GetStringFUTF16(IDS_SERIAL_PORT_CHOOSER_NAME_WITH_PATH,
                                      base::UTF8ToUTF16(*port.display_name),
                                      display_path);
  }

  return l10n_util::GetStringFUTF16(IDS_SERIAL_PORT_CHOOSER_PATH_ONLY,
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

void SerialChooserController::OpenHelpCenterUrl() const {
  auto* rfh = initiator_document_.AsRenderFrameHostIfValid();
  auto* web_contents = rfh && rfh->IsActive()
                           ? content::WebContents::FromRenderFrameHost(rfh)
                           : nullptr;
  if (!web_contents)
    return;

  web_contents->OpenURL(content::OpenURLParams(
      GURL(chrome::kChooserSerialOverviewUrl), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false));
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
    DCHECK(port.has_vendor_id && port.has_product_id);
    AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kInfo,
        base::StringPrintf(
            "Chooser dialog is not displaying a port blocked by "
            "the Serial blocklist: vendorId=%d, "
            "productId=%d, name='%s', serial='%s'",
            port.vendor_id, port.product_id,
            port.display_name ? port.display_name.value().c_str() : "",
            port.serial_number ? port.serial_number.value().c_str() : ""));
    return false;
  }

  if (filters_.empty())
    return true;

  for (const auto& filter : filters_) {
    if (filter->has_vendor_id &&
        (!port.has_vendor_id || filter->vendor_id != port.vendor_id)) {
      continue;
    }
    if (filter->has_product_id &&
        (!port.has_product_id || filter->product_id != port.product_id)) {
      continue;
    }
    return true;
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
