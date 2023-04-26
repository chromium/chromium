// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/screen_ai_library_wrapper.h"

#include "base/metrics/histogram_functions.h"
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

}  // namespace

ScreenAILibraryWrapper::ScreenAILibraryWrapper() = default;

template <typename T>
bool ScreenAILibraryWrapper::LoadFunction(T& function_variable,
                                          const char* function_name) {
  function_variable =
      reinterpret_cast<T>(library_.GetFunctionPointer(function_name));
  if (function_variable == nullptr) {
    VLOG(0) << "Could not load function: " << function_name;
    return false;
  }
  return true;
}

bool ScreenAILibraryWrapper::Init(const base::FilePath& library_path) {
  library_ = base::ScopedNativeLibrary(library_path);

#if BUILDFLAG(IS_WIN)
  DWORD error = library_.GetError()->code;
  base::UmaHistogramSparse(
      "Accessibility.ScreenAI.LibraryLoadDetailedResultOnWindows",
      static_cast<int>(error));
  if (error != ERROR_SUCCESS) {
    VLOG(0) << "Library load error: " << library_.GetError()->code;
    return false;
  }
#else

  if (!library_.GetError()->message.empty()) {
    VLOG(0) << "Library load error: " << library_.GetError()->message;
    return false;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!LoadFunction(set_logger_, "SetLogger")) {
    return false;
  }
#endif

  // General functions.
  if (!LoadFunction(get_library_version_, "GetLibraryVersion") ||
      !LoadFunction(get_library_version_, "GetLibraryVersion") ||
      !LoadFunction(enable_debug_mode_, "EnableDebugMode") ||
      !LoadFunction(free_library_allocated_int32_array_,
                    "FreeLibraryAllocatedInt32Array") ||
      !LoadFunction(free_library_allocated_char_array_,
                    "FreeLibraryAllocatedCharArray")) {
    return false;
  }

  // Layout Extraction functions.
  if (features::IsLayoutExtractionEnabled()) {
    if (!LoadFunction(init_layout_extraction_, "InitLayoutExtraction") ||
        !LoadFunction(extract_layout_, "ExtractLayout")) {
      return false;
    }
  }

  // OCR functions.
  if (features::IsPdfOcrEnabled()) {
    if (!LoadFunction(init_ocr_, "InitOCR") ||
        !LoadFunction(perform_ocr_, "PerformOCR")) {
      return false;
    }
  }

  // Main Content Extraction functions.
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    if (!LoadFunction(init_main_content_extraction_,
                      "InitMainContentExtraction") ||
        !LoadFunction(extract_main_content_, "ExtractMainContent")) {
      return false;
    }
  }

  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapper::ScreenAILibraryWrapper::SetLogger() {
  CHECK(set_logger_);
  set_logger_(&HandleLibraryLogging);
}
#endif

NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapper::GetLibraryVersion(uint32_t& major,
                                               uint32_t& minor) {
  CHECK(get_library_version_);
  get_library_version_(major, minor);
}

NO_SANITIZE("cfi-icall")
void ScreenAILibraryWrapper::EnableDebugMode() {
  CHECK(enable_debug_mode_);
  enable_debug_mode_();
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::InitLayoutExtraction() {
  CHECK(init_layout_extraction_);
  return init_layout_extraction_();
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::InitOCR(const base::FilePath& models_folder) {
  CHECK(init_ocr_);
  return init_ocr_(models_folder.MaybeAsASCII().c_str());
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::InitMainContentExtraction(
    base::File& model_config_file,
    base::File& model_tflite_file) {
  CHECK(init_main_content_extraction_);

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
bool ScreenAILibraryWrapper::PerformOcr(
    const SkBitmap& image,
    chrome_screen_ai::VisualAnnotation& annotation_proto) {
  CHECK(perform_ocr_);
  CHECK(free_library_allocated_char_array_);

  uint32_t annotation_proto_length = 0;
  std::unique_ptr<char, decltype(free_library_allocated_char_array_)>
      library_buffer(perform_ocr_(image, annotation_proto_length),
                     free_library_allocated_char_array_);

  bool result = library_buffer
                    ? annotation_proto.ParseFromArray(library_buffer.get(),
                                                      annotation_proto_length)
                    : false;

  // TODO(crbug.com/1278245): Remove this after fixing the crash issue on Linux
  // official.
#if BUILDFLAG(IS_LINUX)
  free_library_allocated_char_array_(library_buffer.release());
#endif

  return result;
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::ExtractLayout(
    const SkBitmap& image,
    chrome_screen_ai::VisualAnnotation& annotation_proto) {
  CHECK(extract_layout_);
  CHECK(free_library_allocated_char_array_);

  uint32_t annotation_proto_length = 0;
  std::unique_ptr<char, decltype(free_library_allocated_char_array_)>
      library_buffer(extract_layout_(image, annotation_proto_length),
                     free_library_allocated_char_array_);

  bool result = library_buffer
                    ? annotation_proto.ParseFromArray(library_buffer.get(),
                                                      annotation_proto_length)
                    : false;

  // TODO(crbug.com/1278245): Remove this after fixing the crash issue on Linux
  // official.
#if BUILDFLAG(IS_LINUX)
  free_library_allocated_char_array_(library_buffer.release());
#endif

  return result;
}

NO_SANITIZE("cfi-icall")
bool ScreenAILibraryWrapper::ExtractMainContent(
    const std::string& serialized_view_hierarchy,
    std::vector<int32_t>& node_ids) {
  CHECK(extract_main_content_);
  CHECK(free_library_allocated_int32_array_);

  uint32_t nodes_count = 0;
  std::unique_ptr<int32_t, decltype(free_library_allocated_int32_array_)>
      library_buffer(extract_main_content_(serialized_view_hierarchy.data(),
                                           serialized_view_hierarchy.length(),
                                           nodes_count),
                     free_library_allocated_int32_array_);

  if (!library_buffer) {
    return false;
  }

  node_ids.resize(nodes_count);
  if (nodes_count != 0) {
    memcpy(node_ids.data(), library_buffer.get(),
           nodes_count * sizeof(int32_t));
  }

  // TODO(crbug.com/1278245): Remove this after fixing the crash issue on Linux
  // official.
#if BUILDFLAG(IS_LINUX)
  free_library_allocated_int32_array_(library_buffer.release());
#endif

  return true;
}

}  // namespace screen_ai
