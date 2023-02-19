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

}  // namespace

ScreenAILibraryWrapper::ScreenAILibraryWrapper(
    const base::FilePath& library_path) {
  library_ = base::ScopedNativeLibrary(library_path);
  DCHECK(library_.GetError());
#if BUILDFLAG(IS_WIN)
  DCHECK_EQ(0u, library_.GetError()->code)
      << "Library load error: " << library_.GetError()->code;
#else
  DCHECK(library_.GetError()->message.empty())
      << "Library load error: " << library_.GetError()->message;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  set_logger_ =
      reinterpret_cast<SetLoggerFn>(library_.GetFunctionPointer("SetLogger"));
  DCHECK(set_logger_);
#endif

  // General functions.
  get_library_version_ = reinterpret_cast<GetLibraryVersionFn>(
      library_.GetFunctionPointer("GetLibraryVersion"));
  DCHECK(get_library_version_);
  enable_debug_mode_ = reinterpret_cast<EnableDebugModeFn>(
      library_.GetFunctionPointer("EnableDebugMode"));
  DCHECK(enable_debug_mode_);
  read_buffered_int32_array_ = reinterpret_cast<ReadBufferedInt32ArrayFn>(
      library_.GetFunctionPointer("ReadBufferedInt32Array"));
  DCHECK(read_buffered_int32_array_);
  read_buffered_char_array_ = reinterpret_cast<ReadBufferedCharArrayFn>(
      library_.GetFunctionPointer("ReadBufferedCharArray"));
  DCHECK(read_buffered_char_array_);

  // Layout Extraction.
  if (features::IsLayoutExtractionEnabled()) {
    init_layout_extraction_ = reinterpret_cast<InitLayoutExtractionFn>(
        library_.GetFunctionPointer("InitLayoutExtraction"));
    DCHECK(init_layout_extraction_);
    extract_layout_ = reinterpret_cast<ExtractLayoutFn>(
        library_.GetFunctionPointer("ExtractLayout"));
    DCHECK(extract_layout_);
  }

  // OCR.
  if (features::IsPdfOcrEnabled()) {
    init_ocr_ =
        reinterpret_cast<InitOCRFn>(library_.GetFunctionPointer("InitOCR"));
    DCHECK(init_ocr_);
    perform_ocr_ = reinterpret_cast<PerformOcrFn>(
        library_.GetFunctionPointer("PerformOCR"));
    DCHECK(perform_ocr_);
  }

  // Main Content Extraction functions.
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    init_main_content_extraction_ =
        reinterpret_cast<InitMainContentExtractionFn>(
            library_.GetFunctionPointer("InitMainContentExtraction"));
    DCHECK(init_main_content_extraction_);
    extract_main_content_ = reinterpret_cast<ExtractMainContentFn>(
        library_.GetFunctionPointer("ExtractMainContent"));
    DCHECK(extract_main_content_);
  }
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
