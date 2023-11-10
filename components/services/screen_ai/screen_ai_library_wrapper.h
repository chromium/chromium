// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_
#define COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_

#include <stdint.h>
#include <cstdint>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace screen_ai {

// Wrapper class for Chrome Screen AI library.
class ScreenAILibraryWrapper {
 public:
  struct MainContentExtractionModelData {
    MainContentExtractionModelData(std::vector<char> config,
                                   std::vector<char> tflite);
    MainContentExtractionModelData(const MainContentExtractionModelData&) =
        delete;
    MainContentExtractionModelData& operator=(
        const MainContentExtractionModelData&) = delete;
    ~MainContentExtractionModelData();
    std::vector<char> config;
    std::vector<char> tflite;
  };

  ScreenAILibraryWrapper();
  ScreenAILibraryWrapper(const ScreenAILibraryWrapper&) = delete;
  ScreenAILibraryWrapper& operator=(const ScreenAILibraryWrapper&) = delete;
  ~ScreenAILibraryWrapper() = default;

  bool Load(const base::FilePath& library_path);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetLogger();
#endif

  void GetLibraryVersion(uint32_t& major, uint32_t& minor);
  void SetFileContentFunctions(
      uint32_t (*get_file_content_size)(const char* relative_file_path),
      void (*get_file_content)(const char* relative_file_path,
                               uint32_t buffer_size,
                               char* buffer));
  void EnableDebugMode();
  bool InitLayoutExtraction();

  bool InitOCR();
  bool InitMainContentExtraction();

  absl::optional<chrome_screen_ai::VisualAnnotation> ExtractLayout(
      const SkBitmap& image);
  absl::optional<chrome_screen_ai::VisualAnnotation> PerformOcr(
      const SkBitmap& image);
  absl::optional<std::vector<int32_t>> ExtractMainContent(
      const std::string& serialized_view_hierarchy);

 private:
  template <typename T>
  bool LoadFunction(T& function_variable, const char* function_name);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Sets a function to receive library logs and add them to Chrome logs.
  typedef void (*SetLoggerFn)(void (*logger_func)(int /*severity*/,
                                                  const char* /*message*/));
  SetLoggerFn set_logger_ = nullptr;
#endif

  // Gets the library version number.
  typedef void (*GetLibraryVersionFn)(uint32_t& major, uint32_t& minor);
  GetLibraryVersionFn get_library_version_ = nullptr;

  typedef void (*SetFileContentFunctionsFn)(
      uint32_t (*get_file_content_size)(const char* /*relative_file_path*/),
      void (*get_file_content)(const char* /*relative_file_path*/,
                               uint32_t /*buffer_size*/,
                               char* /*buffer*/));
  SetFileContentFunctionsFn set_file_content_functions_ = nullptr;

  // Enables the debug mode which stores all i/o protos in the temp folder.
  typedef void (*EnableDebugModeFn)();
  EnableDebugModeFn enable_debug_mode_ = nullptr;

  // Initializes the pipeline for layout extraction.
  typedef bool (*InitLayoutExtractionFn)();
  InitLayoutExtractionFn init_layout_extraction_ = nullptr;

  // Initializes the pipeline for OCR.
  typedef bool (*InitOCRFn)();
  InitOCRFn init_ocr_ = nullptr;

  // Initializes the pipeline for main content extraction.
  typedef bool (*InitMainContentExtractionFn)();
  InitMainContentExtractionFn init_main_content_extraction_ = nullptr;

  // Sends the given bitmap to layout extraction pipeline and returns visual
  // annotations. The annotations will be returned as a serialized
  // VisualAnnotation proto if the task is successful, otherwise nullptr is
  // returned. The returned string is not null-terminated and its size will be
  // put in `serialized_visual_annotation_length`. The allocated memory should
  // be released in the library to avoid cross boundary memory issues.
  typedef char* (*ExtractLayoutFn)(
      const SkBitmap& /*bitmap*/,
      uint32_t& /*serialized_visual_annotation_length*/);
  ExtractLayoutFn extract_layout_ = nullptr;

  // Sends the given bitmap to the OCR pipeline and returns visual
  // annotations. The annotations will be returned as a serialized
  // VisualAnnotation proto if the task is successful, otherwise nullptr is
  // returned. The returned string is not null-terminated and its size will be
  // put in `serialized_visual_annotation_length`. The allocated memory should
  // be released in the library to avoid cross boundary memory issues.
  typedef char* (*PerformOcrFn)(
      const SkBitmap& /*bitmap*/,
      uint32_t& /*serialized_visual_annotation_length*/);
  PerformOcrFn perform_ocr_ = nullptr;

  // Passes the given accessibility tree proto to Screen2x pipeline and returns
  // the main content ids. The input is in form of a serialized ViewHierarchy
  // proto.
  // If the task is successful, main content node ids will be returned as an
  // integer array, otherwise nullptr is returned. The number of returned ids
  // will be put in `content_node_ids_length`. The allocated memory should be
  // released in the library to avoid cross boundary memory issues.
  typedef int32_t* (*ExtractMainContentFn)(
      const char* /*serialized_view_hierarchy*/,
      uint32_t /*serialized_view_hierarchy_length*/,
      uint32_t& /*content_node_ids_length*/);
  ExtractMainContentFn extract_main_content_ = nullptr;

  // Releases a memory that is allocated by the library.
  typedef void (*FreeLibraryAllocatedInt32ArrayFn)(int32_t* /*memory*/);
  FreeLibraryAllocatedInt32ArrayFn free_library_allocated_int32_array_ =
      nullptr;

  // Releases a memory that is allocated by the library.
  typedef void (*FreeLibraryAllocatedCharArrayFn)(char* /*memory*/);
  FreeLibraryAllocatedCharArrayFn free_library_allocated_char_array_ = nullptr;

  base::ScopedNativeLibrary library_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_
