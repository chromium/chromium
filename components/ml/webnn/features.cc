// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ml/webnn/features.h"
#include "base/feature_list.h"

namespace webnn::features {

BASE_FEATURE(kEnableMachineLearningNeuralNetworkService,
             "MachineLearningNeuralNetworkService",
             base::FEATURE_DISABLED_BY_DEFAULT);

}
