// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"

namespace {

// The minimum confidence level that a Screen AI annotation should have to be
// accepted.
// TODO(https://crbug.com/1278249): Add experiment or heuristics to better
// adjust this threshold.
const float kScreenAIMinConfidenceThreshold = 0.1;

}  // namespace

namespace screen_ai {

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIService> receiver)
    : library_(screen_ai::GetLibraryFilePath()),
      init_function_(reinterpret_cast<ScreenAIInitFunction>(
          library_.GetFunctionPointer("Init"))),
      annotator_function_(reinterpret_cast<ScreenAIAnnotateFunction>(
          library_.GetFunctionPointer("Annotate"))),
      receiver_(this, std::move(receiver)) {
  if (!init_function_ || !init_function_()) {
    VLOG(1) << "Screen AI library initialization failed.";
    annotator_function_ = nullptr;
  }

  // TODO(https://crbug.com/1278249): Try to refrain from creating the service
  // if library functions are not available.
}

ScreenAIService::~ScreenAIService() = default;

void ScreenAIService::BindAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) {
  screen_ai_annotators_.Add(this, std::move(annotator));
}

void ScreenAIService::Annotate(const SkBitmap& image,
                               AnnotationCallback callback) {
  ui::AXTreeUpdate updates;

  if (annotator_function_ == nullptr) {
    VLOG(1) << "Screen AI library binary was not found.";
  } else {
    VLOG(2) << "Screen AI library starting to process " << image.width() << "x"
            << image.height() << " snapshot.";

    std::string annotation_text;
    // TODO(https://crbug.com/1278249): Consider adding a signature that
    // verifies the data integrity and source.
    // TODO(https://crbug.com/1278249): Consider replacing the input with a data
    // item that includes data size.
    if (annotator_function_(
            static_cast<const unsigned char*>(image.getPixels()), image.width(),
            image.height(), annotation_text)) {
      updates = DecodeProto(annotation_text);
    } else {
      VLOG(1) << "Screen AI library could not process snapshot.";
    }
  }

  std::move(callback).Run(updates);
}

ui::AXTreeUpdate ScreenAIService::DecodeProto(
    const std::string& serialized_proto) {
  ui::AXTreeUpdate updates;

  // TODO(https://crbug.com/1278249): Consider adding version checking.
  chrome_screen_ai::VisualAnnotation results;
  if (!results.ParseFromString(serialized_proto)) {
    VLOG(1) << "Could not parse Screen AI library output.";
    return updates;
  }

  // TODO(https://crbug.com/1278249): Create an AXTreeSource and create the
  // update using AXTreeSerializer.

  for (const auto& uic : results.ui_component()) {
    // Score is only used to prune very low confidence detections and we don't
    // use it downstream.
    if (uic.predicted_type().score() < kScreenAIMinConfidenceThreshold)
      continue;

    ui::AXNodeData node;

    chrome_screen_ai::UIComponent::Type original_type =
        uic.predicted_type().type();
    node.relative_bounds.bounds.set_x(uic.bounding_box().x());
    node.relative_bounds.bounds.set_y(uic.bounding_box().y());
    node.relative_bounds.bounds.set_width(uic.bounding_box().width());
    node.relative_bounds.bounds.set_height(uic.bounding_box().height());

    // TODO(https://crbug.com/1278249): Add tests to ensure these two types
    // match. Add a PRESUBMIT test that compares the proto and enum.
    node.role = static_cast<ax::mojom::Role>(original_type);

    updates.nodes.push_back(node);
  }

  // TODO(https://crbug.com/1278249): Add UMA metrics to record the number of
  // annotations, item types, confidence levels, etc.

  return updates;
}

}  // namespace screen_ai
