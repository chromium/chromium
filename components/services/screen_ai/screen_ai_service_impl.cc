// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include "base/process/process.h"
#include "components/services/screen_ai/proto/proto_convertor.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"

namespace {

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

  if (features::IsScreenAIVisualAnnotationsEnabled()) {
    if (!screen_ai_init_function_ || !annotate_function_)
      init_result = InitializationResult::kErrorInvalidLibraryFunctions;
    else if (!screen_ai_init_function_(features::IsScreenAIDebugModeEnabled()))
      init_result = InitializationResult::kErrorInitializationFailed;
  }

  if (features::IsReadAnythingWithScreen2xEnabled()) {
    if (!screen_2x_init_function_ || !extract_main_content_function_)
      init_result = InitializationResult::kErrorInvalidLibraryFunctions;
    else if (!screen_2x_init_function_(
                 features::IsScreenAIDebugModeEnabled())) {
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
    updates = ScreenAIVisualAnnotationToAXTreeUpdate(annotation_text);
  } else {
    VLOG(1) << "Screen AI library could not process snapshot.";
  }

  std::move(callback).Run(updates);
}

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ContentExtractionCallback callback) {
  std::string serialized_snapshot = Screen2xSnapshotToViewHierarchy(snapshot);
  std::vector<int32_t> content_node_ids;

  if (!extract_main_content_function_(serialized_snapshot, content_node_ids)) {
    VLOG(1) << "Screen2x did not return main content.";
  } else {
    VLOG(2) << "Screen2x returned " << content_node_ids.size() << " node ids:";
    for (int32_t i : content_node_ids)
      VLOG(2) << i;
  }
  std::move(callback).Run(content_node_ids);
}

}  // namespace screen_ai
