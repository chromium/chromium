// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_library_wrapper.h"

#include "ui/accessibility/accessibility_features.h"

namespace screen_ai {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
void HandleLibraryLogging(int severity, const char* message) {
  switch (severity) {
    case logging::LOG_VERBOSE:
    case logging::LOG_INFO:
      VLOG(2) << message;
      break;
    case logging::LOG_WARNING:
      VLOG(1) << message;
      break;
    case logging::LOG_ERROR:
    case logging::LOG_FATAL:
      VLOG(0) << message;
      break;
  }
}
#endif

std::vector<char> LoadModelFile(base::File& model_file) {
  std::vector<char> buffer;
  int64_t length = model_file.GetLength();
  if (length < 0) {
    VLOG(0) << "Could not query Screen AI model file's length.";
    return buffer;
  }

  buffer.resize(length);
  if (model_file.Read(0, buffer.data(), length) != length) {
    buffer.clear();
    VLOG(0) << "Could not read Screen AI model file's content.";
  }

  return buffer;
}

// TODO(crbug.com/1413983): Remove this function when the bug is fixed.
// This function is added only to get a better stack trace in the crash dump.
void CheckLibraryIsLoaded(base::ScopedNativeLibrary& library) {
  CHECK_NE(library.GetError(), nullptr);
#if BUILDFLAG(IS_WIN)
  CHECK_EQ(library.GetError()->code, 0u);
#else
  CHECK(library.GetError()->message.empty());
#endif
}

}  // namespace

ScreenAILibraryWrapper::ScreenAILibraryWrapper() = default;

bool ScreenAILibraryWrapper::Init(const base::FilePath& library_path) {
  library_ = base::ScopedNativeLibrary(library_path);
  CheckLibraryIsLoaded(library_);

  if (library_.GetError() == nullptr) {
    VLOG(0) << "Library load state cannot be read.";
    return false;
  }
#if BUILDFLAG(IS_WIN)
  if (library_.GetError()->code != 0u) {
    VLOG(0) << "Library load error: " << library_.GetError()->code;
    return false;
  }
#else
  if (!library_.GetError()->message.empty()) {
    VLOG(0) << "Library load error: " << library_.GetError()->message;
    return false;
  }
#endif

// Loads a library function with name N, type T, and puts it in variable V.
// If the function is not loaded, returns false.
#define LOAD_FUNCTION(V, T, N)                             \
  V = reinterpret_cast<T>(library_.GetFunctionPointer(N)); \
  if (V == nullptr) {                                      \
    VLOG(0) << "Could not load function: " << N;           \
    return false;                                          \
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  set_logger_ =
      reinterpret_cast<SetLoggerFn>(library_.GetFunctionPointer("SetLogger"));
  DCHECK(set_logger_);
#endif

  // General functions.
  LOAD_FUNCTION(get_library_version_, GetLibraryVersionFn, "GetLibraryVersion");
  LOAD_FUNCTION(enable_debug_mode_, EnableDebugModeFn, "EnableDebugMode");
  LOAD_FUNCTION(read_buffered_int32_array_, ReadBufferedInt32ArrayFn,
                "ReadBufferedInt32Array");
  LOAD_FUNCTION(read_buffered_char_array_, ReadBufferedCharArrayFn,
                "ReadBufferedCharArray");

  // Layout Extraction functions.
  if (features::IsLayoutExtractionEnabled()) {
    LOAD_FUNCTION(init_layout_extraction_, InitLayoutExtractionFn,
                  "InitLayoutExtraction");
    LOAD_FUNCTION(extract_layout_, ExtractLayoutFn, "ExtractLayout");
  }

  // OCR functions.
  if (features::IsPdfOcrEnabled()) {
    LOAD_FUNCTION(init_ocr_, InitOCRFn, "InitOCR");
    LOAD_FUNCTION(perform_ocr_, PerformOcrFn, "PerformOCR");
  }

  // Main Content Extraction functions.
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    LOAD_FUNCTION(init_main_content_extraction_, InitMainContentExtractionFn,
                  "InitMainContentExtraction");
    LOAD_FUNCTION(extract_main_content_, ExtractMainContentFn,
                  "ExtractMainContent");
  }

  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapper::ScreenAILibraryWrapper::SetLogger() {
  DCHECK(set_logger_);
  set_logger_(&HandleLibraryLogging);
}
#endif

NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapper::GetLibraryVersion(uint32_t& major,
                                               uint32_t& minor) {
  DCHECK(get_library_version_);
  get_library_version_(major, minor);
}

NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapper::EnableDebugMode() {
  DCHECK(enable_debug_mode_);
  enable_debug_mode_();
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::InitLayoutExtraction() {
  DCHECK(init_layout_extraction_);
  return init_layout_extraction_();
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::InitOCR(const base::FilePath& models_folder) {
  DCHECK(init_ocr_);
  return init_ocr_(models_folder.MaybeAsASCII().c_str());
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::InitMainContentExtraction(
    base::File& model_config_file,
    base::File& model_tflite_file) {
  DCHECK(init_main_content_extraction_);

  std::vector<char> model_config = LoadModelFile(model_config_file);
  std::vector<char> model_tflite = LoadModelFile(model_tflite_file);
  if (model_config.empty() || model_tflite.empty()) {
    return false;
  }

  return init_main_content_extraction_(model_config.data(), model_config.size(),
                                       model_tflite.data(),
                                       model_tflite.size());
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::PerformOcr(const SkBitmap& image,
                                        std::string& annotation_proto) {
  DCHECK(perform_ocr_);
  DCHECK(read_buffered_char_array_);

  uint32_t annotation_proto_length;
  if (!perform_ocr_(image, annotation_proto_length)) {
    return false;
  }

  annotation_proto.resize(annotation_proto_length);
  return read_buffered_char_array_(annotation_proto.data(),
                                   annotation_proto_length);
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::ExtractLayout(const SkBitmap& image,
                                           std::string& annotation_proto) {
  DCHECK(extract_layout_);
  DCHECK(read_buffered_char_array_);
  uint32_t annotation_proto_length;
  if (!extract_layout_(image, annotation_proto_length)) {
    return false;
  }

  annotation_proto.resize(annotation_proto_length);
  return read_buffered_char_array_(annotation_proto.data(),
                                   annotation_proto_length);
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::ExtractMainContent(
    const std::string& serialized_view_hierarchy,
    std::vector<int32_t>& node_ids) {
  DCHECK(extract_main_content_);
  DCHECK(read_buffered_int32_array_);

  uint32_t nodes_count;
  if (!extract_main_content_(serialized_view_hierarchy.data(),
                             serialized_view_hierarchy.length(), nodes_count)) {
    return false;
  }
  node_ids.resize(nodes_count);
  if (nodes_count == 0) {
    return true;
  }

  return read_buffered_int32_array_(node_ids.data(), nodes_count);
}

}  // namespace screen_ai
