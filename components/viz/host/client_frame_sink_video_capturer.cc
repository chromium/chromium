// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/client_frame_sink_video_capturer.h"

#include <utility>

#include "base/bind.h"
#include "media/capture/mojom/video_capture_types.mojom.h"

namespace viz {

namespace {

// How long to wait before attempting to re-establish a lost connection.
constexpr base::TimeDelta kReEstablishConnectionDelay =
    base::TimeDelta::FromMilliseconds(100);

}  // namespace

ClientFrameSinkVideoCapturer::ClientFrameSinkVideoCapturer(
    EstablishConnectionCallback callback)
    : establish_connection_callback_(callback) {
  EstablishConnection();
}

ClientFrameSinkVideoCapturer::~ClientFrameSinkVideoCapturer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ClientFrameSinkVideoCapturer::SetFormat(media::VideoPixelFormat format,
                                             gfx::ColorSpace color_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  format_.emplace(format, color_space);
  capturer_remote_->SetFormat(format, color_space);
}

void ClientFrameSinkVideoCapturer::SetMinCapturePeriod(
    base::TimeDelta min_capture_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  min_capture_period_ = min_capture_period;
  capturer_remote_->SetMinCapturePeriod(min_capture_period);
}

void ClientFrameSinkVideoCapturer::SetMinSizeChangePeriod(
    base::TimeDelta min_period) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  min_size_change_period_ = min_period;
  capturer_remote_->SetMinSizeChangePeriod(min_period);
}

void ClientFrameSinkVideoCapturer::SetResolutionConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size,
    bool use_fixed_aspect_ratio) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  resolution_constraints_.emplace(min_size, max_size, use_fixed_aspect_ratio);
  capturer_remote_->SetResolutionConstraints(min_size, max_size,
                                             use_fixed_aspect_ratio);
}

void ClientFrameSinkVideoCapturer::SetAutoThrottlingEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto_throttling_enabled_ = enabled;
  capturer_remote_->SetAutoThrottlingEnabled(enabled);
}

void ClientFrameSinkVideoCapturer::ChangeTarget(
    const base::Optional<FrameSinkId>& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  target_ = frame_sink_id;
  capturer_remote_->ChangeTarget(frame_sink_id);
}

void ClientFrameSinkVideoCapturer::Start(
    mojom::FrameSinkVideoConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(consumer);

  is_started_ = true;
  consumer_ = consumer;
  StartInternal();
}

void ClientFrameSinkVideoCapturer::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_started_ = false;
  capturer_remote_->Stop();
}

void ClientFrameSinkVideoCapturer::StopAndResetConsumer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();
  consumer_ = nullptr;
  consumer_receiver_.reset();
}

void ClientFrameSinkVideoCapturer::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  capturer_remote_->RequestRefreshFrame();
}

std::unique_ptr<ClientFrameSinkVideoCapturer::Overlay>
ClientFrameSinkVideoCapturer::CreateOverlay(int32_t stacking_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there is an existing overlay at the same index, drop it.
  auto it = std::find_if(overlays_.begin(), overlays_.end(),
                         [&stacking_index](const Overlay* overlay) {
                           return overlay->stacking_index() == stacking_index;
                         });
  if (it != overlays_.end()) {
    (*it)->DisconnectPermanently();
    overlays_.erase(it);
  }

  auto overlay =
      std::make_unique<Overlay>(weak_factory_.GetWeakPtr(), stacking_index);
  overlays_.push_back(overlay.get());
  if (capturer_remote_)
    overlays_.back()->EstablishConnection(capturer_remote_.get());
  return overlay;
}

ClientFrameSinkVideoCapturer::Format::Format(
    media::VideoPixelFormat pixel_format,
    gfx::ColorSpace color_space)
    : pixel_format(pixel_format), color_space(color_space) {}

ClientFrameSinkVideoCapturer::ResolutionConstraints::ResolutionConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size,
    bool use_fixed_aspect_ratio)
    : min_size(min_size),
      max_size(max_size),
      use_fixed_aspect_ratio(use_fixed_aspect_ratio) {}

void ClientFrameSinkVideoCapturer::OnFrameCaptured(
    base::ReadOnlySharedMemoryRegion data,
    media::mojom::VideoFrameInfoPtr info,
    const gfx::Rect& content_rect,
    mojo::PendingRemote<mojom::FrameSinkVideoConsumerFrameCallbacks>
        callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  consumer_->OnFrameCaptured(std::move(data), std::move(info), content_rect,
                             std::move(callbacks));
}

void ClientFrameSinkVideoCapturer::OnStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  consumer_->OnStopped();
}

void ClientFrameSinkVideoCapturer::EstablishConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  capturer_remote_.reset();
  establish_connection_callback_.Run(
      capturer_remote_.BindNewPipeAndPassReceiver());
  capturer_remote_.set_disconnect_handler(
      base::BindOnce(&ClientFrameSinkVideoCapturer::OnConnectionError,
                     base::Unretained(this)));
  if (format_)
    capturer_remote_->SetFormat(format_->pixel_format, format_->color_space);
  if (min_capture_period_)
    capturer_remote_->SetMinCapturePeriod(*min_capture_period_);
  if (min_size_change_period_)
    capturer_remote_->SetMinSizeChangePeriod(*min_size_change_period_);
  if (resolution_constraints_) {
    capturer_remote_->SetResolutionConstraints(
        resolution_constraints_->min_size, resolution_constraints_->max_size,
        resolution_constraints_->use_fixed_aspect_ratio);
  }
  if (auto_throttling_enabled_)
    capturer_remote_->SetAutoThrottlingEnabled(*auto_throttling_enabled_);
  if (target_)
    capturer_remote_->ChangeTarget(target_);
  for (Overlay* overlay : overlays_)
    overlay->EstablishConnection(capturer_remote_.get());
  if (is_started_)
    StartInternal();
}

void ClientFrameSinkVideoCapturer::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ClientFrameSinkVideoCapturer::EstablishConnection,
                     weak_factory_.GetWeakPtr()),
      kReEstablishConnectionDelay);
}

void ClientFrameSinkVideoCapturer::StartInternal() {
  if (consumer_receiver_.is_bound())
    consumer_receiver_.reset();
  capturer_remote_->Start(consumer_receiver_.BindNewPipeAndPassRemote());
}

void ClientFrameSinkVideoCapturer::OnOverlayDestroyed(Overlay* overlay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto it = std::find(overlays_.begin(), overlays_.end(), overlay);
  DCHECK(it != overlays_.end());
  overlays_.erase(it);
}

ClientFrameSinkVideoCapturer::Overlay::Overlay(
    base::WeakPtr<ClientFrameSinkVideoCapturer> client_capturer,
    int32_t stacking_index)
    : client_capturer_(client_capturer), stacking_index_(stacking_index) {}

ClientFrameSinkVideoCapturer::Overlay::~Overlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (client_capturer_)
    client_capturer_->OnOverlayDestroyed(this);
}

void ClientFrameSinkVideoCapturer::Overlay::SetImageAndBounds(
    const SkBitmap& image,
    const gfx::RectF& bounds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!image.isNull());

  if (!client_capturer_)
    return;

  image_ = image;
  bounds_ = bounds;
  overlay_->SetImageAndBounds(image_, bounds_);
}

void ClientFrameSinkVideoCapturer::Overlay::SetBounds(
    const gfx::RectF& bounds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!client_capturer_)
    return;

  bounds_ = bounds;
  overlay_->SetBounds(bounds_);
}

void ClientFrameSinkVideoCapturer::Overlay::DisconnectPermanently() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  client_capturer_.reset();
  overlay_.reset();
  image_.reset();
}

void ClientFrameSinkVideoCapturer::Overlay::EstablishConnection(
    mojom::FrameSinkVideoCapturer* capturer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_capturer_);

  capturer->CreateOverlay(stacking_index_,
                          overlay_.BindNewPipeAndPassReceiver());
  // Note: There's no need to add a connection error handler on the remote. If
  // the connection to the service is lost, the ClientFrameSinkVideoCapturer
  // will realize this when the FrameSinkVideoCapturer's binding is lost, and
  // re-establish a connection to both that and this overlay.

  if (!image_.isNull())
    overlay_->SetImageAndBounds(image_, bounds_);
}

}  // namespace viz
