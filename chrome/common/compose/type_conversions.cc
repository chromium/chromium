// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/compose/type_conversions.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

optimization_guide::proto::ComposeLength ComposeLength(
    compose::mojom::Length length) {
  switch (length) {
    case compose::mojom::Length::kShorter:
      return optimization_guide::proto::ComposeLength::COMPOSE_SHORTER;
    case compose::mojom::Length::kLonger:
      return optimization_guide::proto::ComposeLength::COMPOSE_LONGER;
    case compose::mojom::Length::kUnset:
    default:
      return optimization_guide::proto::ComposeLength::
          COMPOSE_UNSPECIFIED_LENGTH;
  }
}

optimization_guide::proto::ComposeTone ComposeTone(compose::mojom::Tone tone) {
  switch (tone) {
    case compose::mojom::Tone::kCasual:
      return optimization_guide::proto::ComposeTone::COMPOSE_INFORMAL;
    case compose::mojom::Tone::kFormal:
      return optimization_guide::proto::ComposeTone::COMPOSE_FORMAL;
    case compose::mojom::Tone::kUnset:
    default:
      return optimization_guide::proto::ComposeTone::COMPOSE_UNSPECIFIED_TONE;
  }
}

compose::mojom::ComposeStatus ComposeStatusFromOptimizationGuideResult(
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  if (result.has_value()) {
    return compose::mojom::ComposeStatus::kOk;
  }

  switch (result.error().error()) {
    case ModelExecutionError::kUnknown:
    case ModelExecutionError::kRequestThrottled:
    case ModelExecutionError::kGenericFailure:
    case ModelExecutionError::kRetryableError:
      return compose::mojom::ComposeStatus::kTryAgainLater;
    case ModelExecutionError::kInvalidRequest:
      return compose::mojom::ComposeStatus::kNotSuccessful;
    case ModelExecutionError::kPermissionDenied:
      return compose::mojom::ComposeStatus::kPermissionDenied;
    case ModelExecutionError::kNonRetryableError:
    case ModelExecutionError::kUnsupportedLanguage:
    case ModelExecutionError::kFiltered:
    case ModelExecutionError::kDisabled:
    case ModelExecutionError::kCancelled:
      return compose::mojom::ComposeStatus::kNotSuccessful;
  }
}

optimization_guide::proto::UserFeedback OptimizationFeedbackFromComposeFeedback(
    compose::mojom::UserFeedback feedback) {
  switch (feedback) {
    case compose::mojom::UserFeedback::kUserFeedbackPositive:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_UP;
    case compose::mojom::UserFeedback::kUserFeedbackNegative:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_THUMBS_DOWN;
    default:
      return optimization_guide::proto::UserFeedback::USER_FEEDBACK_UNSPECIFIED;
  }
}
