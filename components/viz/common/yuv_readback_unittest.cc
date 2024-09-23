// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <tuple>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/gl_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if !BUILDFLAG(IS_ANDROID)

namespace viz {

namespace {
int kYUVReadbackSizes[] = {2, 4, 14};
}

class YUVReadbackTest : public testing::Test {
 protected:
  YUVReadbackTest() : context_(std::make_unique<gpu::GLInProcessContext>()) {
    gpu::ContextCreationAttribs attributes;
    attributes.bind_generates_resource = false;

    auto result = context_->Initialize(
        TestGpuServiceHolder::GetInstance()->task_executor(), attributes,
        gpu::SharedMemoryLimits());
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
    gl_ = context_->GetImplementation();

    gpu::ContextSupport* support = context_->GetImplementation();
    helper_ = std::make_unique<gpu::GLHelper>(gl_, support);
  }

  ~YUVReadbackTest() override = default;

  void StartTracing(const std::string& filter) {
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        base::trace_event::TraceConfig(filter,
                                       base::trace_event::RECORD_UNTIL_FULL),
        base::trace_event::TraceLog::RECORDING_MODE);
  }

  static void TraceDataCB(
      base::OnceClosure quit_closure,
      std::string* output,
      const scoped_refptr<base::RefCountedString>& json_events_str,
      bool has_more_events) {
    if (output->size() > 1 && !json_events_str->as_string().empty()) {
      output->append(",");
    }
    output->append(json_events_str->as_string());
    if (!has_more_events) {
      std::move(quit_closure).Run();
    }
  }

  // End tracing, return tracing data in a simple map
  // of event name->counts.
  void EndTracing(std::map<std::string, int>* event_counts) {
    std::string json_data = "[";
    base::trace_event::TraceLog::GetInstance()->SetDisabled();
    base::RunLoop run_loop;
    base::trace_event::TraceLog::GetInstance()->Flush(base::BindRepeating(
        &YUVReadbackTest::TraceDataCB, run_loop.QuitClosure(),
        base::Unretained(&json_data)));
    run_loop.Run();
    json_data.append("]");

    auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(json_data);
    CHECK(parsed_json.has_value())
        << "JSON parsing failed (" << parsed_json.error().message
        << ") JSON data:" << std::endl
        << json_data;

    CHECK(parsed_json->is_list());
    for (const base::Value& entry : parsed_json->GetList()) {
      const auto& dict = entry.GetDict();
      const std::string* name = dict.FindString("name");
      CHECK(name);
      const std::string* trace_type = dict.FindString("ph");
      CHECK(trace_type);
      // Count all except END traces, as they come in BEGIN/END pairs.
      if (*trace_type != "E" && *trace_type != "e")
        (*event_counts)[*name]++;
      VLOG(1) << "trace name: " << *name;
    }
  }

  // Look up a single channel value. Works for 4-channel and single channel
  // bitmaps.  Clamp x/y.
  int Channel(SkBitmap* pixels, int x, int y, int c) {
    if (pixels->bytesPerPixel() == 4) {
      uint32_t* data =
          pixels->getAddr32(std::clamp(x, 0, pixels->width() - 1),
                            std::clamp(y, 0, pixels->height() - 1));
      return (*data) >> (c * 8) & 0xff;
    } else {
      DCHECK_EQ(pixels->bytesPerPixel(), 1);
      DCHECK_EQ(c, 0);
      return *pixels->getAddr8(std::clamp(x, 0, pixels->width() - 1),
                               std::clamp(y, 0, pixels->height() - 1));
    }
  }

  // Set a single channel value. Works for 4-channel and single channel
  // bitmaps.  Clamp x/y.
  void SetChannel(SkBitmap* pixels, int x, int y, int c, int v) {
    DCHECK_GE(x, 0);
    DCHECK_GE(y, 0);
    DCHECK_LT(x, pixels->width());
    DCHECK_LT(y, pixels->height());
    if (pixels->bytesPerPixel() == 4) {
      uint32_t* data = pixels->getAddr32(x, y);
      v = std::clamp(v, 0, 255);
      *data = (*data & ~(0xffu << (c * 8))) | (v << (c * 8));
    } else {
      DCHECK_EQ(pixels->bytesPerPixel(), 1);
      DCHECK_EQ(c, 0);
      uint8_t* data = pixels->getAddr8(x, y);
      v = std::clamp(v, 0, 255);
      *data = v;
    }
  }

  // Print all the R, G, B or A values from an SkBitmap in a
  // human-readable format.
  void PrintChannel(SkBitmap* pixels, int c) {
    for (int y = 0; y < pixels->height(); y++) {
      std::string formatted;
      for (int x = 0; x < pixels->width(); x++) {
        formatted.append(base::StringPrintf("%3d, ", Channel(pixels, x, y, c)));
      }
      LOG(ERROR) << formatted;
    }
  }

  // Get a single R, G, B or A value as a float.
  float ChannelAsFloat(SkBitmap* pixels, int x, int y, int c) {
    return Channel(pixels, x, y, c) / 255.0;
  }

  // Works like a GL_LINEAR lookup on an SkBitmap.
  float Bilinear(SkBitmap* pixels, float x, float y, int c) {
    x -= 0.5;
    y -= 0.5;
    int base_x = static_cast<int>(floorf(x));
    int base_y = static_cast<int>(floorf(y));
    x -= base_x;
    y -= base_y;
    return (ChannelAsFloat(pixels, base_x, base_y, c) * (1 - x) * (1 - y) +
            ChannelAsFloat(pixels, base_x + 1, base_y, c) * x * (1 - y) +
            ChannelAsFloat(pixels, base_x, base_y + 1, c) * (1 - x) * y +
            ChannelAsFloat(pixels, base_x + 1, base_y + 1, c) * x * y);
  }

  void FlipSKBitmap(SkBitmap* bitmap) {
    int bpp = bitmap->bytesPerPixel();
    DCHECK(bpp == 4 || bpp == 1);
    int top_line = 0;
    int bottom_line = bitmap->height() - 1;
    while (top_line < bottom_line) {
      for (int x = 0; x < bitmap->width(); x++) {
        bpp == 4 ? std::swap(*bitmap->getAddr32(x, top_line),
                             *bitmap->getAddr32(x, bottom_line))
                 : std::swap(*bitmap->getAddr8(x, top_line),
                             *bitmap->getAddr8(x, bottom_line));
      }
      top_line++;
      bottom_line--;
    }
  }

  // Note: Left/Right means Top/Bottom when used for Y dimension.
  enum Margin {
    MarginLeft,
    MarginMiddle,
    MarginRight,
    MarginInvalid,
  };

  static Margin NextMargin(Margin m) {
    switch (m) {
      case MarginLeft:
        return MarginMiddle;
      case MarginMiddle:
        return MarginRight;
      case MarginRight:
        return MarginInvalid;
      default:
        return MarginInvalid;
    }
  }

  int compute_margin(int insize, int outsize, Margin m) {
    int available = outsize - insize;
    switch (m) {
      default:
        EXPECT_TRUE(false) << "This should not happen.";
        return 0;
      case MarginLeft:
        return 0;
      case MarginMiddle:
        return (available / 2) & ~1;
      case MarginRight:
        return available;
    }
  }

  // Convert 0.0 - 1.0 to 0 - 255
  int float_to_byte(float v) {
    int ret = static_cast<int>(floorf(v * 255.0f + 0.5f));
    if (ret < 0) {
      return 0;
    }
    if (ret > 255) {
      return 255;
    }
    return ret;
  }

  void PrintPlane(const unsigned char* plane,
                  int xsize,
                  int stride,
                  int ysize) {
    for (int y = 0; y < std::min(24, ysize); y++) {
      std::string formatted;
      for (int x = 0; x < std::min(24, xsize); x++) {
        formatted.append(base::StringPrintf("%3d, ", plane[y * stride + x]));
      }
      LOG(ERROR) << formatted;
    }
  }

  // Compare two planes make sure that each component of each pixel
  // is no more than |maxdiff| apart.
  void ComparePlane(const unsigned char* truth,
                    int truth_stride,
                    const unsigned char* other,
                    int other_stride,
                    int maxdiff,
                    int xsize,
                    int ysize,
                    SkBitmap* source,
                    std::string message) {
    for (int x = 0; x < xsize; x++) {
      for (int y = 0; y < ysize; y++) {
        int a = other[y * other_stride + x];
        int b = truth[y * truth_stride + x];
        EXPECT_NEAR(a, b, maxdiff)
            << " x=" << x << " y=" << y << " " << message;
        if (std::abs(a - b) > maxdiff) {
          LOG(ERROR) << "-------expected--------";
          PrintPlane(truth, xsize, truth_stride, ysize);
          LOG(ERROR) << "-------actual--------";
          PrintPlane(other, xsize, other_stride, ysize);
          if (source) {
            LOG(ERROR) << "-------before yuv conversion: red--------";
            PrintChannel(source, 0);
            LOG(ERROR) << "-------before yuv conversion: green------";
            PrintChannel(source, 1);
            LOG(ERROR) << "-------before yuv conversion: blue-------";
            PrintChannel(source, 2);
          }
          return;
        }
      }
    }
  }

  // YUV readback test. Create a test pattern, convert to YUV
  // with reference implementation and compare to what gl_helper
  // returns.
  void TestYUVReadback(int xsize,
                       int ysize,
                       int output_xsize,
                       int output_ysize,
                       int xmargin,
                       int ymargin,
                       int test_pattern,
                       bool flip,
                       bool use_mrt,
                       gpu::GLHelper::ScalerQuality quality) {
    GLuint src_texture;
    gl_->GenTextures(1, &src_texture);
    SkBitmap input_pixels;
    input_pixels.allocN32Pixels(xsize, ysize);

    for (int x = 0; x < xsize; ++x) {
      for (int y = 0; y < ysize; ++y) {
        switch (test_pattern) {
          case 0:  // Smooth test pattern
            SetChannel(&input_pixels, x, y, 0, x * 10);
            SetChannel(&input_pixels, x, y, 1, y * 10);
            SetChannel(&input_pixels, x, y, 2, (x + y) * 10);
            SetChannel(&input_pixels, x, y, 3, 255);
            break;
          case 1:  // Small blocks
            SetChannel(&input_pixels, x, y, 0, x & 1 ? 255 : 0);
            SetChannel(&input_pixels, x, y, 1, y & 1 ? 255 : 0);
            SetChannel(&input_pixels, x, y, 2, (x + y) & 1 ? 255 : 0);
            SetChannel(&input_pixels, x, y, 3, 255);
            break;
          case 2:  // Medium blocks
            SetChannel(&input_pixels, x, y, 0, 10 + x / 2 * 50);
            SetChannel(&input_pixels, x, y, 1, 10 + y / 3 * 50);
            SetChannel(&input_pixels, x, y, 2, (x + y) / 5 * 50 + 5);
            SetChannel(&input_pixels, x, y, 3, 255);
            break;
        }
      }
    }

    gl_->BindTexture(GL_TEXTURE_2D, src_texture);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, xsize, ysize, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, input_pixels.getPixels());

    std::string message = base::StringPrintf(
        "input size: %dx%d "
        "output size: %dx%d "
        "margin: %dx%d "
        "pattern: %d %s %s",
        xsize, ysize, output_xsize, output_ysize, xmargin, ymargin,
        test_pattern, flip ? "flip" : "noflip", use_mrt ? "mrt" : "nomrt");
    std::unique_ptr<gpu::ReadbackYUVInterface> yuv_reader =
        helper_->CreateReadbackPipelineYUV(flip, use_mrt);

    scoped_refptr<media::VideoFrame> output_frame =
        media::VideoFrame::CreateFrame(
            media::PIXEL_FORMAT_I420,
            // The coded size of the output frame is rounded up to the next
            // 16-byte boundary.  This tests that the readback is being
            // positioned inside the frame's visible region, and not dependent
            // on its coded size.
            gfx::Size((output_xsize + 15) & ~15, (output_ysize + 15) & ~15),
            gfx::Rect(0, 0, output_xsize, output_ysize),
            gfx::Size(output_xsize, output_ysize), base::Seconds(0));
    scoped_refptr<media::VideoFrame> truth_frame =
        media::VideoFrame::CreateFrame(
            media::PIXEL_FORMAT_I420, gfx::Size(output_xsize, output_ysize),
            gfx::Rect(0, 0, output_xsize, output_ysize),
            gfx::Size(output_xsize, output_ysize), base::Seconds(0));

    base::RunLoop run_loop;
    auto run_quit_closure = [](base::OnceClosure quit_closure, bool result) {
      std::move(quit_closure).Run();
    };
    yuv_reader->ReadbackYUV(
        src_texture, gfx::Size(xsize, ysize), gfx::Rect(0, 0, xsize, ysize),
        output_frame->stride(media::VideoFrame::Plane::kY),
        output_frame->writable_data(media::VideoFrame::Plane::kY),
        output_frame->stride(media::VideoFrame::Plane::kU),
        output_frame->writable_data(media::VideoFrame::Plane::kU),
        output_frame->stride(media::VideoFrame::Plane::kV),
        output_frame->writable_data(media::VideoFrame::Plane::kV),
        gfx::Point(xmargin, ymargin),
        base::BindOnce(run_quit_closure, run_loop.QuitClosure()));

    const gfx::Rect paste_rect(gfx::Point(xmargin, ymargin),
                               gfx::Size(xsize, ysize));
    media::LetterboxVideoFrame(output_frame.get(), paste_rect);
    run_loop.Run();

    if (flip) {
      FlipSKBitmap(&input_pixels);
    }

    unsigned char* Y =
        truth_frame->GetWritableVisibleData(media::VideoFrame::Plane::kY);
    unsigned char* U =
        truth_frame->GetWritableVisibleData(media::VideoFrame::Plane::kU);
    unsigned char* V =
        truth_frame->GetWritableVisibleData(media::VideoFrame::Plane::kV);
    int32_t y_stride = truth_frame->stride(media::VideoFrame::Plane::kY);
    int32_t u_stride = truth_frame->stride(media::VideoFrame::Plane::kU);
    int32_t v_stride = truth_frame->stride(media::VideoFrame::Plane::kV);
    memset(Y, 0x00, y_stride * output_ysize);
    memset(U, 0x80, u_stride * output_ysize / 2);
    memset(V, 0x80, v_stride * output_ysize / 2);

    const float kRGBtoYColorWeights[] = {0.257f, 0.504f, 0.098f, 0.0625f};
    const float kRGBtoUColorWeights[] = {-0.148f, -0.291f, 0.439f, 0.5f};
    const float kRGBtoVColorWeights[] = {0.439f, -0.368f, -0.071f, 0.5f};

    for (int y = 0; y < ysize; y++) {
      for (int x = 0; x < xsize; x++) {
        Y[(y + ymargin) * y_stride + x + xmargin] = float_to_byte(
            ChannelAsFloat(&input_pixels, x, y, 0) * kRGBtoYColorWeights[0] +
            ChannelAsFloat(&input_pixels, x, y, 1) * kRGBtoYColorWeights[1] +
            ChannelAsFloat(&input_pixels, x, y, 2) * kRGBtoYColorWeights[2] +
            kRGBtoYColorWeights[3]);
      }
    }

    for (int y = 0; y < ysize / 2; y++) {
      for (int x = 0; x < xsize / 2; x++) {
        U[(y + ymargin / 2) * u_stride + x + xmargin / 2] =
            float_to_byte(Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 0) *
                              kRGBtoUColorWeights[0] +
                          Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 1) *
                              kRGBtoUColorWeights[1] +
                          Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 2) *
                              kRGBtoUColorWeights[2] +
                          kRGBtoUColorWeights[3]);
        V[(y + ymargin / 2) * v_stride + x + xmargin / 2] =
            float_to_byte(Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 0) *
                              kRGBtoVColorWeights[0] +
                          Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 1) *
                              kRGBtoVColorWeights[1] +
                          Bilinear(&input_pixels, x * 2 + 1.0, y * 2 + 1.0, 2) *
                              kRGBtoVColorWeights[2] +
                          kRGBtoVColorWeights[3]);
      }
    }

    ComparePlane(
        Y, y_stride, output_frame->visible_data(media::VideoFrame::Plane::kY),
        output_frame->stride(media::VideoFrame::Plane::kY), 2, output_xsize,
        output_ysize, &input_pixels, message + " Y plane");
    ComparePlane(
        U, u_stride, output_frame->visible_data(media::VideoFrame::Plane::kU),
        output_frame->stride(media::VideoFrame::Plane::kU), 2, output_xsize / 2,
        output_ysize / 2, &input_pixels, message + " U plane");
    ComparePlane(
        V, v_stride, output_frame->visible_data(media::VideoFrame::Plane::kV),
        output_frame->stride(media::VideoFrame::Plane::kV), 2, output_xsize / 2,
        output_ysize / 2, &input_pixels, message + " V plane");

    gl_->DeleteTextures(1, &src_texture);
  }

  std::unique_ptr<gpu::GLInProcessContext> context_;
  raw_ptr<gpu::gles2::GLES2Interface> gl_;
  std::unique_ptr<gpu::GLHelper> helper_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
};

TEST_F(YUVReadbackTest, YUVReadbackOptTest) {
  for (int use_mrt = 0; use_mrt <= 1; ++use_mrt) {
    // This test uses the gpu.service/gpu.decoder tracing events to detect how
    // many scaling passes are actually performed by the YUV readback pipeline.
    StartTracing(TRACE_DISABLED_BY_DEFAULT(
        "gpu.service") "," TRACE_DISABLED_BY_DEFAULT("gpu.decoder"));

    // Run a test with no size scaling, just planerization.
    TestYUVReadback(800, 400, 800, 400, 0, 0, 1, false, use_mrt == 1,
                    gpu::GLHelper::SCALER_QUALITY_FAST);

    std::map<std::string, int> event_counts;
    EndTracing(&event_counts);
    int draw_buffer_calls = event_counts["kDrawBuffersEXTImmediate"];
    int draw_arrays_calls = event_counts["kDrawArrays"];
    VLOG(1) << "Draw buffer calls: " << draw_buffer_calls;
    VLOG(1) << "DrawArrays calls: " << draw_arrays_calls;

    if (use_mrt) {
      // When using MRT, the YUV readback code should only execute two
      // glDrawArrays(). It will call glDrawBuffersEXT() twice for each pass
      // (once to draw to multiple outputs, and once to restore back to a single
      // output).
      EXPECT_EQ(2, draw_arrays_calls);
      EXPECT_EQ(4, draw_buffer_calls);
    } else {
      // When not using MRT, there are three passes for the YUV.
      // glDrawBuffersEXT() should never be called because none of the
      // planerizers should draw multiple outputs.
      EXPECT_EQ(3, draw_arrays_calls);
      EXPECT_EQ(0, draw_buffer_calls);
    }
  }
}

class YUVReadbackPixelTest
    : public YUVReadbackTest,
      public ::testing::WithParamInterface<
          std::tuple<bool, bool, unsigned int, unsigned int>> {};

TEST_P(YUVReadbackPixelTest, Test) {
  bool flip = std::get<0>(GetParam());
  bool use_mrt = std::get<1>(GetParam());
  unsigned int x = std::get<2>(GetParam());
  unsigned int y = std::get<3>(GetParam());

  for (unsigned int ox = x; ox < std::size(kYUVReadbackSizes); ox++) {
    for (unsigned int oy = y; oy < std::size(kYUVReadbackSizes); oy++) {
      // If output is a subsection of the destination frame, (letterbox)
      // then try different variations of where the subsection goes.
      for (Margin xm = x < ox ? MarginLeft : MarginRight; xm <= MarginRight;
           xm = NextMargin(xm)) {
        for (Margin ym = y < oy ? MarginLeft : MarginRight; ym <= MarginRight;
             ym = NextMargin(ym)) {
          for (int pattern = 0; pattern < 3; pattern++) {
            TestYUVReadback(
                kYUVReadbackSizes[x], kYUVReadbackSizes[y],
                kYUVReadbackSizes[ox], kYUVReadbackSizes[oy],
                compute_margin(kYUVReadbackSizes[x], kYUVReadbackSizes[ox], xm),
                compute_margin(kYUVReadbackSizes[y], kYUVReadbackSizes[oy], ym),
                pattern, flip, use_mrt, gpu::GLHelper::SCALER_QUALITY_GOOD);
            if (HasFailure()) {
              return;
            }
          }
        }
      }
    }
  }
}

// First argument is intentionally empty.
INSTANTIATE_TEST_SUITE_P(
    All,
    YUVReadbackPixelTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Range<unsigned int>(0, std::size(kYUVReadbackSizes)),
        ::testing::Range<unsigned int>(0, std::size(kYUVReadbackSizes))));

}  // namespace viz

#endif
