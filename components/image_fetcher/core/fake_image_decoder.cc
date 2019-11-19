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

FakeImageDecoder::FakeImageDecoder(const FakeImageDecoder& other) {
  enabled_ = other.enabled_;
  valid_ = other.valid_;
  before_image_decoded_ = other.before_image_decoded_;
  decoded_image_ = other.decoded_image_;
}

FakeImageDecoder::~FakeImageDecoder() = default;

void FakeImageDecoder::DecodeImage(
    const std::string& image_data,
    const gfx::Size& desired_image_frame_size,
    image_fetcher::ImageDecodedCallback callback) {
  ASSERT_TRUE(enabled_);
  gfx::Image image;
  if (valid_ && decoded_image_.IsEmpty() && !image_data.empty()) {
    decoded_image_ = gfx::test::CreateImage(2, 3);
  }

  if (before_image_decoded_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     before_image_decoded_);
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), decoded_image_));
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

void FakeImageDecoder::SetDecodedImage(const gfx::Image& image) {
  decoded_image_ = image;
}

}  // namespace image_fetcher
