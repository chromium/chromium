// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_TEST_SUPPORT_DCOMP_MOCKS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_TEST_SUPPORT_DCOMP_MOCKS_H_

#include <dcomp.h>
#include <wrl.h>

#include "media/base/win/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Example mock for IDCompositionDevice, modeled after D3D11DeviceMock.
class DCompositionDeviceMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDCompositionDevice> {
 public:
  DCompositionDeviceMock();
  ~DCompositionDeviceMock() override;

  // IUnknown
  MOCK_STDCALL_METHOD2(QueryInterface, HRESULT(REFIID, void**));
  MOCK_STDCALL_METHOD0(AddRef, ULONG());
  MOCK_STDCALL_METHOD0(Release, ULONG());

  // IDCompositionDevice
  MOCK_STDCALL_METHOD0(Commit, HRESULT());
  MOCK_STDCALL_METHOD0(WaitForCommitCompletion, HRESULT());
  MOCK_STDCALL_METHOD1(GetFrameStatistics,
                       HRESULT(DCOMPOSITION_FRAME_STATISTICS*));
  MOCK_STDCALL_METHOD3(CreateTargetForHwnd,
                       HRESULT(HWND, BOOL, IDCompositionTarget**));
  MOCK_STDCALL_METHOD1(CreateVisual, HRESULT(IDCompositionVisual**));
  MOCK_STDCALL_METHOD5(CreateSurface,
                       HRESULT(UINT,
                               UINT,
                               DXGI_FORMAT,
                               DXGI_ALPHA_MODE,
                               IDCompositionSurface**));
  MOCK_STDCALL_METHOD5(CreateVirtualSurface,
                       HRESULT(UINT,
                               UINT,
                               DXGI_FORMAT,
                               DXGI_ALPHA_MODE,
                               IDCompositionVirtualSurface**));
  MOCK_STDCALL_METHOD2(CreateSurfaceFromHandle, HRESULT(HANDLE, IUnknown**));
  MOCK_STDCALL_METHOD2(CreateSurfaceFromHwnd, HRESULT(HWND, IUnknown**));
  MOCK_STDCALL_METHOD1(CreateTranslateTransform,
                       HRESULT(IDCompositionTranslateTransform**));
  MOCK_STDCALL_METHOD1(CreateScaleTransform,
                       HRESULT(IDCompositionScaleTransform**));
  MOCK_STDCALL_METHOD1(CreateRotateTransform,
                       HRESULT(IDCompositionRotateTransform**));
  MOCK_STDCALL_METHOD1(CreateSkewTransform,
                       HRESULT(IDCompositionSkewTransform**));
  MOCK_STDCALL_METHOD1(CreateMatrixTransform,
                       HRESULT(IDCompositionMatrixTransform**));
  MOCK_STDCALL_METHOD3(CreateTransformGroup,
                       HRESULT(IDCompositionTransform**,
                               UINT,
                               IDCompositionTransform**));
  MOCK_STDCALL_METHOD1(CreateTranslateTransform3D,
                       HRESULT(IDCompositionTranslateTransform3D**));
  MOCK_STDCALL_METHOD1(CreateScaleTransform3D,
                       HRESULT(IDCompositionScaleTransform3D**));
  MOCK_STDCALL_METHOD1(CreateRotateTransform3D,
                       HRESULT(IDCompositionRotateTransform3D**));
  MOCK_STDCALL_METHOD1(CreateMatrixTransform3D,
                       HRESULT(IDCompositionMatrixTransform3D**));
  MOCK_STDCALL_METHOD3(CreateTransform3DGroup,
                       HRESULT(IDCompositionTransform3D**,
                               UINT,
                               IDCompositionTransform3D**));
  MOCK_STDCALL_METHOD1(CreateEffectGroup, HRESULT(IDCompositionEffectGroup**));
  MOCK_STDCALL_METHOD1(CreateRectangleClip,
                       HRESULT(IDCompositionRectangleClip**));
  MOCK_STDCALL_METHOD1(CreateAnimation, HRESULT(IDCompositionAnimation**));
  MOCK_STDCALL_METHOD1(CheckDeviceState, HRESULT(BOOL*));
};

class DCompositionTargetMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDCompositionTarget> {
 public:
  DCompositionTargetMock();
  ~DCompositionTargetMock() override;

  // IUnknown
  MOCK_STDCALL_METHOD2(QueryInterface, HRESULT(REFIID, void**));
  MOCK_STDCALL_METHOD0(AddRef, ULONG());
  MOCK_STDCALL_METHOD0(Release, ULONG());

  // IDCompositionTarget
  MOCK_STDCALL_METHOD1(SetRoot, HRESULT(IDCompositionVisual*));
};

class DCompositionVisualMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDCompositionVisual> {
 public:
  DCompositionVisualMock();
  ~DCompositionVisualMock() override;

  // IUnknown
  MOCK_STDCALL_METHOD2(QueryInterface, HRESULT(REFIID riid, void** ppvObject));
  MOCK_STDCALL_METHOD0(AddRef, ULONG());
  MOCK_STDCALL_METHOD0(Release, ULONG());

  // IDCompositionVisual (with all overloaded methods left as-is)
  MOCK_STDCALL_METHOD1(SetOffsetX, HRESULT(float offsetX));
  MOCK_STDCALL_METHOD1(SetOffsetX, HRESULT(IDCompositionAnimation* animation));

  MOCK_STDCALL_METHOD1(SetOffsetY, HRESULT(float offsetY));
  MOCK_STDCALL_METHOD1(SetOffsetY, HRESULT(IDCompositionAnimation* animation));

  MOCK_STDCALL_METHOD1(SetTransform, HRESULT(const D2D_MATRIX_3X2_F& matrix));
  MOCK_STDCALL_METHOD1(SetTransform,
                       HRESULT(IDCompositionTransform* transform));

  MOCK_STDCALL_METHOD1(SetTransformParent,
                       HRESULT(IDCompositionVisual* visual));
  MOCK_STDCALL_METHOD1(SetEffect, HRESULT(IDCompositionEffect* effect));

  MOCK_STDCALL_METHOD1(SetBitmapInterpolationMode,
                       HRESULT(DCOMPOSITION_BITMAP_INTERPOLATION_MODE mode));
  MOCK_STDCALL_METHOD1(SetBorderMode, HRESULT(DCOMPOSITION_BORDER_MODE mode));

  // The interface calls these "SetClip", so we keep them as such:
  MOCK_STDCALL_METHOD1(SetClip, HRESULT(const D2D_RECT_F& rect));
  MOCK_STDCALL_METHOD1(SetClip, HRESULT(IDCompositionClip* clip));

  MOCK_STDCALL_METHOD1(SetContent, HRESULT(IUnknown* content));

  MOCK_STDCALL_METHOD3(AddVisual,
                       HRESULT(IDCompositionVisual* visual,
                               BOOL insertAbove,
                               IDCompositionVisual* referenceVisual));
  MOCK_STDCALL_METHOD1(RemoveVisual, HRESULT(IDCompositionVisual* visual));
  MOCK_STDCALL_METHOD0(RemoveAllVisuals, HRESULT());

  MOCK_STDCALL_METHOD1(SetCompositeMode,
                       HRESULT(DCOMPOSITION_COMPOSITE_MODE mode));
};

}  // namespace media

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_TEST_SUPPORT_DCOMP_MOCKS_H_
