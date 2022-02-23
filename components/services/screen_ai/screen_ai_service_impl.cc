// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_service_impl.h"

namespace screen_ai {

// static
base::FilePath ScreenAIService::GetLibraryPath() {
  return base::FilePath(FILE_PATH_LITERAL("/"))
      .Append(FILE_PATH_LITERAL("lib"))
      .Append(FILE_PATH_LITERAL("libchrome_screen_ai.so"));
}

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIService> receiver)
    : library_(GetLibraryPath()),
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
  std::vector<mojom::NodePtr> nodes;
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
      VLOG(2) << "Screen AI library returned: " << annotation_text;
      DecodeProto(annotation_text, nodes);
    } else {
      VLOG(1) << "Screen AI library could not process snapshot.";
      error = mojom::ErrorType::kFailedProcessingImage;
    }
  } else {
    error = mojom::ErrorType::kFailedLibraryNotFound;
  }

  std::move(callback).Run(error, std::move(nodes));
}

// TODO(https://crbug.com/1278249): Implement!
bool ScreenAIService::DecodeProto(const std::string& proto_text,
                                  std::vector<mojom::NodePtr>& annotations) {
  return false;
}

}  // namespace screen_ai
