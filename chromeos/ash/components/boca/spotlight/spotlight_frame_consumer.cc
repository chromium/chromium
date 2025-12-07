// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/spotlight/spotlight_frame_consumer.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "remoting/protocol/frame_consumer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace ash::boca {

namespace {
SkBitmap GenerateBitmap(const webrtc::DesktopFrame& frame) {
  SkBitmap bitmap;
  bitmap.setInfo(
      SkImageInfo::MakeN32(frame.rect().width(), frame.rect().height(),
                           SkAlphaType::kOpaque_SkAlphaType),
      frame.stride());
  bitmap.setPixels(frame.data());
  bitmap.setImmutable();

  return bitmap;
}

}  // namespace

SpotlightFrameConsumer::SpotlightFrameConsumer(FrameReceivedCallback callback)
    : callback_(std::move(callback)) {}

SpotlightFrameConsumer::~SpotlightFrameConsumer() = default;

std::unique_ptr<webrtc::DesktopFrame> SpotlightFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  return std::make_unique<webrtc::BasicDesktopFrame>(size, webrtc::FOURCC_ARGB);
}

void SpotlightFrameConsumer::DrawFrame(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    base::OnceClosure done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The generated `SkBitmap` keeps a reference to the pixel memory, however
  // the `webrtc::DesktopFrame` maintains ownership of the pixels. Therefore
  // we must keep the frame alive as long as we need the bitmap.
  SkBitmap bitmap = GenerateBitmap(*frame);
  callback_.Run(std::move(bitmap), std::move(frame));

  std::move(done).Run();
}

remoting::protocol::FrameConsumer::PixelFormat
SpotlightFrameConsumer::GetPixelFormat() {
  return FORMAT_RGBA;
}

}  // namespace ash::boca
