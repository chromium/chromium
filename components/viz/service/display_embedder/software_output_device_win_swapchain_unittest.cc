// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_win_swapchain.h"

#include <windows.h>

#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "base/win/scoped_gdi_object.h"
#include "base/win/windows_types.h"
#include "components/viz/service/display_embedder/output_device_backing.h"
#include "components/viz/service/display_embedder/test_support/dcomp_mocks.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/base/win/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
namespace {

constexpr gfx::Size kTestSize(1024, 768);

class MockOutputDeviceBacking : public OutputDeviceBacking {
 public:
  MockOutputDeviceBacking() = default;
  ~MockOutputDeviceBacking() override = default;

  MOCK_METHOD(Microsoft::WRL::ComPtr<ID3D11Texture2D>,
              GetOrCreateStagingTexture,
              (),
              (override));
  MOCK_METHOD(HRESULT,
              GetOrCreateDXObjects,
              (Microsoft::WRL::ComPtr<ID3D11Device> * d3d11_device,
               Microsoft::WRL::ComPtr<IDXGIFactory2>* dxgi_factory,
               Microsoft::WRL::ComPtr<IDCompositionDevice>* dcomp_device),
              (override));
};

class TestSoftwareOutputDeviceWinSwapChain
    : public SoftwareOutputDeviceWinSwapChain {
 public:
  TestSoftwareOutputDeviceWinSwapChain(HWND hwnd,
                                       HWND& child_hwnd,
                                       OutputDeviceBacking* output_backing)
      : SoftwareOutputDeviceWinSwapChain(hwnd, child_hwnd, output_backing) {}

  // Override to avoid actual window operations.
  bool UpdateWindowSize(const gfx::Size& viewport_pixel_size) override {
    return resize_result_;
  }

  void set_resize_result(bool result) { resize_result_ = result; }

 private:
  bool resize_result_ = true;
};

class TestWindowClass {
 public:
  TestWindowClass() : window_(nullptr), child_window_(nullptr) {
    // Create a temporary window for testing
    window_ = CreateWindowEx(0, L"STATIC", L"Test Window", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr,
                             nullptr, GetModuleHandle(nullptr), nullptr);
  }

  ~TestWindowClass() { DestroyWindow(window_); }

  HWND window() const { return window_; }
  HWND child_window() const { return child_window_; }
  void set_child_window(HWND hwnd) { child_window_ = hwnd; }

 private:
  HWND window_;
  HWND child_window_;
};

class SoftwareOutputDeviceWinSwapChainTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_output_backing_ =
        std::make_unique<testing::NiceMock<MockOutputDeviceBacking>>();
    mock_d3d11_device_ =
        media::MakeComPtr<testing::NiceMock<media::D3D11DeviceMock>>();
    mock_dxgi_factory_ =
        media::MakeComPtr<testing::NiceMock<media::DXGIFactory2Mock>>();
    mock_dcomp_device_ =
        media::MakeComPtr<testing::NiceMock<media::DCompositionDeviceMock>>();
    mock_dxgi_swapchain_ =
        media::MakeComPtr<testing::NiceMock<media::DXGISwapChain1Mock>>();
    mock_d3d11_device_context_ =
        media::MakeComPtr<testing::NiceMock<media::D3D11DeviceContextMock>>();
    mock_dcomp_visual_ =
        media::MakeComPtr<testing::NiceMock<media::DCompositionVisualMock>>();
    mock_dcomp_target_ =
        media::MakeComPtr<testing::NiceMock<media::DCompositionTargetMock>>();

    // Allow leak for the mock DCompositionDevice since it is prevented from
    // being reset in the event of unexpected gpu process exit to allow for a
    // final commit. Also allow leak for the mock DXGISwapChain because the
    // DCompositionDevice maintains a reference to it via the root visual.
    testing::Mock::AllowLeak(mock_dcomp_device_.Get());
    testing::Mock::AllowLeak(mock_dxgi_swapchain_.Get());

    ON_CALL(*mock_output_backing_, GetOrCreateDXObjects)
        .WillByDefault(
            [this](Microsoft::WRL::ComPtr<ID3D11Device>* d3d11_device,
                   Microsoft::WRL::ComPtr<IDXGIFactory2>* dxgi_factory,
                   Microsoft::WRL::ComPtr<IDCompositionDevice>* dcomp_device) {
              *d3d11_device = mock_d3d11_device_;
              *dxgi_factory = mock_dxgi_factory_;
              *dcomp_device = mock_dcomp_device_;
              return S_OK;
            });
    HWND child_hwnd = nullptr;
    device_ = std::make_unique<TestSoftwareOutputDeviceWinSwapChain>(
        window_class_.window(), child_hwnd, mock_output_backing_.get());
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(mock_dxgi_swapchain_.Get());
  }

  void SetSwapChainExpectations() {
    ON_CALL(*mock_dcomp_device_.Get(), CreateTargetForHwnd)
        .WillByDefault(
            media::SetComPointeeAndReturnOk<2>(mock_dcomp_target_.Get()));
    ON_CALL(*mock_dcomp_device_.Get(), CreateVisual)
        .WillByDefault(
            media::SetComPointeeAndReturnOk<0>(mock_dcomp_visual_.Get()));
    ON_CALL(*mock_d3d11_device_.Get(), GetImmediateContext)
        .WillByDefault(
            media::SetComPointee<0>(mock_d3d11_device_context_.Get()));
    ON_CALL(*mock_dxgi_factory_.Get(), CreateSwapChainForComposition)
        .WillByDefault(
            media::SetComPointeeAndReturnOk<3>(mock_dxgi_swapchain_.Get()));

    // Device should start with no D3D resources.
    EXPECT_FALSE(device_->HasSwapChainForTesting());
    EXPECT_FALSE(device_->HasDeviceContextForTesting());
    EXPECT_CALL(*mock_output_backing_, GetOrCreateDXObjects).Times(1);
    EXPECT_EQ(device_->GetViewportPixelSize(), gfx::Size(0, 0));
  }

  void DoResize(gfx::Size size) {
    device_->Resize(size, 1.f);
    EXPECT_EQ(device_->GetViewportPixelSize(), size);
    EXPECT_TRUE(device_->HasSwapChainForTesting());
    EXPECT_TRUE(device_->HasDeviceContextForTesting());
  }

  void DoBeginPaint(gfx::Size size) {
    Microsoft::WRL::ComPtr<media::D3D11Texture2DMock>
        mock_d3d11_staging_texture =
            media::MakeComPtr<testing::NiceMock<media::D3D11Texture2DMock>>();
    EXPECT_CALL(*mock_output_backing_, GetOrCreateStagingTexture)
        .WillOnce(testing::Return(mock_d3d11_staging_texture));
    auto desc = D3D11_TEXTURE2D_DESC{
        .Width = static_cast<UINT>(size.width()),
        .Height = static_cast<UINT>(size.height()),
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Usage = D3D11_USAGE_STAGING,
        .BindFlags = 0,
        .CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE};
    EXPECT_CALL(*mock_d3d11_staging_texture.Get(), GetDesc)
        .WillOnce(
            [desc](D3D11_TEXTURE2D_DESC* descriptor) { *descriptor = desc; });
    EXPECT_CALL(*mock_d3d11_device_context_.Get(), Map)
        .WillOnce(testing::Return(S_OK));
    SkCanvas* canvas = device_->BeginPaintDelegated();
    auto image_info = canvas->imageInfo();
    EXPECT_EQ(image_info.width(), size.width());
    EXPECT_EQ(image_info.height(), size.height());
  }

  TestWindowClass window_class_;
  std::unique_ptr<testing::NiceMock<MockOutputDeviceBacking>>
      mock_output_backing_;
  std::unique_ptr<TestSoftwareOutputDeviceWinSwapChain> device_;

  Microsoft::WRL::ComPtr<media::DXGIFactory2Mock> mock_dxgi_factory_;
  Microsoft::WRL::ComPtr<media::D3D11DeviceMock> mock_d3d11_device_;
  Microsoft::WRL::ComPtr<media::DCompositionDeviceMock> mock_dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> mock_dcomp_target_;
  Microsoft::WRL::ComPtr<media::D3D11DeviceContextMock>
      mock_d3d11_device_context_;
  Microsoft::WRL::ComPtr<IDCompositionVisual> mock_dcomp_visual_;
  Microsoft::WRL::ComPtr<media::DXGISwapChain1Mock> mock_dxgi_swapchain_;
};

}  // namespace

// Test that ResizeDelegated creates D3D resources when none exist.
TEST_F(SoftwareOutputDeviceWinSwapChainTest,
       ResizeDelegatedCreatesD3DResources) {
  SetSwapChainExpectations();
  DoResize(kTestSize);
}

// Test that ResizeDelegated handles SetWindowPos failure.
TEST_F(SoftwareOutputDeviceWinSwapChainTest, SetWindowPosFailure) {
  // Set the resize to fail
  device_->set_resize_result(false);
  EXPECT_EQ(device_->GetViewportPixelSize(), gfx::Size(0, 0));
  device_->Resize(kTestSize, 1.f);
  EXPECT_EQ(device_->GetViewportPixelSize(), gfx::Size(0, 0));
}

// Test that ResizeDelegated fails without process crash if CreateTargetForHwnd
// fails due to E_INVALIDARG. Such a scenario can occur if the window is deleted
// before the resize is complete.
TEST_F(SoftwareOutputDeviceWinSwapChainTest, CreateTargetForHwndInvalidArg) {
  EXPECT_CALL(*mock_dcomp_device_.Get(), CreateTargetForHwnd)
      .WillOnce(testing::Return(E_INVALIDARG));
  EXPECT_EQ(device_->GetViewportPixelSize(), gfx::Size(0, 0));
  device_->Resize(kTestSize, 1.f);
  EXPECT_EQ(device_->GetViewportPixelSize(), gfx::Size(0, 0));
}

// Test the entire paint pipeline.
TEST_F(SoftwareOutputDeviceWinSwapChainTest, Paint) {
  SetSwapChainExpectations();
  // Ensure present gets called in EndPaint.
  ON_CALL(*mock_dxgi_swapchain_.Get(), Present1)
      .WillByDefault(testing::Return(S_OK));

  DoResize(kTestSize);
  DoBeginPaint(kTestSize);
  device_->EndPaintDelegated(
      gfx::Rect(0, 0, kTestSize.width(), kTestSize.height()));
}

// Test that Map fails without process crash if the device is removed.
TEST_F(SoftwareOutputDeviceWinSwapChainTest, FailedMapDueToDeviceRemoved) {
  SetSwapChainExpectations();
  EXPECT_CALL(*mock_d3d11_device_context_.Get(), Map)
      .WillOnce(testing::Return(DXGI_ERROR_DEVICE_REMOVED));
  EXPECT_CALL(*mock_d3d11_device_.Get(), GetDeviceRemovedReason)
      .WillOnce(testing::Return(DXGI_ERROR_DEVICE_REMOVED));

  DoResize(kTestSize);

  Microsoft::WRL::ComPtr<media::D3D11Texture2DMock> mock_d3d11_staging_texture =
      media::MakeComPtr<testing::NiceMock<media::D3D11Texture2DMock>>();
  EXPECT_CALL(*mock_output_backing_, GetOrCreateStagingTexture)
      .WillOnce(testing::Return(mock_d3d11_staging_texture));
  // There should be no GPU process termination.
  SkCanvas* canvas = device_->BeginPaintDelegated();
  EXPECT_FALSE(!!canvas);
}

TEST_F(SoftwareOutputDeviceWinSwapChainTest, FailedPresentDueToDeviceRemoved) {
  SetSwapChainExpectations();
  EXPECT_CALL(*mock_dxgi_swapchain_.Get(), Present1)
      .WillOnce(testing::Return(DXGI_ERROR_DEVICE_REMOVED));
  EXPECT_CALL(*mock_d3d11_device_.Get(), GetDeviceRemovedReason)
      .WillOnce(testing::Return(DXGI_ERROR_DEVICE_REMOVED));

  DoResize(kTestSize);
  DoBeginPaint(kTestSize);

  // There should be no GPU process termination.
  device_->EndPaintDelegated(
      gfx::Rect(0, 0, kTestSize.width(), kTestSize.height()));
}

using SoftwareOutputDeviceWinSwapChainDeathTest =
    SoftwareOutputDeviceWinSwapChainTest;

// Ensure the GPU process is terminated if ResizeBuffers fails.
TEST_F(SoftwareOutputDeviceWinSwapChainDeathTest, ResizeBuffersFailure) {
  SetSwapChainExpectations();
  ON_CALL(*mock_dxgi_swapchain_.Get(),
          ResizeBuffers(/*BufferCount=*/2, /*Width=*/1180, /*Height=*/620,
                        /*NewFormat=*/testing::_, /*SwapChainFlags=*/0))
      .WillByDefault(testing::Return(E_FAIL));

  // Do first resize and ensure it is successful. We don't expect ResizeBuffers
  // to be called here as it's the first Resize and the SwapChain will be
  // initialized in the scall.
  DoResize(kTestSize);

  // SwapChain should be initialized, resulting in a ResizeBuffers call; which
  // should fail triggering a crash.
  EXPECT_DEATH(device_->Resize(gfx::Size(1180, 620), 1.f), "");
}

// Test that ResizeDelegated results in a process crash if
// CreateSwapChainForComposition fails.
TEST_F(SoftwareOutputDeviceWinSwapChainDeathTest,
       CreateSwapSchainForCompositionFailure) {
  ON_CALL(*mock_dcomp_device_.Get(), CreateTargetForHwnd)
      .WillByDefault(
          media::SetComPointeeAndReturnOk<2>(mock_dcomp_target_.Get()));
  ON_CALL(*mock_dcomp_device_.Get(), CreateVisual)
      .WillByDefault(
          media::SetComPointeeAndReturnOk<0>(mock_dcomp_visual_.Get()));
  ON_CALL(*mock_dxgi_factory_.Get(), CreateSwapChainForComposition)
      .WillByDefault(testing::Return(E_OUTOFMEMORY));
  EXPECT_DEATH(device_->Resize(kTestSize, 1.f), "");
}

}  // namespace viz
