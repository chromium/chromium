// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file looks like a unit test, but it contains benchmarks and test
// utilities intended for manual evaluation of the scalers in
// gl_helper*. These tests produce output in the form of files and printouts,
// but cannot really "fail". There is no point in making these tests part
// of any test automation run.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stddef.h>
#include <stdio.h>

#include <cmath>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/viz/common/gl_helper.h"
#include "components/viz/common/gl_helper_scaling.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/gfx/codec/png_codec.h"

namespace viz {

namespace {

GLHelper::ScalerQuality kQualities[] = {
    GLHelper::SCALER_QUALITY_BEST, GLHelper::SCALER_QUALITY_GOOD,
    GLHelper::SCALER_QUALITY_FAST,
};

const char* const kQualityNames[] = {
    "best", "good", "fast",
};

}  // namespace

class GLHelperBenchmark : public testing::Test {
 protected:
  void SetUp() override {
    gpu::ContextCreationAttribs attributes;
    attributes.alpha_size = 8;
    attributes.depth_size = 24;
    attributes.red_size = 8;
    attributes.green_size = 8;
    attributes.blue_size = 8;
    attributes.stencil_size = 8;
    attributes.samples = 4;
    attributes.sample_buffers = 1;
    attributes.bind_generates_resource = false;
    attributes.gpu_preference = gl::GpuPreference::kHighPerformance;

    context_ = std::make_unique<gpu::GLInProcessContext>();
    auto result = context_->Initialize(
        TestGpuServiceHolder::GetInstance()->task_executor(),
        nullptr,                 /* surface */
        true,                    /* offscreen */
        gpu::kNullSurfaceHandle, /* window */
        attributes, gpu::SharedMemoryLimits(),
        nullptr, /* gpu_memory_buffer_manager */
        nullptr, /* image_factory */
        base::ThreadTaskRunnerHandle::Get());
    DCHECK_EQ(result, gpu::ContextResult::kSuccess);
    gl_ = context_->GetImplementation();
    gpu::ContextSupport* support = context_->GetImplementation();

    helper_.reset(new GLHelper(gl_, support));
    helper_scaling_.reset(new GLHelperScaling(gl_, helper_.get()));
  }

  void TearDown() override {
    helper_scaling_.reset(nullptr);
    helper_.reset(nullptr);
    context_.reset(nullptr);
  }

  void LoadPngFileToSkBitmap(const base::FilePath& filename, SkBitmap* bitmap) {
    std::string compressed;
    base::ReadFileToString(base::MakeAbsoluteFilePath(filename), &compressed);
    ASSERT_TRUE(compressed.size());
    ASSERT_TRUE(gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(compressed.data()),
        compressed.size(), bitmap));
  }

  // Save the image to a png file. Used to create the initial test files.
  void SaveToFile(SkBitmap* bitmap, const base::FilePath& filename) {
    std::vector<unsigned char> compressed;
    ASSERT_TRUE(gfx::PNGCodec::Encode(
        static_cast<unsigned char*>(bitmap->getPixels()),
        gfx::PNGCodec::FORMAT_BGRA,
        gfx::Size(bitmap->width(), bitmap->height()),
        static_cast<int>(bitmap->rowBytes()), true,
        std::vector<gfx::PNGCodec::Comment>(), &compressed));
    ASSERT_TRUE(compressed.size());
    FILE* f = base::OpenFile(filename, "wb");
    ASSERT_TRUE(f);
    ASSERT_EQ(fwrite(&*compressed.begin(), 1, compressed.size(), f),
              compressed.size());
    base::CloseFile(f);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
  gpu::gles2::GLES2Interface* gl_;
  std::unique_ptr<GLHelper> helper_;
  std::unique_ptr<GLHelperScaling> helper_scaling_;
  base::circular_deque<GLHelperScaling::ScaleOp> x_ops_, y_ops_;
};

TEST_F(GLHelperBenchmark, ScaleBenchmark) {
  int output_sizes[] = {1920, 1080, 1249, 720,  // Output size on pixel
                        256,  144};
  int input_sizes[] = {3200, 2040, 2560, 1476,  // Pixel tab size
                       1920, 1080, 1280, 720,  800, 480, 256, 144};

  for (size_t q = 0; q < base::size(kQualities); q++) {
    for (size_t outsize = 0; outsize < base::size(output_sizes); outsize += 2) {
      for (size_t insize = 0; insize < base::size(input_sizes); insize += 2) {
        uint32_t src_texture;
        gl_->GenTextures(1, &src_texture);
        uint32_t dst_texture;
        gl_->GenTextures(1, &dst_texture);
        uint32_t framebuffer;
        gl_->GenFramebuffers(1, &framebuffer);
        const gfx::Size src_size(input_sizes[insize], input_sizes[insize + 1]);
        const gfx::Size dst_size(output_sizes[outsize],
                                 output_sizes[outsize + 1]);
        SkBitmap input;
        input.allocN32Pixels(src_size.width(), src_size.height());

        SkBitmap output_pixels;
        output_pixels.allocN32Pixels(dst_size.width(), dst_size.height());

        gl_->BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        gl_->BindTexture(GL_TEXTURE_2D, dst_texture);
        gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dst_size.width(),
                        dst_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        nullptr);
        gl_->BindTexture(GL_TEXTURE_2D, src_texture);
        gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src_size.width(),
                        src_size.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                        input.getPixels());

        std::unique_ptr<GLHelper::ScalerInterface> scaler =
            helper_->CreateScaler(
                kQualities[q],
                gfx::Vector2d(src_size.width(), src_size.height()),
                gfx::Vector2d(dst_size.width(), dst_size.height()), false,
                false, false);
        // Scale once beforehand before we start measuring.
        const gfx::Rect output_rect(dst_size);
        scaler->Scale(src_texture, src_size, gfx::Vector2dF(), dst_texture,
                      output_rect);
        gl_->Finish();

        base::TimeTicks start_time = base::TimeTicks::Now();
        int iterations = 0;
        base::TimeTicks end_time;
        while (true) {
          for (int i = 0; i < 50; i++) {
            iterations++;
            scaler->Scale(src_texture, src_size, gfx::Vector2dF(), dst_texture,
                          output_rect);
            gl_->Flush();
          }
          gl_->Finish();
          end_time = base::TimeTicks::Now();
          if (iterations > 2000) {
            break;
          }
          if ((end_time - start_time).InMillisecondsF() > 1000) {
            break;
          }
        }
        gl_->DeleteTextures(1, &dst_texture);
        gl_->DeleteTextures(1, &src_texture);
        gl_->DeleteFramebuffers(1, &framebuffer);

        std::string name;
        name = base::StringPrintf("scale_%dx%d_to_%dx%d_%s", src_size.width(),
                                  src_size.height(), dst_size.width(),
                                  dst_size.height(), kQualityNames[q]);

        float ms = (end_time - start_time).InMillisecondsF() / iterations;
        VLOG(0) << base::StringPrintf("*RESULT gpu_scale_time: %s=%.2f ms\n",
                                      name.c_str(), ms);
      }
    }
  }
}

}  // namespace viz
