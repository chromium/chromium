// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/browser_renderer.h"

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/test/mock_browser_ui_interface.h"
#include "chrome/browser/vr/ui_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Return;

namespace vr {

class MockUi : public UiInterface {
 public:
  MockUi() = default;
  ~MockUi() override = default;

  base::WeakPtr<BrowserUiInterface> GetBrowserUiWeakPtr() override {
    return nullptr;
  }
  SchedulerUiInterface* GetSchedulerUiPtr() override { return nullptr; }
  MOCK_METHOD0(OnGlInitialized, void());
  MOCK_METHOD2(GetTargetPointForTesting,
               gfx::Point3F(UserFriendlyElementName,
                            const gfx::PointF& position));
  MOCK_METHOD1(GetElementVisibility, bool(UserFriendlyElementName));
  MOCK_METHOD2(OnBeginFrame, bool(base::TimeTicks, const gfx::Transform&));
  MOCK_CONST_METHOD0(SceneHasDirtyTextures, bool());
  MOCK_METHOD0(UpdateSceneTextures, void());
  MOCK_METHOD1(Draw, void(const RenderInfo&));
  MOCK_METHOD1(DrawWebVrOverlayForeground, void(const RenderInfo&));
  MOCK_METHOD0(HasWebXrOverlayElementsToDraw, bool());
  FovRectangles GetMinimalFovForWebXrOverlayElements(const gfx::Transform&,
                                                     const FovRectangle&,
                                                     const gfx::Transform&,
                                                     const FovRectangle&,
                                                     float) override {
    return {};
  }
};

class MockGraphicsDelegate : public GraphicsDelegate {
 public:
  MockGraphicsDelegate() = default;
  ~MockGraphicsDelegate() override = default;

  bool using_buffer() { return using_buffer_; }

  // GraphicsDelegate
  void Initialize(base::OnceClosure on_initialized) override {
    std::move(on_initialized).Run();
  }
  bool PreRender() override { return true; }
  void PostRender() override {}
  gfx::GpuMemoryBufferHandle GetTexture() override { NOTREACHED(); }
  gpu::SyncToken GetSyncToken() override { NOTREACHED(); }
  void ResetMemoryBuffer() override {}
  bool BindContext() override { return true; }
  void ClearContext() override {}
  void ClearBufferToBlack() override {}

 private:
  void UseBuffer() {
    CHECK(!using_buffer_);
    using_buffer_ = true;
  }
  bool using_buffer_ = false;
};

std::vector<device::mojom::XRViewPtr> GetDefaultViews() {
  int x_offset = 0;
  static int width = 128;
  static int height = 128;

  auto left = device::mojom::XRView::New();
  left->eye = device::mojom::XREye::kLeft;
  left->viewport = gfx::Rect(x_offset, 0, width, height);
  left->field_of_view =
      device::mojom::VRFieldOfView::New(45.0f, 45.0f, 45.0f, 45.0f);
  x_offset += width;

  auto right = device::mojom::XRView::New();
  right->eye = device::mojom::XREye::kRight;
  right->viewport = gfx::Rect(x_offset, 0, width, height);
  right->field_of_view =
      device::mojom::VRFieldOfView::New(45.0f, 45.0f, 45.0f, 45.0f);

  std::vector<device::mojom::XRViewPtr> views;
  views.push_back(std::move(left));
  views.push_back(std::move(right));

  return views;
}

class BrowserRendererTest : public testing::Test {
 public:
  struct BuildParams {
    std::unique_ptr<MockUi> ui;
    std::unique_ptr<MockGraphicsDelegate> graphics_delegate;
  };

  void SetUp() override {
    auto ui = std::make_unique<MockUi>();
    build_params_ = {
        std::make_unique<MockUi>(),
        std::make_unique<MockGraphicsDelegate>(),
    };
    ui_ = build_params_.ui.get();
    graphics_delegate_ = build_params_.graphics_delegate.get();
    graphics_delegate_->SetXrViews(GetDefaultViews());
  }

  std::unique_ptr<BrowserRenderer> CreateBrowserRenderer() {
    return std::make_unique<BrowserRenderer>(
        std::move(build_params_.ui),
        std::move(build_params_.graphics_delegate),
        1 /* sliding_time_size */);
  }

 protected:
  raw_ptr<MockUi, DanglingUntriaged> ui_;
  raw_ptr<MockGraphicsDelegate, DanglingUntriaged> graphics_delegate_;

 private:
  BuildParams build_params_;
};

TEST_F(BrowserRendererTest, DrawWebXrFrameNoOverlay) {
  testing::Sequence s;
  auto browser_renderer = CreateBrowserRenderer();
  EXPECT_CALL(*ui_, SceneHasDirtyTextures()).WillOnce(Return(false));
  EXPECT_CALL(*ui_, UpdateSceneTextures).Times(0);
  EXPECT_CALL(*ui_, HasWebXrOverlayElementsToDraw()).WillOnce(Return(false));

  EXPECT_CALL(*ui_, OnBeginFrame(_, _)).Times(1).InSequence(s);
  EXPECT_CALL(*ui_, Draw(_)).Times(0);
  EXPECT_CALL(*ui_, DrawWebVrOverlayForeground(_)).Times(0);
  // TODO: Replace this with something to validate that a call happened.
  // EXPECT_CALL(*scheduler_delegate_, SubmitDrawnFrame(kWebXrFrame, _))
  //     .Times(1)
  //     .InSequence(s);

  browser_renderer->DrawWebXrFrame(base::TimeTicks(), {});
  EXPECT_FALSE(graphics_delegate_->using_buffer());
}

TEST_F(BrowserRendererTest, DrawWebXrFrameWithOverlay) {
  testing::Sequence s;
  auto browser_renderer = CreateBrowserRenderer();
  EXPECT_CALL(*ui_, SceneHasDirtyTextures()).WillOnce(Return(false));
  EXPECT_CALL(*ui_, UpdateSceneTextures).Times(0);
  EXPECT_CALL(*ui_, HasWebXrOverlayElementsToDraw()).WillOnce(Return(true));

  EXPECT_CALL(*ui_, OnBeginFrame(_, _)).Times(1).InSequence(s);
  EXPECT_CALL(*ui_, Draw(_)).Times(0);
  EXPECT_CALL(*ui_, DrawWebVrOverlayForeground(_)).Times(1).InSequence(s);
  // TODO: Replace this with something to validate that a call happened.
  // EXPECT_CALL(*scheduler_delegate_, SubmitDrawnFrame(kWebXrFrame, _))
  //     .Times(1)
  //     .InSequence(s);

  browser_renderer->DrawWebXrFrame(base::TimeTicks(), {});
  EXPECT_FALSE(graphics_delegate_->using_buffer());
}

}  // namespace vr
