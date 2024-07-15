// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "components/optimization_guide/core/model_enums.h"
#include "components/optimization_guide/proto/models.pb.h"

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
std::optional<base::FilePath> StringToFilePath(const std::string& str_path);

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
std::optional<
    std::pair<std::string, std::optional<optimization_guide::proto::Any>>>
GetModelOverrideForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target);

// Checks all the files in |file_paths_to_check| exists.
bool CheckAllPathsExist(const std::vector<base::FilePath>& file_paths_to_check);

// Returns the relative filepath for |child| w.r.t. |parent|. For example, with
// child="/foo/bar/baz/abc.txt"  and parent="/foo/bar/", this returns the
// relative path "baz/abc.txt".
base::FilePath ConvertToRelativePath(const base::FilePath& parent,
                                     const base::FilePath& child);

// Returns the hash of |model_cache_key| that can be used as key in a
// persistent dict, or can be used as file paths.
std::string GetModelCacheKeyHash(proto::ModelCacheKey model_cache_key);

// Records the model remove version histograms. One general histogram and one
// histogram broken down by |optimization_target| are recorded.
void RecordPredictionModelStoreModelRemovalVersionHistogram(
    proto::OptimizationTarget optimization_target,
    PredictionModelStoreModelRemovalReason model_removal_reason);

// Returns whether the model for `opt_target` with `model_version` is in the
// `killswitch_model_versions`.
bool IsPredictionModelVersionInKillSwitch(
    const std::map<proto::OptimizationTarget, std::set<int64_t>>&
        killswitch_model_versions,
    proto::OptimizationTarget opt_target,
    int64_t model_version);

// Returns the model info parsed from |model_info_path|.
std::optional<proto::ModelInfo> ParseModelInfoFromFile(
    const base::FilePath& model_info_path);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_UTIL_H_
