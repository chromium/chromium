// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

#include "base/process/process.h"
#include "components/services/screen_ai/proto/proto_convertor.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/gfx/geometry/rect_f.h"

namespace screen_ai {

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIService> receiver)
    : library_(GetPreloadedLibraryFilePath()),
      init_function_(
          reinterpret_cast<InitFunction>(library_.GetFunctionPointer("Init"))),
      annotate_function_(reinterpret_cast<AnnotateFunction>(
          library_.GetFunctionPointer("Annotate"))),
      extract_main_content_function_(
          reinterpret_cast<ExtractMainContentFunction>(
              library_.GetFunctionPointer("ExtractMainContent"))),
      receiver_(this, std::move(receiver)) {
  DCHECK(init_function_ && annotate_function_ &&
         extract_main_content_function_);
  if (!init_function_(features::IsScreenAIVisualAnnotationsEnabled(),
                      features::IsReadAnythingWithScreen2xEnabled(),
                      features::IsScreenAIDebugModeEnabled(),
                      GetPreloadedLibraryFilePath().DirName().MaybeAsASCII())) {
    // TODO(https://crbug.com/1278249): Add UMA metrics to monitor failures.
    VLOG(0) << "Screen AI library initialization failed.";
    base::Process::TerminateCurrentProcessImmediately(-1);
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
  ui::AXTreeUpdate update;

  VLOG(2) << "Screen AI library starting to process " << image.width() << "x"
          << image.height() << " snapshot.";

  std::string annotation_text;
  // TODO(https://crbug.com/1278249): Consider adding a signature that
  // verifies the data integrity and source.
  if (annotate_function_(image, annotation_text)) {
    gfx::Rect image_rect(image.width(), image.height());
    update =
        ScreenAIVisualAnnotationToAXTreeUpdate(annotation_text, image_rect);
  } else {
    VLOG(1) << "Screen AI library could not process snapshot.";
  }

  std::move(callback).Run(update);
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
