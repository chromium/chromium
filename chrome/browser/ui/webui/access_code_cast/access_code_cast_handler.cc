// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"

AccessCodeCastHandler::AccessCodeCastHandler(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    Profile* profile)
    : page_(std::move(page)),
      receiver_(this, std::move(page_handler)),
      profile_(profile) {
  DCHECK(profile_);
}

AccessCodeCastHandler::~AccessCodeCastHandler() = default;

void AccessCodeCastHandler::AddSink(
    const std::string& access_code,
    access_code_cast::mojom::CastDiscoveryMethod discovery_method,
    AddSinkCallback callback) {
  add_sink_callback_ = std::move(callback);

  discovery_server_interface_ =
      std::make_unique<AccessCodeCastDiscoveryInterface>(profile_, access_code);

  discovery_server_interface_->ValidateDiscoveryAccessCode(
      base::BindOnce(&AccessCodeCastHandler::OnAccessCodeValidated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccessCodeCastHandler::OnAccessCodeValidated(
    absl::optional<DiscoveryDevice> discovery_device,
    access_code_cast::mojom::AddSinkResultCode result_code) {
  if (discovery_device.has_value() &&
      result_code == access_code_cast::mojom::AddSinkResultCode::OK) {
    CreateSink(std::move(add_sink_callback_));
  } else {
    std::move(add_sink_callback_).Run(result_code);
  }
}
void AccessCodeCastHandler::CreateSink(AddSinkCallback callback) {
  // TODO (b/205184100): Complete implementation of creating a media sink after
  // validation
  NOTIMPLEMENTED();
}

void AccessCodeCastHandler::CastToSink(CastToSinkCallback callback) {
  // TODO (b/204572061): Complete casting implementation
  NOTIMPLEMENTED();
}
