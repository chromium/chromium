// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include "base/process/process.h"
#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/accessibility_features.h"

namespace {

// The minimum confidence level that a Screen AI annotation should have to be
// accepted.
// TODO(https://crbug.com/1278249): Add experiment or heuristics to better
// adjust this threshold.
const float kScreenAIMinConfidenceThreshold = 0.1;

enum class InitializationResult {
  kOk = 0,
  kErrorInvalidLibraryFunctions = 1,
  kErrorInitializationFailed = 2,
};

}  // namespace

namespace screen_ai {

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIService> receiver)
    : library_(GetPreloadedLibraryFilePath()),
      screen_ai_init_function_(reinterpret_cast<ScreenAIInitFunction>(
          library_.GetFunctionPointer("InitScreenAI"))),
      annotate_function_(reinterpret_cast<AnnotateFunction>(
          library_.GetFunctionPointer("Annotate"))),
      screen_2x_init_function_(reinterpret_cast<Screen2xInitFunction>(
          library_.GetFunctionPointer("InitScreen2x"))),
      extract_main_content_function_(
          reinterpret_cast<ExtractMainContentFunction>(
              library_.GetFunctionPointer("ExtractMainContent"))),
      receiver_(this, std::move(receiver)) {
  auto init_result = InitializationResult::kOk;

  if (features::IsScreenAIEnabled()) {
    if (!screen_ai_init_function_ || !annotate_function_)
      init_result = InitializationResult::kErrorInvalidLibraryFunctions;
    else if (!screen_ai_init_function_())
      init_result = InitializationResult::kErrorInitializationFailed;
  }

  if (features::IsReadAnythingWithScreen2xEnabled()) {
    if (!screen_2x_init_function_ || !extract_main_content_function_)
      init_result = InitializationResult::kErrorInvalidLibraryFunctions;
    else if (!screen_2x_init_function_()) {
      init_result = InitializationResult::kErrorInitializationFailed;
    }
  }

  if (init_result != InitializationResult::kOk) {
    // TODO(https://crbug.com/1278249): Add UMA metrics to monitor failures.
    VLOG(1) << "Screen AI library initialization failed: "
            << static_cast<int>(init_result);
    base::Process::TerminateCurrentProcessImmediately(
        static_cast<int>(init_result));
  }
}

ScreenAIService::~ScreenAIService() = default;

void ScreenAIService::BindAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) {
  screen_ai_annotators_.Add(this, std::move(annotator));
}

void ScreenAIService::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>
        main_content_extractor) {
  screen_2x_main_content_extractors_.Add(this,
                                         std::move(main_content_extractor));
}

void ScreenAIService::Annotate(const SkBitmap& image,
                               AnnotationCallback callback) {
  ui::AXTreeUpdate updates;

  VLOG(2) << "Screen AI library starting to process " << image.width() << "x"
          << image.height() << " snapshot.";

  std::string annotation_text;
  // TODO(https://crbug.com/1278249): Consider adding a signature that
  // verifies the data integrity and source.
  if (annotate_function_(image, annotation_text)) {
    updates = DecodeAnnotatorProto(annotation_text);
  } else {
    VLOG(1) << "Screen AI library could not process snapshot.";
  }

  std::move(callback).Run(updates);
}

ui::AXTreeUpdate ScreenAIService::DecodeAnnotatorProto(
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

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ContentExtractionCallback callback) {
  // TODO(https://crbug.com/1278249): Call |extract_main_content_function_|,
  // pass |snapshot| to it, receive results, and send them to |callback|.
  std::move(callback).Run(std::vector<int32_t>());
}

}  // namespace screen_ai
