// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/mojom/video_accelerator_mojom_traits.h"

#include <vector>

#include "base/files/file_util.h"
#include "base/stl_util.h"
#include "components/arc/mojom/video_common.mojom.h"
#include "components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "components/arc/video_accelerator/decoder_buffer.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {
constexpr int64_t kTimestamp = 1234;
constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr media::VideoPixelFormat kFormat = media::PIXEL_FORMAT_I420;
constexpr gfx::Size kCodedSize(kWidth, kHeight);
}  // namespace

TEST(VideoAcceleratorStructTraitsTest, ConvertVideoFrameLayout) {
  std::vector<media::ColorPlaneLayout> planes;
  planes.emplace_back(kWidth, 0, kWidth * kHeight);
  planes.emplace_back(kWidth / 2, kWidth * kHeight, kWidth * kHeight / 4);
  planes.emplace_back(kWidth / 2, kWidth * kHeight + kWidth * kHeight / 4,
                      kWidth * kHeight / 4);
  // Choose a non-default value.
  constexpr size_t buffer_addr_align = 128;
  constexpr uint64_t modifier = 0x1234;

  base::Optional<media::VideoFrameLayout> layout =
      media::VideoFrameLayout::CreateWithPlanes(kFormat, kCodedSize, planes,
                                                buffer_addr_align, modifier);
  EXPECT_TRUE(layout);

  std::unique_ptr<media::VideoFrameLayout> input =
      std::make_unique<media::VideoFrameLayout>(*layout);
  std::unique_ptr<media::VideoFrameLayout> output;
  mojo::test::SerializeAndDeserialize<arc::mojom::VideoFrameLayout>(&input,
                                                                    &output);

  EXPECT_EQ(output->format(), kFormat);
  EXPECT_EQ(output->coded_size(), kCodedSize);
  EXPECT_EQ(output->planes(), planes);
  EXPECT_EQ(output->buffer_addr_align(), buffer_addr_align);
  EXPECT_EQ(output->modifier(), modifier);
}

TEST(VideoAcceleratorStructTraitsTest, ConvertNullVideoFrameLayout) {
  std::unique_ptr<media::VideoFrameLayout> input;
  std::unique_ptr<media::VideoFrameLayout> output;
  mojo::test::SerializeAndDeserialize<arc::mojom::VideoFrameLayout>(&input,
                                                                    &output);

  EXPECT_FALSE(output);
}

TEST(VideoAcceleratorStructTraitsTest, ConvertVideoFrame) {
  // We store id in the first 8 bytes of kMailbox.
  gpu::Mailbox kMailbox;
  kMailbox.name[0] = 0xff;
  kMailbox.name[1] = 0xed;
  kMailbox.name[2] = 0xfb;
  kMailbox.name[3] = 0xea;
  kMailbox.name[4] = 0xf9;
  kMailbox.name[5] = 0x7e;
  kMailbox.name[6] = 0xe5;
  kMailbox.name[7] = 0xe3;

  gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes];
  mailbox_holders[0] = gpu::MailboxHolder(kMailbox, gpu::SyncToken(), 0);

  scoped_refptr<media::VideoFrame> input =
      media::VideoFrame::WrapNativeTextures(
          kFormat, mailbox_holders, media::VideoFrame::ReleaseMailboxCB(),
          kCodedSize, gfx::Rect(kCodedSize), kCodedSize,
          base::TimeDelta::FromMilliseconds(kTimestamp));
  scoped_refptr<media::VideoFrame> output;
  mojo::test::SerializeAndDeserialize<arc::mojom::VideoFrame>(&input, &output);

  // Verify the fields of input and output frames.
  EXPECT_EQ(output->mailbox_holder(0).mailbox, kMailbox);
  EXPECT_EQ(output->visible_rect(), input->visible_rect());
  EXPECT_EQ(output->timestamp(), input->timestamp());
}

TEST(VideoAcceleratorStructTraitsTest, ConvertNullVideoFrame) {
  scoped_refptr<media::VideoFrame> input;
  scoped_refptr<media::VideoFrame> output;
  mojo::test::SerializeAndDeserialize<arc::mojom::VideoFrame>(&input, &output);

  EXPECT_FALSE(output);
}

TEST(VideoAcceleratorStructTraitsTest, ConvertDecoderBuffer) {
  const std::string kData = "TESTING_STRING";
  const uint32_t kOffset = 3;
  const uint32_t kDataSize = kData.size() - kOffset;
  constexpr bool kEndOfStream = false;

  arc::DecoderBuffer input(arc::CreateTempFileForTesting(kData), kOffset,
                           kDataSize, kEndOfStream,
                           base::TimeDelta::FromMilliseconds(kTimestamp));
  arc::DecoderBuffer output;
  mojo::test::SerializeAndDeserialize<arc::mojom::DecoderBuffer>(&input,
                                                                 &output);

  EXPECT_EQ(output.end_of_stream, input.end_of_stream);
  EXPECT_EQ(output.timestamp, input.timestamp);
  EXPECT_EQ(output.offset, input.offset);
  EXPECT_EQ(output.payload_size, input.payload_size);

  scoped_refptr<media::DecoderBuffer> buf =
      std::move(output).ToMediaDecoderBuffer();
  EXPECT_EQ(memcmp(kData.c_str() + kOffset, buf->data(), kDataSize), 0);
}

}  // namespace mojo
