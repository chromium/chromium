// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca {

SpotlightFrameConsumer::SpotlightFrameConsumer(FrameReceivedCallback callback)
    : callback_(std::move(callback)) {}

SpotlightFrameConsumer::~SpotlightFrameConsumer() = default;

std::unique_ptr<webrtc::DesktopFrame> SpotlightFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  return std::make_unique<webrtc::BasicDesktopFrame>(size);
}
void SpotlightFrameConsumer::DrawFrame(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    base::OnceClosure done) {
  std::move(done).Run();

  // TODO: dorianbrandon@google.com - Process frame and send to Boca UI.
}

remoting::protocol::FrameConsumer::PixelFormat
SpotlightFrameConsumer::GetPixelFormat() {
  return FORMAT_BGRA;
}

}  // namespace ash::boca
