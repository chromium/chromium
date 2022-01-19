// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"

#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner_util.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/media_sink_discovery_metrics.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/discovery/media_sink_service_base.h"

using ::media_router::CreateAccessCodeMediaSink;
using SinkSource = ::media_router::CastDeviceCountMetrics::SinkSource;

// TODO(b/213324920): Remove WebUI from the media_router namespace after
// expiration module has been completed.
namespace media_router {

AccessCodeCastHandler::AccessCodeCastHandler(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    Profile* profile,
    MediaRouter* media_router,
    const media_router::CastModeSet& cast_mode_set,
    content::WebContents* web_contents)
    : AccessCodeCastHandler(std::move(page_handler),
                            std::move(page),
                            profile,
                            media_router,
                            cast_mode_set,
                            web_contents,
                            media_router::DualMediaSinkService::GetInstance()
                                ->GetCastMediaSinkServiceImpl()) {
  DCHECK(profile_);
}

AccessCodeCastHandler::AccessCodeCastHandler(
    mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
    mojo::PendingRemote<access_code_cast::mojom::Page> page,
    Profile* profile,
    MediaRouter* media_router,
    const media_router::CastModeSet& cast_mode_set,
    content::WebContents* web_contents,
    CastMediaSinkServiceImpl* cast_media_sink_service_impl)
    : page_(std::move(page)),
      receiver_(this, std::move(page_handler)),
      profile_(profile),
      media_router_(media_router),
      cast_mode_set_(cast_mode_set),
      web_contents_(web_contents),
      cast_media_sink_service_impl_(cast_media_sink_service_impl) {
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
  // Check to see if the media sink already exists in the media router.
  base::PostTaskAndReplyWithResult(
      cast_media_sink_service_impl_->task_runner().get(), FROM_HERE,
      base::BindOnce(&CastMediaSinkServiceImpl::HasSink,
                     base::Unretained(cast_media_sink_service_impl_),
                     creation_result.first.value().id()),
      base::BindOnce(&AccessCodeCastHandler::HandleSinkPresentInMediaRouter,
                     weak_ptr_factory_.GetWeakPtr(),
                     creation_result.first.value()));
}

void AccessCodeCastHandler::SetSinkCallbackForTesting(
    AddSinkCallback callback) {
  add_sink_callback_ = std::move(callback);
}

void AccessCodeCastHandler::HandleSinkPresentInMediaRouter(
    MediaSinkInternal media_sink,
    bool has_sink) {
  if (has_sink) {
    std::move(add_sink_callback_).Run(AddSinkResultCode::OK);
    return;
  }
  AccessCodeCastHandler::AddSinkToMediaRouter(media_sink);
}

void AccessCodeCastHandler::AddSinkToMediaRouter(MediaSinkInternal media_sink) {
  // TODO (b/201430609): Complete addition to media_router. Additionally must
  // add an observer to the cast_media_sink_service_impl that waits for the
  // actual channel to successfully open. The observer will trigger the
  // AddSinkCallback stored in the class.
  cast_media_sink_service_impl_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastMediaSinkServiceImpl::OpenChannel,
                                base::Unretained(cast_media_sink_service_impl_),
                                media_sink, nullptr, SinkSource::kAccessCode));
  std::move(add_sink_callback_).Run(AddSinkResultCode::OK);
}

void AccessCodeCastHandler::CastToSink(CastToSinkCallback callback) {
  // TODO (b/204572061): Complete casting implementation.
  NOTIMPLEMENTED();
}
}  // namespace media_router
