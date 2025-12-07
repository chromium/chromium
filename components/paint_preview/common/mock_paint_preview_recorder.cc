// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/mock_paint_preview_recorder.h"

#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

MockPaintPreviewRecorder::MockPaintPreviewRecorder() = default;

MockPaintPreviewRecorder::~MockPaintPreviewRecorder() {
  // All responses should have been sent.
  CHECK(!response_);
  CHECK(!geometry_response_);
}

void MockPaintPreviewRecorder::CapturePaintPreview(
    mojom::PaintPreviewCaptureParamsPtr params,
    mojom::PaintPreviewRecorder::CapturePaintPreviewCallback callback) {
  CheckParams(params);

  send_response_callback_ = std::move(callback);
  if (received_request_closure_) {
    std::move(received_request_closure_).Run();
  } else {
    SendResponse();
  }
}

void MockPaintPreviewRecorder::GetGeometryMetadata(
    mojom::GeometryMetadataParamsPtr params,
    mojom::PaintPreviewRecorder::GetGeometryMetadataCallback callback) {
  CheckGeometryParams(params);

  send_geometry_response_callback_ = std::move(callback);
  if (received_request_closure_) {
    std::move(received_request_closure_).Run();
  } else {
    SendGeometryResponse();
  }
}

void MockPaintPreviewRecorder::SetExpectedParams(
    mojom::PaintPreviewCaptureParamsPtr params) {
  expected_params_ = std::move(params);
}

void MockPaintPreviewRecorder::SetExpectedGeometryParams(
    mojom::GeometryMetadataParamsPtr params) {
  expected_geometry_params_ = std::move(params);
}

void MockPaintPreviewRecorder::SetResponse(
    base::expected<mojom::PaintPreviewCaptureResponsePtr,
                   mojom::PaintPreviewStatus> response) {
  response_ = std::move(response);
}

void MockPaintPreviewRecorder::SetGeometryResponse(
    mojom::GeometryMetadataResponsePtr response) {
  geometry_response_ = std::move(response);
}

void MockPaintPreviewRecorder::SetReceivedRequestClosure(
    base::OnceClosure closure) {
  ASSERT_FALSE(received_request_closure_);
  received_request_closure_ = std::move(closure);
}

void MockPaintPreviewRecorder::SendResponse() {
  ASSERT_TRUE(send_response_callback_);
  ASSERT_TRUE(response_) << "SetResponse() not called";
  if (response_.value().has_value() && response_.value().value().is_null()) {
    response_ = NewResponse();
  }
  std::optional<base::expected<mojom::PaintPreviewCaptureResponsePtr,
                               mojom::PaintPreviewStatus>>
      resp = std::nullopt;
  resp.swap(response_);
  std::move(send_response_callback_).Run(std::move(resp).value());
}

void MockPaintPreviewRecorder::SendGeometryResponse() {
  ASSERT_TRUE(send_geometry_response_callback_);
  if (!geometry_response_) {
    geometry_response_ = mojom::GeometryMetadataResponse::New();
  }
  std::move(send_geometry_response_callback_)
      .Run(std::move(geometry_response_));
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

void MockPaintPreviewRecorder::CheckGeometryParams(
    const mojom::GeometryMetadataParamsPtr& input_params) {
  ASSERT_TRUE(expected_geometry_params_) << "SetExpectedGeoParams() not called";
  EXPECT_EQ(expected_geometry_params_->clip_rect, input_params->clip_rect);
}

}  // namespace paint_preview
