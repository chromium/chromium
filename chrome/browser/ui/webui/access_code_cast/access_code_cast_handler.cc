// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "components/media_router/common/discovery/media_sink_internal.h"

using ::media_router::CreateAccessCodeMediaSink;

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
    AddSinkResultCode result_code) {
  if (result_code != AddSinkResultCode::OK) {
    std::move(add_sink_callback_).Run(result_code);
    return;
  }
  if (!discovery_device.has_value()) {
    std::move(add_sink_callback_).Run(AddSinkResultCode::EMPTY_RESPONSE);
    return;
  }
  std::pair<absl::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
      creation_result = CreateAccessCodeMediaSink(discovery_device.value());

  if (!creation_result.first.has_value() ||
      creation_result.second != CreateCastMediaSinkResult::kOk) {
    std::move(add_sink_callback_).Run(AddSinkResultCode::SINK_CREATION_ERROR);
    return;
  }
  std::move(add_sink_callback_)
      .Run(AccessCodeCastHandler::AddSinkToMediaRouter(
          creation_result.first.value()));
}

void AccessCodeCastHandler::SetSinkCallbackForTesting(
    AddSinkCallback callback) {
  add_sink_callback_ = std::move(callback);
}

AddSinkResultCode AccessCodeCastHandler::AddSinkToMediaRouter(
    MediaSinkInternal media_sink) {
  // TODO (b/201430609): Complete addition to media_router.
  NOTIMPLEMENTED();
  return AddSinkResultCode::OK;
}

void AccessCodeCastHandler::CastToSink(CastToSinkCallback callback) {
  // TODO (b/204572061): Complete casting implementation.
  NOTIMPLEMENTED();
}
