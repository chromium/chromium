// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/fake_image_decoder.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace image_fetcher {

FakeImageDecoder::FakeImageDecoder() : enabled_(true), valid_(true) {}
FakeImageDecoder::~FakeImageDecoder() = default;

void FakeImageDecoder::DecodeImage(
    const std::string& image_data,
    const gfx::Size& desired_image_frame_size,
    const image_fetcher::ImageDecodedCallback& callback) {
  ASSERT_TRUE(enabled_);
  gfx::Image image;
  if (valid_ && !image_data.empty()) {
    image = gfx::test::CreateImage(2, 3);
  }

  if (before_image_decoded_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     before_image_decoded_);
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(callback, image));
}

void FakeImageDecoder::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

void FakeImageDecoder::SetDecodingValid(bool valid) {
  valid_ = valid;
}

void FakeImageDecoder::SetBeforeImageDecoded(
    const base::RepeatingClosure& callback) {
  before_image_decoded_ = callback;
}

}  // namespace image_fetcher
