// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_scaler.h"

#include "cc/test/pixel_test.h"
#include "components/viz/common/gl_scaler_test_util.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::Sequence;

namespace viz {
namespace {

class MockContextProvider : public ContextProvider {
 public:
  MockContextProvider() {
    ON_CALL(*this, ContextGL())
        .WillByDefault(
            Return(reinterpret_cast<gpu::gles2::GLES2Interface*>(0xdeadbeef)));
    ON_CALL(*this, ContextCapabilities()).WillByDefault(ReturnRef(caps_));
  }

  MOCK_METHOD1(AddObserver, void(ContextLostObserver* obs));
  MOCK_METHOD1(RemoveObserver, void(ContextLostObserver* obs));
  MOCK_CONST_METHOD0(ContextCapabilities, const gpu::Capabilities&());
  MOCK_METHOD0(ContextGL, gpu::gles2::GLES2Interface*());

  // Stubbed-out, because the tests just stack-allocate this object.
  void AddRef() const final {}
  void Release() const final {}

 private:
  gpu::Capabilities caps_;

  // Other ContextProvider methods; but stubbed-out because they are never
  // called.
  gpu::ContextResult BindToCurrentThread() final {
    NOTREACHED();
    return gpu::ContextResult::kSuccess;
  }
  base::Lock* GetLock() final {
    NOTREACHED();
    return nullptr;
  }
  ContextCacheController* CacheController() final {
    NOTREACHED();
    return nullptr;
  }
  gpu::ContextSupport* ContextSupport() final {
    NOTREACHED();
    return nullptr;
  }
  class GrDirectContext* GrContext() final {
    NOTREACHED();
    return nullptr;
  }
  gpu::SharedImageInterface* SharedImageInterface() final {
    NOTREACHED();
    return nullptr;
  }
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const final {
    NOTREACHED();
    return *reinterpret_cast<gpu::GpuFeatureInfo*>(0xdeadbeef);
  }
};

class GLScalerTest : public cc::PixelTest {
 protected:
  void SetUp() final {
    cc::PixelTest::SetUpGLWithoutRenderer(gfx::SurfaceOrigin::kBottomLeft);
  }

  void TearDown() final { cc::PixelTest::TearDown(); }
};

TEST_F(GLScalerTest, AddAndRemovesSelfAsContextLossObserver) {
  NiceMock<MockContextProvider> provider;
  ContextLostObserver* registered_observer = nullptr;
  Sequence s;
  EXPECT_CALL(provider, AddObserver(NotNull()))
      .InSequence(s)
      .WillOnce(SaveArg<0>(&registered_observer));
  EXPECT_CALL(provider, RemoveObserver(Eq(ByRef(registered_observer))))
      .InSequence(s);
  GLScaler scaler(&provider);
}

TEST_F(GLScalerTest, RemovesObserverWhenContextIsLost) {
  NiceMock<MockContextProvider> provider;
  ContextLostObserver* registered_observer = nullptr;
  Sequence s;
  EXPECT_CALL(provider, AddObserver(NotNull()))
      .InSequence(s)
      .WillOnce(SaveArg<0>(&registered_observer));
  EXPECT_CALL(provider, RemoveObserver(Eq(ByRef(registered_observer))))
      .InSequence(s);
  GLScaler scaler(&provider);
  static_cast<ContextLostObserver&>(scaler).OnContextLost();
  // Verify RemoveObserver() was called before |scaler| goes out-of-scope.
  Mock::VerifyAndClearExpectations(&provider);
}

TEST_F(GLScalerTest, StopsScalingWhenContextIsLost) {
  GLScaler scaler(context_provider());

  // Configure the scaler with default parameters (1:1 scale ratio).
  ASSERT_TRUE(scaler.Configure(GLScaler::Parameters()));

  // Call Scale() and expect it to return true to indicate the operation
  // succeeded.
  GLScalerTestTextureHelper helper(context_provider()->ContextGL());
  constexpr gfx::Size kSomeSize = gfx::Size(32, 32);
  const GLuint src_texture = helper.CreateTexture(kSomeSize);
  const GLuint dest_texture = helper.CreateTexture(kSomeSize);
  EXPECT_TRUE(scaler.Scale(src_texture, kSomeSize, gfx::Vector2d(),
                           dest_texture, gfx::Rect(kSomeSize)));

  // After the context is lost, another call to Scale() should return false.
  static_cast<ContextLostObserver&>(scaler).OnContextLost();
  EXPECT_FALSE(scaler.Scale(src_texture, kSomeSize, gfx::Vector2d(),
                            dest_texture, gfx::Rect(kSomeSize)));
}

TEST_F(GLScalerTest, Configure_RequiresValidScalingVectors) {
  GLScaler scaler(context_provider());

  GLScaler::Parameters params;
  EXPECT_TRUE(scaler.Configure(params));

  for (int i = 0; i < 4; ++i) {
    params.scale_from = gfx::Vector2d(i == 0 ? 0 : 1, i == 1 ? 0 : 1);
    params.scale_to = gfx::Vector2d(i == 2 ? 0 : 1, i == 3 ? 0 : 1);
    EXPECT_FALSE(scaler.Configure(params));
  }
}

TEST_F(GLScalerTest, Configure_ResolvesUnspecifiedColorSpaces) {
  GLScaler scaler(context_provider());

  // Neither source nor output space specified: Both should resolve to sRGB.
  GLScaler::Parameters params;
  EXPECT_TRUE(scaler.Configure(params));
  const auto srgb = gfx::ColorSpace::CreateSRGB();
  EXPECT_EQ(srgb, scaler.params().source_color_space);
  EXPECT_EQ(srgb, scaler.params().output_color_space);
  EXPECT_TRUE(GLScaler::ParametersAreEquivalent(params, scaler.params()));

  // Source space set to XYZD50 with no output space specified: Both should
  // resolve to XYZD50.
  const auto xyzd50 = gfx::ColorSpace::CreateXYZD50();
  params.source_color_space = xyzd50;
  EXPECT_TRUE(scaler.Configure(params));
  EXPECT_EQ(xyzd50, scaler.params().source_color_space);
  EXPECT_EQ(xyzd50, scaler.params().output_color_space);
  EXPECT_TRUE(GLScaler::ParametersAreEquivalent(params, scaler.params()));

  // Source space set to XYZD50 with output space set to P3D65: Nothing should
  // change.
  const auto p3d65 = gfx::ColorSpace::CreateDisplayP3D65();
  params.output_color_space = p3d65;
  EXPECT_TRUE(scaler.Configure(params));
  EXPECT_EQ(xyzd50, scaler.params().source_color_space);
  EXPECT_EQ(p3d65, scaler.params().output_color_space);
  EXPECT_TRUE(GLScaler::ParametersAreEquivalent(params, scaler.params()));
}

TEST_F(GLScalerTest, Configure_RequiresValidSwizzles) {
  GLScaler scaler(context_provider());
  GLScaler::Parameters params;

  // Test that all valid combinations work.
  for (int i = 0; i < 4; ++i) {
    params.swizzle[0] = (i % 2 == 0) ? GL_RGBA : GL_BGRA_EXT;
    params.swizzle[1] = (i / 2 == 0) ? GL_RGBA : GL_BGRA_EXT;
    EXPECT_TRUE(scaler.Configure(params)) << "i=" << i;
  }

  // Test that invalid combinations don't work.
  for (int i = 1; i < 4; ++i) {
    params.swizzle[0] = (i % 2 == 0) ? GL_RGBA : GL_RGB;
    params.swizzle[1] = (i / 2 == 0) ? GL_RGBA : GL_RGB;
    EXPECT_FALSE(scaler.Configure(params)) << "i=" << i;
  }
}

TEST_F(GLScalerTest, DetectsEquivalentScaleRatios) {
  GLScaler::Parameters params;
  EXPECT_TRUE(GLScaler::ParametersHasSameScaleRatio(params, gfx::Vector2d(1, 1),
                                                    gfx::Vector2d(1, 1)));
  EXPECT_TRUE(GLScaler::ParametersHasSameScaleRatio(
      params, gfx::Vector2d(15, 15), gfx::Vector2d(15, 15)));

  params.scale_from = gfx::Vector2d(2, 1);
  EXPECT_TRUE(GLScaler::ParametersHasSameScaleRatio(params, gfx::Vector2d(2, 1),
                                                    gfx::Vector2d(1, 1)));
  EXPECT_TRUE(GLScaler::ParametersHasSameScaleRatio(
      params, gfx::Vector2d(30, 15), gfx::Vector2d(15, 15)));

  params.scale_from = gfx::Vector2d(1, 2);
  EXPECT_TRUE(GLScaler::ParametersHasSameScaleRatio(params, gfx::Vector2d(1, 2),
                                                    gfx::Vector2d(1, 1)));
  EXPECT_TRUE(GLScaler::ParametersHasSameScaleRatio(
      params, gfx::Vector2d(15, 30), gfx::Vector2d(15, 15)));

  params.scale_from = gfx::Vector2d(2, 1);
  EXPECT_FALSE(GLScaler::ParametersHasSameScaleRatio(
      params, gfx::Vector2d(1, 2), gfx::Vector2d(1, 1)));
  EXPECT_FALSE(GLScaler::ParametersHasSameScaleRatio(
      params, gfx::Vector2d(15, 30), gfx::Vector2d(15, 15)));

  params.scale_from = gfx::Vector2d(1, 2);
  EXPECT_FALSE(GLScaler::ParametersHasSameScaleRatio(
      params, gfx::Vector2d(2, 1), gfx::Vector2d(1, 1)));
  EXPECT_FALSE(GLScaler::ParametersHasSameScaleRatio(
      params, gfx::Vector2d(30, 15), gfx::Vector2d(15, 15)));
}

}  // namespace
}  // namespace viz
