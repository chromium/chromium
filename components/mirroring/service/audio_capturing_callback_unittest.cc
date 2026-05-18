// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/audio_capturing_callback.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

TEST(AudioCapturingCallbackTest, DataCallbackCalled) {
  base::test::TaskEnvironment task_environment;

  bool data_callback_called = false;
  auto data_callback = base::BindRepeating(
      [](bool* called, std::unique_ptr<media::AudioBus> audio_bus,
         base::TimeTicks recorded_time) { *called = true; },
      &data_callback_called);

  mojo::Remote<mojom::SessionObserver> observer;

  AudioCapturingCallback callback(data_callback, base::DoNothing(), observer);

  auto audio_bus = media::AudioBus::Create(2, 480);
  base::TimeTicks now = base::TimeTicks::Now();

  media::AudioCapturerSource::CaptureCallback* capture_callback = &callback;
  capture_callback->Capture(audio_bus.get(), now, media::AudioGlitchInfo(),
                            1.0);

  EXPECT_TRUE(data_callback_called);
}

TEST(AudioCapturingCallbackTest, ErrorCallbackCalled) {
  base::test::TaskEnvironment task_environment;

  bool error_callback_called = false;
  auto error_callback =
      base::BindOnce([](bool* called, std::string message) { *called = true; },
                     &error_callback_called);

  mojo::Remote<mojom::SessionObserver> observer;

  AudioCapturingCallback callback(base::DoNothing(), std::move(error_callback),
                                  observer);

  media::AudioCapturerSource::CaptureCallback* capture_callback = &callback;
  capture_callback->OnCaptureError(
      media::AudioCapturerSource::ErrorCode::kUnknown, "test error");

  EXPECT_TRUE(error_callback_called);
}

}  // namespace mirroring
