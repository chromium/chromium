// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/mock_paint_preview_recorder.h"

#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

MockPaintPreviewRecorder::MockPaintPreviewRecorder() = default;

MockPaintPreviewRecorder::~MockPaintPreviewRecorder() = default;

void MockPaintPreviewRecorder::CapturePaintPreview(
    mojom::PaintPreviewCaptureParamsPtr params,
    mojom::PaintPreviewRecorder::CapturePaintPreviewCallback callback) {
  CheckParams(params);

  if (received_request_closure_) {
    send_response_callback_ = std::move(callback);
    std::move(received_request_closure_).Run();
  } else {
    if (!response_) {
      response_ = NewResponse();
    }
    std::move(callback).Run(status_, std::move(response_));
  }
}

void MockPaintPreviewRecorder::SetExpectedParams(
    mojom::PaintPreviewCaptureParamsPtr params) {
  expected_params_ = std::move(params);
}

void MockPaintPreviewRecorder::SetResponse(
    mojom::PaintPreviewStatus status,
    mojom::PaintPreviewCaptureResponsePtr response) {
  status_ = status;
  response_ = std::move(response);
}

void MockPaintPreviewRecorder::SetReceivedRequestClosure(
    base::OnceClosure closure) {
  ASSERT_FALSE(received_request_closure_);
  received_request_closure_ = std::move(closure);
}

void MockPaintPreviewRecorder::SendResponse() {
  ASSERT_TRUE(send_response_callback_);
  if (!response_) {
    response_ = NewResponse();
  }
  std::move(send_response_callback_).Run(status_, std::move(response_));
}

void MockPaintPreviewRecorder::BindRequest(
    mojo::ScopedInterfaceEndpointHandle handle) {
  binding_.reset();
  binding_.Bind(mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder>(
      std::move(handle)));
}

// static
mojom::PaintPreviewCaptureResponsePtr MockPaintPreviewRecorder::NewResponse() {
  auto response = mojom::PaintPreviewCaptureResponse::New();
  response->geometry_metadata = mojom::GeometryMetadataResponse::New();
  return response;
}

void MockPaintPreviewRecorder::CheckParams(
    const mojom::PaintPreviewCaptureParamsPtr& input_params) {
  ASSERT_TRUE(expected_params_) << "SetExpectedParams() not called";
  EXPECT_EQ(input_params->guid, expected_params_->guid);
  EXPECT_EQ(input_params->geometry_metadata_params->clip_rect,
            expected_params_->geometry_metadata_params->clip_rect);
  EXPECT_EQ(input_params->is_main_frame, expected_params_->is_main_frame);
  if (expected_params_->is_main_frame) {
    EXPECT_FALSE(input_params->geometry_metadata_params->clip_rect_is_hint);
  }
  if (input_params->is_main_frame) {
    EXPECT_EQ(
        input_params->geometry_metadata_params->clip_x_coord_override,
        expected_params_->geometry_metadata_params->clip_x_coord_override);
    EXPECT_EQ(
        input_params->geometry_metadata_params->clip_y_coord_override,
        expected_params_->geometry_metadata_params->clip_y_coord_override);
  } else {
    EXPECT_EQ(input_params->geometry_metadata_params->clip_x_coord_override,
              mojom::ClipCoordOverride::kNone);
    EXPECT_EQ(input_params->geometry_metadata_params->clip_y_coord_override,
              mojom::ClipCoordOverride::kNone);
  }
}

}  // namespace paint_preview
