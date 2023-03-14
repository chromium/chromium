// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_
#define COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace screen_ai {

// Wrapper class for Chrome Screen AI library.
class ScreenAILibraryWrapper {
 public:
  ScreenAILibraryWrapper();
  ScreenAILibraryWrapper(const ScreenAILibraryWrapper&) = delete;
  ScreenAILibraryWrapper& operator=(const ScreenAILibraryWrapper&) = delete;
  ~ScreenAILibraryWrapper() = default;

  bool Init(const base::FilePath& library_path);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetLogger();
#endif

  void GetLibraryVersion(uint32_t& major, uint32_t& minor);
  void EnableDebugMode();
  bool InitLayoutExtraction();
  bool InitOCR(const base::FilePath& models_folder);
  bool InitMainContentExtraction(base::File& model_config_file,
                                 base::File& model_tflite_file);
  bool ExtractLayout(const SkBitmap& image, std::string& annotation_proto);
  bool PerformOcr(const SkBitmap& image, std::string& annotation_proto);
  bool ExtractMainContent(const std::string& serialized_view_hierarchy,
                          std::vector<int32_t>& node_ids);

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

  // Enables the debug mode which stores all i/o protos in the temp folder.
  typedef void (*EnableDebugModeFn)();
  EnableDebugModeFn enable_debug_mode_ = nullptr;

  // Initializes the pipeline for layout extraction.
  typedef bool (*InitLayoutExtractionFn)();
  InitLayoutExtractionFn init_layout_extraction_ = nullptr;

  // Initializes the pipeline for OCR.
  // |models_folder| is a null terminated string pointing to the
  // folder that includes model files for OCR.
  // TODO(http://crbug.com/1278249): Replace |models_folder| with file
  // handle(s).
  typedef bool (*InitOCRFn)(const char* /*models_folder*/);
  InitOCRFn init_ocr_ = nullptr;

  // Initializes the pipeline for main content extraction.
  // |model_config| and |model_tflite| pass content of the required files to
  // initialize Screen2x engine.
  typedef bool (*InitMainContentExtractionFn)(const char* model_config,
                                              uint32_t model_config_length,
                                              const char* model_tflite,
                                              uint32_t model_tflite_length);
  InitMainContentExtractionFn init_main_content_extraction_ = nullptr;

  // Sends the given bitmap to layout extraction pipeline and returns visual
  // annotations. The annotations will be returned as a serialized
  // VisualAnnotation proto.
  // `serialized_visual_annotation_length` will return the size of required
  // buffer to read the output serialized proto. The buffered results are
  // guaranteed to be kept until the next call to any function of the library.
  // The caller should then allocate the required buffer and call
  // `ReadBufferedCharArray` to get the results. Note that the results can be
  // read only once.
  typedef bool (*ExtractLayoutFn)(
      const SkBitmap& /*bitmap*/,
      uint32_t& /*serialized_visual_annotation_length*/);
  ExtractLayoutFn extract_layout_ = nullptr;

  // Sends the given bitmap to the OCR pipeline and returns visual
  // annotations. The annotations will be returned as a serialized
  // VisualAnnotation proto.
  // `serialized_visual_annotation_length` will return the size of required
  // buffer to read the output serialized proto. The buffered results are
  // guaranteed to be kept until the next call to any function of the library.
  // The caller should then allocate the required buffer and call
  // `ReadBufferedCharArray` to get the results. Note that the results can be
  // read only once.
  typedef bool (*PerformOcrFn)(
      const SkBitmap& /*bitmap*/,
      uint32_t& /*serialized_visual_annotation_length*/);
  PerformOcrFn perform_ocr_ = nullptr;

  // Passes the given accessibility tree proto to Screen2x pipeline and returns
  // the main content ids. The input is in form of a serialized ViewHierarchy
  // proto.
  // `content_node_ids_length` returns the size of required buffer to read the
  // output content node ids. The buffered results are guaranteed to be kept
  // until the next call to any function of the library.
  // The caller should then allocate the required buffer and call
  // `ReadBufferedInt32Array` to get the results. Note that the results can be
  // read only once.
  typedef bool (*ExtractMainContentFn)(
      const char* /*serialized_view_hierarchy*/,
      uint32_t /*serialized_view_hierarchy_length*/,
      uint32_t& /*content_node_ids_length*/);
  ExtractMainContentFn extract_main_content_ = nullptr;

  // Reads buffered int32 array. This function is used to read the int32*
  // results that are computed by other functions and kept in library's memory.
  // The read operation can be done only once and the results are cleared after
  // reading.
  typedef bool (*ReadBufferedInt32ArrayFn)(int32_t* results,
                                           uint32_t max_count);
  ReadBufferedInt32ArrayFn read_buffered_int32_array_;

  // Reads buffered char array. This function is used to read the char*
  // results that are computed by other functions and kept in library's memory.
  // The read operation can be done only once and the results are cleared after
  // reading.
  typedef bool (*ReadBufferedCharArrayFn)(char* results, uint32_t max_count);
  ReadBufferedCharArrayFn read_buffered_char_array_;

  base::ScopedNativeLibrary library_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_LIBRARY_WRAPPER_H_
