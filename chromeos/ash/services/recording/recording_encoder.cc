// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/recording_encoder.h"

#include "base/logging.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom-shared.h"

namespace recording {

RecordingEncoder::RecordingEncoder(OnFailureCallback on_failure_callback)
    : on_failure_callback_(std::move(on_failure_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

RecordingEncoder::~RecordingEncoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RecordingEncoder::NotifyFailure(mojom::RecordingStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (on_failure_callback_) {
    std::move(on_failure_callback_).Run(status);
  }
}

void RecordingEncoder::OnEncoderStatus(bool for_video,
                                       media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.is_ok()) {
    return;
  }

  LOG(ERROR) << "Failed to encode " << (for_video ? "video" : "audio")
             << " frame: " << status.message();
  NotifyFailure(for_video ? mojom::RecordingStatus::kVideoEncodingError
                          : mojom::RecordingStatus::kAudioEncodingError);
}

}  // namespace recording
