// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/scheduler.h"

#include "components/viz/service/display_embedder/skia_output_surface_dependency_impl.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gl/gl_surface_stub.h"

using ::testing::_;
using ::testing::Expectation;
using ::testing::Ne;
using ::testing::Return;

namespace {

// These MACRO and TestOnGpu class make it easier to write tests that runs on
// GPU Thread
// Use TEST_F_GPU instead of TEST_F in the same manner and in your subclass
// of TestOnGpu implement SetUpOnMain/SetUpOnGpu and
// TearDownOnMain/TearDownOnGpu instead of SetUp and TearDown respectively.
//
// NOTE: Most likely you need to implement TearDownOnGpu instead of relying on
// destructor to ensure that necessary cleanup happens on GPU Thread.

// TODO(vasilyt): Extract this for others to use?

#define GTEST_TEST_GPU_(test_suite_name, test_name, parent_class, parent_id)  \
  class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                    \
      : public parent_class {                                                 \
   public:                                                                    \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() {}                   \
                                                                              \
   private:                                                                   \
    virtual void TestBodyOnGpu();                                             \
    static ::testing::TestInfo* const test_info_ GTEST_ATTRIBUTE_UNUSED_;     \
    GTEST_DISALLOW_COPY_AND_ASSIGN_(GTEST_TEST_CLASS_NAME_(test_suite_name,   \
                                                           test_name));       \
  };                                                                          \
                                                                              \
  ::testing::TestInfo* const GTEST_TEST_CLASS_NAME_(test_suite_name,          \
                                                    test_name)::test_info_ =  \
      ::testing::internal::MakeAndRegisterTestInfo(                           \
          #test_suite_name, #test_name, nullptr, nullptr,                     \
          ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id), \
          ::testing::internal::SuiteApiResolver<                              \
              parent_class>::GetSetUpCaseOrSuite(__FILE__, __LINE__),         \
          ::testing::internal::SuiteApiResolver<                              \
              parent_class>::GetTearDownCaseOrSuite(__FILE__, __LINE__),      \
          new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(    \
              test_suite_name, test_name)>);                                  \
  void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBodyOnGpu()

#define TEST_F_GPU(test_fixture, test_name)              \
  GTEST_TEST_GPU_(test_fixture, test_name, test_fixture, \
                  ::testing::internal::GetTypeId<test_fixture>())

class TestOnGpu : public ::testing::Test {
 protected:
  TestOnGpu()
      : wait_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void TestBody() override {
    auto callback =
        base::BindLambdaForTesting([&]() { this->TestBodyOnGpu(); });
    ScheduleGpuTask(std::move(callback));
  }

  void SetUp() override {
    gpu_service_holder_ = viz::TestGpuServiceHolder::GetInstance();
    SetUpOnMain();

    auto setup = base::BindLambdaForTesting([&]() { this->SetUpOnGpu(); });
    ScheduleGpuTask(setup);
  }

  void TearDown() override {
    auto teardown =
        base::BindLambdaForTesting([&]() { this->TearDownOnGpu(); });
    ScheduleGpuTask(teardown);

    TearDownOnMain();
  }

  void CallOnGpuAndUnblockMain(base::OnceClosure callback) {
    DCHECK(!wait_.IsSignaled());
    std::move(callback).Run();
    wait_.Signal();
  }

  void ScheduleGpuTask(base::OnceClosure callback) {
    auto wrap = base::BindOnce(&TestOnGpu::CallOnGpuAndUnblockMain,
                               base::Unretained(this), std::move(callback));
    gpu_service_holder_->ScheduleGpuTask(std::move(wrap));
    wait_.Wait();
  }

  virtual void SetUpOnMain() {}
  virtual void SetUpOnGpu() {}
  virtual void TearDownOnMain() {}
  virtual void TearDownOnGpu() {}
  virtual void TestBodyOnGpu() {}

  viz::TestGpuServiceHolder* gpu_service_holder_;
  base::WaitableEvent wait_;
};

// Here starts SkiaOutputDeviceBufferQueue test related code

class MockGLSurfaceAsync : public gl::GLSurfaceStub {
 public:
  bool SupportsAsyncSwap() override { return true; }

  void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                        PresentationCallback presentation_callback) override {
    DCHECK(!callback_);
    callback_ = std::move(completion_callback);
  }

  void SwapComplete() {
    std::move(callback_).Run(gfx::SwapResult::SWAP_ACK, nullptr);
  }

 protected:
  ~MockGLSurfaceAsync() override {}
  SwapCompletionCallback callback_;
};

}  // namespace

namespace viz {

class SkiaOutputDeviceBufferQueueTest : public TestOnGpu {
 public:
  SkiaOutputDeviceBufferQueueTest() {}

  void SetUpOnMain() override {
    gpu::SurfaceHandle surface_handle_ = gpu::kNullSurfaceHandle;
    dependency_ = std::make_unique<SkiaOutputSurfaceDependencyImpl>(
        gpu_service_holder_->gpu_service(), surface_handle_);
  }

  void SetUpOnGpu() override {
    gl_surface_ = base::MakeRefCounted<MockGLSurfaceAsync>();

    auto present_callback =
        base::DoNothing::Repeatedly<gpu::SwapBuffersCompleteParams,
                                    const gfx::Size&>();

    uint32_t shared_image_usage =
        gpu::SHARED_IMAGE_USAGE_DISPLAY |
        gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;

    std::unique_ptr<SkiaOutputDeviceBufferQueue> onscreen_device =
        std::make_unique<SkiaOutputDeviceBufferQueue>(
            gl_surface_, dependency_.get(), present_callback,
            shared_image_usage);

    output_device_ = std::move(onscreen_device);
  }

  void TearDownOnGpu() override { output_device_.reset(); }

  using Image = SkiaOutputDeviceBufferQueue::Image;

  Image* current_image() { return output_device_->current_image_.get(); }

  const std::vector<std::unique_ptr<Image>>& available_images() {
    return output_device_->available_images_;
  }

  Image* displayed_image() { return output_device_->displayed_image_.get(); }

  base::circular_deque<std::unique_ptr<Image>>& in_flight_images() {
    return output_device_->in_flight_images_;
  }

  int CountBuffers() {
    int n = available_images().size() + in_flight_images().size();

    if (displayed_image())
      n++;
    if (current_image())
      n++;
    return n;
  }

  void CheckUnique() {
    std::set<Image*> images;
    for (const auto& image : available_images())
      images.insert(image.get());
    for (const auto& image : in_flight_images())
      images.insert(image.get());

    if (displayed_image())
      images.insert(displayed_image());

    if (current_image())
      images.insert(current_image());

    EXPECT_EQ(images.size(), (size_t)CountBuffers());
  }

  Image* GetCurrentImage() { return output_device_->GetCurrentImage(); }

  void SwapBuffers() {
    auto present_callback =
        base::DoNothing::Once<const gfx::PresentationFeedback&>();

    output_device_->SwapBuffers(std::move(present_callback),
                                std::vector<ui::LatencyInfo>());
  }

  void PageFlipComplete() { gl_surface_->SwapComplete(); }

 protected:
  std::unique_ptr<SkiaOutputSurfaceDependency> dependency_;
  scoped_refptr<MockGLSurfaceAsync> gl_surface_;
  std::unique_ptr<SkiaOutputDeviceBufferQueue> output_device_;
};

namespace {

const gfx::Size screen_size = gfx::Size(30, 30);

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, MultipleGetCurrentBufferCalls) {
  // Check that multiple bind calls do not create or change surfaces.

  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false,
                          gfx::OVERLAY_TRANSFORM_NONE);
  EXPECT_NE(GetCurrentImage(), nullptr);
  EXPECT_EQ(1, CountBuffers());
  auto* fb = current_image();
  EXPECT_NE(GetCurrentImage(), nullptr);
  EXPECT_EQ(1, CountBuffers());
  EXPECT_EQ(fb, current_image());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckDoubleBuffering) {
  // Check buffer flow through double buffering path.
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false,
                          gfx::OVERLAY_TRANSFORM_NONE);
  EXPECT_EQ(0, CountBuffers());

  EXPECT_NE(GetCurrentImage(), nullptr);
  EXPECT_EQ(1, CountBuffers());
  EXPECT_NE(current_image(), nullptr);
  EXPECT_FALSE(displayed_image());
  SwapBuffers();
  EXPECT_EQ(1U, in_flight_images().size());
  PageFlipComplete();
  EXPECT_EQ(0U, in_flight_images().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(GetCurrentImage(), nullptr);
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_NE(current_image(), nullptr);
  EXPECT_EQ(0U, in_flight_images().size());
  EXPECT_TRUE(displayed_image());
  SwapBuffers();
  CheckUnique();
  EXPECT_EQ(1U, in_flight_images().size());
  EXPECT_TRUE(displayed_image());
  PageFlipComplete();
  CheckUnique();
  EXPECT_EQ(0U, in_flight_images().size());
  EXPECT_EQ(1U, available_images().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(GetCurrentImage(), nullptr);
  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_TRUE(available_images().empty());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckTripleBuffering) {
  // Check buffer flow through triple buffering path.
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false,
                          gfx::OVERLAY_TRANSFORM_NONE);

  // This bit is the same sequence tested in the doublebuffering case.
  EXPECT_NE(GetCurrentImage(), nullptr);
  EXPECT_FALSE(displayed_image());
  SwapBuffers();
  PageFlipComplete();
  EXPECT_NE(GetCurrentImage(), nullptr);
  SwapBuffers();

  EXPECT_EQ(2, CountBuffers());
  CheckUnique();
  EXPECT_EQ(1U, in_flight_images().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_NE(GetCurrentImage(), nullptr);
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(current_image(), nullptr);
  EXPECT_EQ(1U, in_flight_images().size());
  EXPECT_TRUE(displayed_image());
  PageFlipComplete();
  EXPECT_EQ(3, CountBuffers());
  CheckUnique();
  EXPECT_NE(current_image(), nullptr);
  EXPECT_EQ(0U, in_flight_images().size());
  EXPECT_TRUE(displayed_image());
  EXPECT_EQ(1U, available_images().size());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckEmptySwap) {
  // Check empty swap flow, in which the damage is empty and BindFramebuffer
  // might not be called.
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false,
                          gfx::OVERLAY_TRANSFORM_NONE);

  EXPECT_EQ(0, CountBuffers());
  auto* image = GetCurrentImage();
  EXPECT_NE(image, nullptr);
  EXPECT_EQ(1, CountBuffers());
  EXPECT_NE(current_image(), nullptr);
  EXPECT_FALSE(displayed_image());

  SwapBuffers();
  // Make sure we won't be drawing to the texture we just sent for scanout.
  auto* new_image = GetCurrentImage();
  EXPECT_NE(new_image, nullptr);
  EXPECT_NE(image, new_image);

  EXPECT_EQ(1U, in_flight_images().size());
  PageFlipComplete();

  // Test swapbuffers without calling BeginPaint/EndPaint (i.e without
  // GetCurrentImage)
  SwapBuffers();
  EXPECT_EQ(1U, in_flight_images().size());
  PageFlipComplete();
  EXPECT_EQ(0U, in_flight_images().size());

  EXPECT_EQ(current_image(), nullptr);
  SwapBuffers();
  EXPECT_EQ(1U, in_flight_images().size());
  PageFlipComplete();
  EXPECT_EQ(0U, in_flight_images().size());
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, CheckCorrectBufferOrdering) {
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false,
                          gfx::OVERLAY_TRANSFORM_NONE);
  const size_t kSwapCount = 5;

  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_NE(GetCurrentImage(), nullptr);
    SwapBuffers();
    EXPECT_NE(GetCurrentImage(), nullptr);
    PageFlipComplete();
  }

  // Note: this must be three, not kSwapCount
  EXPECT_EQ(3, CountBuffers());

  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_NE(GetCurrentImage(), nullptr);
    SwapBuffers();
    EXPECT_EQ(1U, in_flight_images().size());
    auto* next_image = in_flight_images().front().get();
    PageFlipComplete();
    EXPECT_EQ(displayed_image(), next_image);
  }
}

TEST_F_GPU(SkiaOutputDeviceBufferQueueTest, ReshapeWithInFlightSurfaces) {
  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false,
                          gfx::OVERLAY_TRANSFORM_NONE);

  const size_t kSwapCount = 5;

  for (size_t i = 0; i < kSwapCount; ++i) {
    EXPECT_NE(GetCurrentImage(), nullptr);
    SwapBuffers();
    EXPECT_NE(GetCurrentImage(), nullptr);
    PageFlipComplete();
  }

  SwapBuffers();

  output_device_->Reshape(screen_size, 1.0f, gfx::ColorSpace(), false,
                          gfx::OVERLAY_TRANSFORM_NONE);
  EXPECT_EQ(1u, in_flight_images().size());

  PageFlipComplete();
  EXPECT_FALSE(displayed_image());

  // The dummy surfacess left should be discarded.
  EXPECT_EQ(0u, available_images().size());

  // Test swap after reshape
  EXPECT_NE(GetCurrentImage(), nullptr);
  SwapBuffers();
  PageFlipComplete();
  EXPECT_NE(displayed_image(), nullptr);
}

}  // namespace
}  // namespace viz
