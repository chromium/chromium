// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_MOCK_PAINT_PREVIEW_RECORDER_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_MOCK_PAINT_PREVIEW_RECORDER_H_

#include "base/functional/callback.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace paint_preview {

class MockPaintPreviewRecorder : public mojom::PaintPreviewRecorder {
 public:
  MockPaintPreviewRecorder();

  MockPaintPreviewRecorder(const MockPaintPreviewRecorder&) = delete;
  MockPaintPreviewRecorder& operator=(const MockPaintPreviewRecorder&) = delete;

  ~MockPaintPreviewRecorder() override;

  // mojom::PaintPreviewRecorder:
  void CapturePaintPreview(
      mojom::PaintPreviewCaptureParamsPtr params,
      mojom::PaintPreviewRecorder::CapturePaintPreviewCallback callback)
      override;
  void GetGeometryMetadata(mojom::GeometryMetadataParamsPtr params,
                           GetGeometryMetadataCallback callback) override;

  // Stores the expected params to compare against in `CheckParams`.
  void SetExpectedParams(mojom::PaintPreviewCaptureParamsPtr params);
  void SetExpectedGeometryParams(mojom::GeometryMetadataParamsPtr params);

  // Sets the status or response that will be sent. If `response` is
  // `base::ok(nullptr)`, a new response will be conructed on demand when
  // needed. Each response is consumed when it is sent.
  void SetResponse(
      base::expected<mojom::PaintPreviewCaptureResponsePtr,
                     mojom::PaintPreviewStatus> response = base::ok(nullptr));
  // Sets the response that will be sent. If `response` is `nullptr`, a new
  // response will be conructed on demand when needed.
  void SetGeometryResponse(
      mojom::GeometryMetadataResponsePtr response = nullptr);

  // Sets a closure to run during `CapturePaintPreview`, instead of immediately
  // invoking the callback. This can be used with a TestFuture or RunLoop to
  // return control flow to the test.
  void SetReceivedRequestClosure(base::OnceClosure closure);

  // Runs the callback received in `CapturePaintPreview`. This may only be used
  // if `SetReceivedRequestClosure` was used prior.
  void SendResponse();
  void SendGeometryResponse();

  void BindRequest(mojo::ScopedInterfaceEndpointHandle handle);

  static mojom::PaintPreviewCaptureResponsePtr NewResponse();

 protected:
  virtual void CheckParams(
      const mojom::PaintPreviewCaptureParamsPtr& input_params);

  virtual void CheckGeometryParams(
      const mojom::GeometryMetadataParamsPtr& input_params);

  // If non-null, this is invoked when the recorder receives a paint preview
  // request.
  base::OnceClosure received_request_closure_;

  mojom::PaintPreviewRecorder::CapturePaintPreviewCallback
      send_response_callback_;
  mojom::PaintPreviewRecorder::GetGeometryMetadataCallback
      send_geometry_response_callback_;

  mojom::PaintPreviewCaptureParamsPtr expected_params_;
  mojom::GeometryMetadataParamsPtr expected_geometry_params_;

  std::optional<base::expected<mojom::PaintPreviewCaptureResponsePtr,
                               mojom::PaintPreviewStatus>>
      response_ = std::nullopt;
  mojom::GeometryMetadataResponsePtr geometry_response_;

  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder> binding_{this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_MOCK_PAINT_PREVIEW_RECORDER_H_
