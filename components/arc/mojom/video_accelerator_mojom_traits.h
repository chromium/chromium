// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_MOJOM_VIDEO_ACCELERATOR_MOJOM_TRAITS_H_
#define COMPONENTS_ARC_MOJOM_VIDEO_ACCELERATOR_MOJOM_TRAITS_H_

#include <string.h>

#include <memory>

#include "components/arc/mojom/arc_gfx_mojom_traits.h"
#include "components/arc/mojom/video_common.mojom.h"
#include "components/arc/video_accelerator/decoder_buffer.h"
#include "components/arc/video_accelerator/video_frame_plane.h"
#include "media/base/color_plane_layout.h"
#include "media/base/decode_status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::VideoCodecProfile, media::VideoCodecProfile> {
  static arc::mojom::VideoCodecProfile ToMojom(media::VideoCodecProfile input);

  static bool FromMojom(arc::mojom::VideoCodecProfile input,
                        media::VideoCodecProfile* output);
};

template <>
struct StructTraits<arc::mojom::VideoFramePlaneDataView, arc::VideoFramePlane> {
  static int32_t offset(const arc::VideoFramePlane& r) {
    DCHECK_GE(r.offset, 0);
    return r.offset;
  }

  static int32_t stride(const arc::VideoFramePlane& r) {
    DCHECK_GE(r.stride, 0);
    return r.stride;
  }

  static bool Read(arc::mojom::VideoFramePlaneDataView data,
                   arc::VideoFramePlane* out);
};

template <>
struct StructTraits<arc::mojom::SizeDataView, gfx::Size> {
  static int width(const gfx::Size& r) {
    DCHECK_GE(r.width(), 0);
    return r.width();
  }

  static int height(const gfx::Size& r) {
    DCHECK_GE(r.height(), 0);
    return r.height();
  }

  static bool Read(arc::mojom::SizeDataView data, gfx::Size* out);
};

template <>
struct EnumTraits<arc::mojom::VideoPixelFormat, media::VideoPixelFormat> {
  static arc::mojom::VideoPixelFormat ToMojom(media::VideoPixelFormat input);

  static bool FromMojom(arc::mojom::VideoPixelFormat input,
                        media::VideoPixelFormat* output);
};

template <>
struct StructTraits<arc::mojom::ColorPlaneLayoutDataView,
                    media::ColorPlaneLayout> {
  static int32_t stride(const media::ColorPlaneLayout& r) { return r.stride; }

  static uint32_t offset(const media::ColorPlaneLayout& r) { return r.offset; }

  static uint32_t size(const media::ColorPlaneLayout& r) { return r.size; }

  static bool Read(arc::mojom::ColorPlaneLayoutDataView data,
                   media::ColorPlaneLayout* out);
};

// Because `media::VideoFrameLayout` doesn't have default constructor, we cannot
// convert from mojo struct directly. Instead, we map to the type
// `std::unique_ptr<media::VideoFrameLayout>`.
template <>
struct StructTraits<arc::mojom::VideoFrameLayoutDataView,
                    std::unique_ptr<media::VideoFrameLayout>> {
  static bool IsNull(const std::unique_ptr<media::VideoFrameLayout>& input) {
    return input.get() == nullptr;
  }

  static void SetToNull(std::unique_ptr<media::VideoFrameLayout>* output) {
    output->reset();
  }

  static media::VideoPixelFormat format(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->format();
  }

  static const gfx::Size& coded_size(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->coded_size();
  }

  static const std::vector<media::ColorPlaneLayout>& planes(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->planes();
  }

  static bool is_multi_planar(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->is_multi_planar();
  }

  static uint32_t buffer_addr_align(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->buffer_addr_align();
  }

  static uint64_t modifier(
      const std::unique_ptr<media::VideoFrameLayout>& input) {
    DCHECK(input);
    return input->modifier();
  }

  static bool Read(arc::mojom::VideoFrameLayoutDataView data,
                   std::unique_ptr<media::VideoFrameLayout>* out);
};

template <>
struct EnumTraits<arc::mojom::DecodeStatus, media::DecodeStatus> {
  static arc::mojom::DecodeStatus ToMojom(media::DecodeStatus input);

  static bool FromMojom(arc::mojom::DecodeStatus input,
                        media::DecodeStatus* output);
};

template <>
struct StructTraits<arc::mojom::VideoFrameDataView,
                    scoped_refptr<media::VideoFrame>> {
  static bool IsNull(const scoped_refptr<media::VideoFrame> input) {
    return !input;
  }

  static void SetToNull(scoped_refptr<media::VideoFrame>* output) {
    output->reset();
  }

  static uint64_t id(const scoped_refptr<media::VideoFrame> input) {
    DCHECK(input);
    DCHECK(!input->mailbox_holder(0).mailbox.IsZero());

    // We store id at the first 8 byte of the mailbox.
    uint64_t id;
    static_assert(GL_MAILBOX_SIZE_CHROMIUM >= sizeof(id),
                  "Size of Mailbox is too small to store id.");
    const int8_t* const name = input->mailbox_holder(0).mailbox.name;
    memcpy(&id, name, sizeof(id));
    return id;
  }

  static gfx::Rect visible_rect(const scoped_refptr<media::VideoFrame> input) {
    DCHECK(input);

    return input->visible_rect();
  }

  static int64_t timestamp(const scoped_refptr<media::VideoFrame> input) {
    DCHECK(input);

    return input->timestamp().InMilliseconds();
  }

  static bool Read(arc::mojom::VideoFrameDataView data,
                   scoped_refptr<media::VideoFrame>* out);
};

template <>
struct StructTraits<arc::mojom::DecoderBufferDataView, arc::DecoderBuffer> {
  static mojo::ScopedHandle handle_fd(arc::DecoderBuffer& input) {
    return mojo::WrapPlatformHandle(
        mojo::PlatformHandle(std::move(input.handle_fd)));
  }

  static uint32_t offset(const arc::DecoderBuffer& input) {
    return input.offset;
  }

  static uint32_t payload_size(const arc::DecoderBuffer& input) {
    return input.payload_size;
  }

  static bool end_of_stream(const arc::DecoderBuffer& input) {
    return input.end_of_stream;
  }

  static int64_t timestamp(const arc::DecoderBuffer& input) {
    return input.timestamp.InMilliseconds();
  }

  static bool Read(arc::mojom::DecoderBufferDataView data,
                   arc::DecoderBuffer* out);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_MOJOM_VIDEO_ACCELERATOR_MOJOM_TRAITS_H_
