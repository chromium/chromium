// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/testing/language_detection_test_utils.h"

#include <memory>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/language_detection/core/language_detection_model.h"
#include "gtest/gtest.h"

namespace language_detection {
base::File GetValidModelFile() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  base::FilePath model_file_path = source_root_dir.AppendASCII("components")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("translate")
                                       .AppendASCII("valid_model.tflite");
  base::File file(model_file_path,
                  (base::File::FLAG_OPEN | base::File::FLAG_READ));
  return file;
}

std::unique_ptr<LanguageDetectionModel> GetValidLanguageModel() {
  auto instance = std::make_unique<LanguageDetectionModel>();
  if (!instance->IsAvailable()) {
    base::File file = GetValidModelFile();
    instance->UpdateWithFile(std::move(file));
  }
  EXPECT_TRUE(instance->IsAvailable());
  return instance;
}
}  // namespace language_detection
