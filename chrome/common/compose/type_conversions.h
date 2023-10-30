// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_
#define CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_

#include "chrome/common/compose/compose.mojom.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/compose.pb.h"

optimization_guide::proto::ComposeLength ComposeLength(
    compose::mojom::Length length);
optimization_guide::proto::ComposeTone ComposeTone(compose::mojom::Tone tone);
compose::mojom::ComposeStatus ComposeStatusFromOptimizationGuideResult(
    optimization_guide::OptimizationGuideModelExecutionResult result);

#endif  // CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_
