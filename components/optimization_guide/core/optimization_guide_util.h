// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace optimization_guide {

// Returns the string than can be used to record histograms for the optimization
// target. If adding a histogram to use the string or adding an optimization
// target, update the OptimizationGuide.OptimizationTargets histogram suffixes
// in histograms.xml.
std::string GetStringNameForOptimizationTarget(
    proto::OptimizationTarget optimization_target);

// Returns false if the host is an IP address, localhosts, or an invalid
// host that is not supported by the remote optimization guide.
bool IsHostValidToFetchFromRemoteOptimizationGuide(const std::string& host);

// Returns the set of active field trials that are allowed to be sent to the
// remote Optimization Guide Service.
google::protobuf::RepeatedPtrField<proto::FieldTrial>
GetActiveFieldTrialsAllowedForFetch();

// Returns the file path that holds the model file for |model|, if applicable.
base::Optional<base::FilePath> GetFilePathFromPredictionModel(
    const proto::PredictionModel& model);

// Fills |model| with the path for which the corresponding model file can be
// found.
void SetFilePathInPredictionModel(const base::FilePath& file_path,
                                  proto::PredictionModel* model);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_UTIL_H_
