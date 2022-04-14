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
}

ScreenAIService::~ScreenAIService() = default;

void ScreenAIService::BindAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) {
  screen_ai_annotators_.Add(this, std::move(annotator));
}

void ScreenAIService::Annotate(const SkBitmap& image,
                               AnnotationCallback callback) {
  std::vector<mojom::Node> annotations;
  mojom::ErrorType error = mojom::ErrorType::kOK;

  if (annotator_function_) {
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
      annotations = DecodeProto(annotation_text);
    } else {
      VLOG(1) << "Screen AI library could not process snapshot.";
      error = mojom::ErrorType::kFailedProcessingImage;
    }
  } else {
    error = mojom::ErrorType::kFailedLibraryNotFound;
  }

  // TODO(https://crbug.com/1278249): Convert |annotations| array to an
  // AxTreeSource and return it.
  VLOG(2) << "Screen AI library has " << annotations.size() << " annotations.";

  std::move(callback).Run(error, std::vector<mojom::NodePtr>());
}

std::vector<mojom::Node> ScreenAIService::DecodeProto(
    const std::string& serialized_proto) {
  // TODO(https://crbug.com/1278249): Consider using AxNodeData here.
  std::vector<mojom::Node> annotations;

  // TODO(https://crbug.com/1278249): Consider adding version checking.
  chrome_screen_ai::VisualAnnotation results;
  if (!results.ParseFromString(serialized_proto)) {
    VLOG(1) << "Could not parse Screen AI library output.";
    return annotations;
  }

  for (const auto& uic : results.ui_component()) {
    float score = uic.predicted_type().score();
    if (score < kScreenAIMinConfidenceThreshold)
      continue;

    chrome_screen_ai::UIComponent::Type original_type =
        uic.predicted_type().type();
    ::gfx::Rect rect(uic.bounding_box().x(), uic.bounding_box().y(),
                     uic.bounding_box().width(), uic.bounding_box().height());

    // TODO(https://crbug.com/1278249): Add tests to ensure these two types
    // match.
    ax::mojom::Role role = static_cast<ax::mojom::Role>(original_type);

    annotations.emplace_back(screen_ai::mojom::Node(rect, role, score));
  }

  // TODO(https://crbug.com/1278249): Add UMA metrics to record the number of
  // annotations, item types, confidence levels, etc.

  return annotations;
}

}  // namespace screen_ai
