// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection//core/language_detection_provider.h"

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "build/build_config.h"

namespace language_detection {
// Allows supplying a local model file. The file cannot be opened if we're in
// a sandbox. You can use --no-sandbox when running tests that rely on this
// flag.
// TODO(https://crbug.com/354069716): Move this to the model service in the
// browser.
BASE_FEATURE(kLanguageDetectionModelForTesting,
             "LanguageDetectionModelForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string> kLanguageDetectionModelForTestingPath{
    &kLanguageDetectionModelForTesting, "path",
    "components/test/data/translate/valid_model.tflite"};

LanguageDetectionModel& GetLanguageDetectionModel() {
  static base::NoDestructor<LanguageDetectionModel> instance;
  if (base::FeatureList::IsEnabled(kLanguageDetectionModelForTesting)) {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
    base::FilePath model_file_path = source_root_dir.AppendASCII(
        kLanguageDetectionModelForTestingPath.Get());
    base::File model_file(model_file_path,
                          (base::File::FLAG_OPEN | base::File::FLAG_READ));
    if (!model_file.IsValid()) {
      // See the note on kLanguageDetectionModelForTesting if you hit this.
      LOG(ERROR) << model_file_path;
      LOG(ERROR) << "error_details: " << model_file.error_details();
    }
    instance->UpdateWithFile(std::move(model_file));
  }
  return *instance;
}

}  // namespace language_detection
