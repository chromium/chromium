// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_decoder.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "components/image_fetcher/core/mock_image_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"

namespace image_fetcher {
namespace {

using testing::_;

TEST(ImageDecoderTest, StringAdapterRetainsBinaryData) {
  MockImageDecoder decoder;
  ImageData retained;
  ImageDecodedCallback forwarded_callback;
  bool callback_ran = false;
  const gfx::Size frame_size(16, 16);
  EXPECT_CALL(decoder, DecodeImage(_, frame_size, nullptr, _))
      .WillOnce([&](ImageData data, const gfx::Size&,
                    data_decoder::DataDecoder*, ImageDecodedCallback callback) {
        retained = std::move(data);
        forwarded_callback = std::move(callback);
      });
  {
    const std::string bytes("a\0b\xff", 4);
    // Exercise the real adapter; the mock only captures the resulting buffer.
    decoder.DecodeImageFromString(
        bytes, frame_size, nullptr,
        base::BindLambdaForTesting(
            [&](const gfx::Image&) { callback_ran = true; }));
  }
  ASSERT_TRUE(retained);
  EXPECT_EQ(base::as_string_view(base::span(*retained)),
            std::string("a\0b\xff", 4));
  EXPECT_FALSE(callback_ran);
  ASSERT_TRUE(forwarded_callback);
  std::move(forwarded_callback).Run(gfx::Image());
  EXPECT_TRUE(callback_ran);
}

TEST(ImageDecoderTest, StringAdapterPassesEmptyNonNullBuffer) {
  MockImageDecoder decoder;
  EXPECT_CALL(decoder, DecodeImage(_, _, nullptr, _))
      .WillOnce([](ImageData data, const gfx::Size&, data_decoder::DataDecoder*,
                   ImageDecodedCallback) {
        ASSERT_TRUE(data);
        EXPECT_EQ(data->size(), 0u);
      });
  decoder.DecodeImageFromString(std::string(), gfx::Size(), nullptr,
                                base::DoNothing());
}

}  // namespace
}  // namespace image_fetcher
