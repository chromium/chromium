// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

SerialChooserController::SerialChooserController(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    content::SerialChooser::Callback callback)
    : ChooserController(render_frame_host,
                        IDS_SERIAL_PORT_CHOOSER_PROMPT_ORIGIN,
                        IDS_SERIAL_PORT_CHOOSER_PROMPT_EXTENSION_NAME),
      filters_(std::move(filters)),
      callback_(std::move(callback)) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  requesting_origin_ = render_frame_host->GetLastCommittedOrigin();
  embedding_origin_ = web_contents->GetMainFrame()->GetLastCommittedOrigin();

  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chooser_context_ =
      SerialChooserContextFactory::GetForProfile(profile)->AsWeakPtr();
  DCHECK(chooser_context_);

  chooser_context_->GetPortManager()->GetDevices(base::BindOnce(
      &SerialChooserController::OnGetDevices, weak_factory_.GetWeakPtr()));
}

SerialChooserController::~SerialChooserController() {
  if (callback_)
    std::move(callback_).Run(nullptr);
}

base::string16 SerialChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(IDS_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

base::string16 SerialChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_SERIAL_PORT_CHOOSER_CONNECT_BUTTON_TEXT);
}

size_t SerialChooserController::NumOptions() const {
  return ports_.size();
}

base::string16 SerialChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, ports_.size());
  const device::mojom::SerialPortInfo& port = *ports_[index];

  // Get the last component of the device path i.e. COM1 or ttyS0 to show the
  // user something similar to other applications that ask them to choose a
  // serial port and to differentiate between ports with similar display names.
  base::string16 display_path = port.path.BaseName().LossyDisplayName();

  if (port.display_name) {
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

  return chooser_context_->HasPortPermission(requesting_origin_,
                                             embedding_origin_, *ports_[index]);
}

void SerialChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  DCHECK_LT(index, ports_.size());

  if (!chooser_context_) {
    std::move(callback_).Run(nullptr);
    return;
  }

  chooser_context_->GrantPortPermission(requesting_origin_, embedding_origin_,
                                        *ports_[index]);
  std::move(callback_).Run(std::move(ports_[index]));
}

void SerialChooserController::Cancel() {}

void SerialChooserController::Close() {}

void SerialChooserController::OpenHelpCenterUrl() const {
  NOTIMPLEMENTED();
}

void SerialChooserController::OnGetDevices(
    std::vector<device::mojom::SerialPortInfoPtr> ports) {
  for (auto& port : ports) {
    if (FilterMatchesAny(*port))
      ports_.push_back(std::move(port));
  }

  if (view())
    view()->OnOptionsInitialized();
}

bool SerialChooserController::FilterMatchesAny(
    const device::mojom::SerialPortInfo& port) const {
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
