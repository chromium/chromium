// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_
#define CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_

#include "chrome/common/compose/compose.mojom.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
optimization_guide::proto::ComposeLength ComposeLength(
    compose::mojom::StyleModifier modifier);
optimization_guide::proto::ComposeTone ComposeTone(
    compose::mojom::StyleModifier modifier);
compose::mojom::ComposeStatus ComposeStatusFromOptimizationGuideResult(
    const optimization_guide::OptimizationGuideModelStreamingExecutionResult&
        result);
optimization_guide::proto::UserFeedback OptimizationFeedbackFromComposeFeedback(
    compose::mojom::UserFeedback feedback);
optimization_guide::proto::ComposeUpfrontInputMode ComposeUpfrontInputMode(
    compose::mojom::InputMode mode);
#endif  // CHROME_COMMON_COMPOSE_TYPE_CONVERSIONS_H_
