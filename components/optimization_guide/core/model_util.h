// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

// Returns the string than can be used to record histograms for the optimization
// target. If adding a histogram to use the string or adding an optimization
// target, update the OptimizationGuide.OptimizationTargets histogram suffixes
// in histograms.xml.
std::string GetStringNameForOptimizationTarget(
    proto::OptimizationTarget optimization_target);

// Returns the file path represented by the given string, handling platform
// differences in the conversion. nullopt is only returned iff the passed string
// is empty.
absl::optional<base::FilePath> StringToFilePath(const std::string& str_path);

// Returns a string representation of the given |file_path|, handling platform
// differences in the conversion.
std::string FilePathToString(const base::FilePath& file_path);

// Returns the base file name to use for storing all prediction models.
base::FilePath GetBaseFileNameForModels();

// Returns the base file name to use for storing the model info that holds the
// metadata.
base::FilePath GetBaseFileNameForModelInfo();

// Returns the separator used in the model override switch below, which differs
// between platforms.
std::string ModelOverrideSeparator();

// Returns the file path string and metadata for the model provided via
// command-line for |optimization_target|, if applicable.
absl::optional<
    std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
GetModelOverrideForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target);

// Checks all the files in |file_paths_to_check| exists.
bool CheckAllPathsExist(const std::vector<base::FilePath>& file_paths_to_check);

// Returns the hash of |model_cache_key| that can be used as key in a
// persistent dict, or can be used as file paths.
std::string GetModelCacheKeyHash(proto::ModelCacheKey model_cache_key);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_
