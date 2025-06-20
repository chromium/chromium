// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_FRAME_CONSUMER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_FRAME_CONSUMER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "remoting/protocol/frame_consumer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace webrtc {
class DesktopFrame;
}

namespace ash::boca {
// Allocates and receives frames from a CRD client session.
class SpotlightFrameConsumer : public remoting::protocol::FrameConsumer {
 public:
  using FrameReceivedCallback = base::RepeatingCallback<
      void(SkBitmap, std::unique_ptr<webrtc::DesktopFrame> frame)>;

  explicit SpotlightFrameConsumer(FrameReceivedCallback callback);

  SpotlightFrameConsumer(const SpotlightFrameConsumer&) = delete;
  SpotlightFrameConsumer& operator=(const SpotlightFrameConsumer&) = delete;

  ~SpotlightFrameConsumer() override;

  // remoting::protocol::FrameConsumer interface.
  std::unique_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override;
  void DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                 base::OnceClosure done) override;
  PixelFormat GetPixelFormat() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  FrameReceivedCallback callback_;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_FRAME_CONSUMER_H_
