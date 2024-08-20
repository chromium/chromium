// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/camera_presence_notifier/camera_presence_notifier.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/video_capture_service.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

namespace ash {

namespace {

// Interval between checks for camera presence.
const int kCameraCheckIntervalSeconds = 3;

// Adapts the CameraPresenceCallback as CameraCountCallback
CameraPresenceNotifier::CameraCountCallback Adapt(
    CameraPresenceNotifier::CameraPresenceCallback presence_callback) {
  return base::BindRepeating([](int count) { return count > 0; })
      .Then(presence_callback);
}

}  // namespace

CameraPresenceNotifier::CameraPresenceNotifier(CameraCountCallback callback)
    : callback_(callback) {
  DCHECK(callback_) << "Notifier must be created with a non-null callback.";

  content::GetVideoCaptureService().ConnectToVideoSourceProvider(
      video_source_provider_remote_.BindNewPipeAndPassReceiver());
  video_source_provider_remote_.set_disconnect_handler(base::BindOnce(
      &CameraPresenceNotifier::VideoSourceProviderDisconnectHandler,
      weak_factory_.GetWeakPtr()));
}

CameraPresenceNotifier::CameraPresenceNotifier(CameraPresenceCallback callback)
    : CameraPresenceNotifier(Adapt(callback)) {}

CameraPresenceNotifier::~CameraPresenceNotifier() {
  // video_source_provider_remote_ expects to be released on the sequence where
  // it was created.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CameraPresenceNotifier::VideoSourceProviderDisconnectHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << "VideoSourceProvider is Disconnected";
  callback_ = base::NullCallback();
}

void CameraPresenceNotifier::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Always pass through First Run on start to ensure an event is emitted ASAP.
  state_ = State::kFirstRun;
  CheckCameraPresence();
  camera_check_timer_.Start(
      FROM_HERE, base::Seconds(kCameraCheckIntervalSeconds),
      base::BindRepeating(&CameraPresenceNotifier::CheckCameraPresence,
                          weak_factory_.GetWeakPtr()));
}

void CameraPresenceNotifier::Stop() {
  state_ = State::kStopped;
  camera_check_timer_.Stop();
}

void CameraPresenceNotifier::CheckCameraPresence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_source_provider_remote_->GetSourceInfos(base::BindOnce(
      &CameraPresenceNotifier::OnGotSourceInfos, weak_factory_.GetWeakPtr()));
}

void CameraPresenceNotifier::OnGotSourceInfos(
    video_capture::mojom::VideoSourceProvider::GetSourceInfosResult,
    const std::vector<media::VideoCaptureDeviceInfo>& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const int camera_count = devices.size();

  const bool count_changed = (camera_count != camera_count_on_last_check_);
  camera_count_on_last_check_ = camera_count;

  if (state_ == State::kStopped)
    return;

  bool run_callback = (state_ == State::kFirstRun || count_changed);
  state_ = State::kStarted;
  if (callback_ && run_callback)
    callback_.Run(camera_count_on_last_check_);
}

}  // namespace ash
