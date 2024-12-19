// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/compose/type_conversions.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

optimization_guide::proto::ComposeLength ComposeLength(
    compose::mojom::StyleModifier modifier) {
  switch (modifier) {
    case compose::mojom::StyleModifier::kShorter:
      return optimization_guide::proto::ComposeLength::COMPOSE_SHORTER;
    case compose::mojom::StyleModifier::kLonger:
      return optimization_guide::proto::ComposeLength::COMPOSE_LONGER;
    case compose::mojom::StyleModifier::kUnset:
    default:
      return optimization_guide::proto::ComposeLength::
          COMPOSE_UNSPECIFIED_LENGTH;
  }
}

optimization_guide::proto::ComposeTone ComposeTone(
    compose::mojom::StyleModifier modifier) {
  switch (modifier) {
    case compose::mojom::StyleModifier::kCasual:
      return optimization_guide::proto::ComposeTone::COMPOSE_INFORMAL;
    case compose::mojom::StyleModifier::kFormal:
      return optimization_guide::proto::ComposeTone::COMPOSE_FORMAL;
    case compose::mojom::StyleModifier::kUnset:
    default:
      return optimization_guide::proto::ComposeTone::COMPOSE_UNSPECIFIED_TONE;
  }
}

compose::mojom::ComposeStatus ComposeStatusFromOptimizationGuideResult(
    const optimization_guide::OptimizationGuideModelStreamingExecutionResult&
        result) {
  if (result.response.has_value()) {
    return compose::mojom::ComposeStatus::kOk;
  }

  switch (result.response.error().error()) {
    case ModelExecutionError::kUnknown:
    case ModelExecutionError::kGenericFailure:
      return compose::mojom::ComposeStatus::kServerError;
    case ModelExecutionError::kRequestThrottled:
      return compose::mojom::ComposeStatus::kRequestThrottled;
    case ModelExecutionError::kRetryableError:
      return compose::mojom::ComposeStatus::kRetryableError;
    case ModelExecutionError::kInvalidRequest:
      return compose::mojom::ComposeStatus::kInvalidRequest;
    case ModelExecutionError::kPermissionDenied:
      return compose::mojom::ComposeStatus::kPermissionDenied;
    case ModelExecutionError::kNonRetryableError:
      return compose::mojom::ComposeStatus::kNonRetryableError;
    case ModelExecutionError::kUnsupportedLanguage:
      return compose::mojom::ComposeStatus::kUnsupportedLanguage;
    case ModelExecutionError::kFiltered:
      return compose::mojom::ComposeStatus::kFiltered;
    case ModelExecutionError::kDisabled:
      return compose::mojom::ComposeStatus::kDisabled;
    case ModelExecutionError::kCancelled:
      return compose::mojom::ComposeStatus::kCancelled;
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

optimization_guide::proto::ComposeUpfrontInputMode ComposeUpfrontInputMode(
    compose::mojom::InputMode mode) {
  switch (mode) {
    case compose::mojom::InputMode::kPolish:
      return optimization_guide::proto::ComposeUpfrontInputMode::
          COMPOSE_POLISH_MODE;
    case compose::mojom::InputMode::kElaborate:
      return optimization_guide::proto::ComposeUpfrontInputMode::
          COMPOSE_ELABORATE_MODE;
    case compose::mojom::InputMode::kFormalize:
      return optimization_guide::proto::ComposeUpfrontInputMode::
          COMPOSE_FORMALIZE_MODE;
    case compose::mojom::InputMode::kUnset:
      return optimization_guide::proto::ComposeUpfrontInputMode::
          COMPOSE_UNSPECIFIED_MODE;
  }
}
