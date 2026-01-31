// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_search_type_converters.h"

#include "components/contextual_search/input_state_model.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/contextual_search_mojom_traits.h"

namespace contextual_search {

composebox_query::mojom::FileUploadStatus ToMojom(FileUploadStatus status) {
  switch (status) {
    case FileUploadStatus::kNotUploaded:
      return composebox_query::mojom::FileUploadStatus::kNotUploaded;
    case FileUploadStatus::kProcessing:
      return composebox_query::mojom::FileUploadStatus::kProcessing;
    case FileUploadStatus::kValidationFailed:
      return composebox_query::mojom::FileUploadStatus::kValidationFailed;
    case FileUploadStatus::kUploadStarted:
      return composebox_query::mojom::FileUploadStatus::kUploadStarted;
    case FileUploadStatus::kUploadSuccessful:
      return composebox_query::mojom::FileUploadStatus::kUploadSuccessful;
    case FileUploadStatus::kUploadFailed:
      return composebox_query::mojom::FileUploadStatus::kUploadFailed;
    case FileUploadStatus::kUploadExpired:
      return composebox_query::mojom::FileUploadStatus::kUploadExpired;
    case FileUploadStatus::kProcessingSuggestSignalsReady:
      return composebox_query::mojom::FileUploadStatus::
          kProcessingSuggestSignalsReady;
  }
}

FileUploadStatus FromMojom(composebox_query::mojom::FileUploadStatus status) {
  switch (status) {
    case composebox_query::mojom::FileUploadStatus::kNotUploaded:
      return FileUploadStatus::kNotUploaded;
    case composebox_query::mojom::FileUploadStatus::kProcessing:
      return FileUploadStatus::kProcessing;
    case composebox_query::mojom::FileUploadStatus::kValidationFailed:
      return FileUploadStatus::kValidationFailed;
    case composebox_query::mojom::FileUploadStatus::kUploadStarted:
      return FileUploadStatus::kUploadStarted;
    case composebox_query::mojom::FileUploadStatus::kUploadSuccessful:
      return FileUploadStatus::kUploadSuccessful;
    case composebox_query::mojom::FileUploadStatus::kUploadFailed:
      return FileUploadStatus::kUploadFailed;
    case composebox_query::mojom::FileUploadStatus::kUploadExpired:
      return FileUploadStatus::kUploadExpired;
    case composebox_query::mojom::FileUploadStatus::
        kProcessingSuggestSignalsReady:
      return FileUploadStatus::kProcessingSuggestSignalsReady;
  }
}

composebox_query::mojom::FileUploadErrorType ToMojom(FileUploadErrorType type) {
  switch (type) {
    case FileUploadErrorType::kUnknown:
      return composebox_query::mojom::FileUploadErrorType::kUnknown;
    case FileUploadErrorType::kBrowserProcessingError:
      return composebox_query::mojom::FileUploadErrorType::
          kBrowserProcessingError;
    case FileUploadErrorType::kNetworkError:
      return composebox_query::mojom::FileUploadErrorType::kNetworkError;
    case FileUploadErrorType::kServerError:
      return composebox_query::mojom::FileUploadErrorType::kServerError;
    case FileUploadErrorType::kServerSizeLimitExceeded:
      return composebox_query::mojom::FileUploadErrorType::
          kServerSizeLimitExceeded;
    case FileUploadErrorType::kAborted:
      return composebox_query::mojom::FileUploadErrorType::kAborted;
    case FileUploadErrorType::kImageProcessingError:
      return composebox_query::mojom::FileUploadErrorType::
          kImageProcessingError;
  }
}

FileUploadErrorType FromMojom(
    composebox_query::mojom::FileUploadErrorType type) {
  switch (type) {
    case composebox_query::mojom::FileUploadErrorType::kUnknown:
      return FileUploadErrorType::kUnknown;
    case composebox_query::mojom::FileUploadErrorType::kBrowserProcessingError:
      return FileUploadErrorType::kBrowserProcessingError;
    case composebox_query::mojom::FileUploadErrorType::kNetworkError:
      return FileUploadErrorType::kNetworkError;
    case composebox_query::mojom::FileUploadErrorType::kServerError:
      return FileUploadErrorType::kServerError;
    case composebox_query::mojom::FileUploadErrorType::kServerSizeLimitExceeded:
      return FileUploadErrorType::kServerSizeLimitExceeded;
    case composebox_query::mojom::FileUploadErrorType::kAborted:
      return FileUploadErrorType::kAborted;
    case composebox_query::mojom::FileUploadErrorType::kImageProcessingError:
      return FileUploadErrorType::kImageProcessingError;
  }
}

composebox_query::mojom::InputStatePtr ToMojom(const InputState& state) {
  auto mojom_state = composebox_query::mojom::InputState::New();

  mojom_state->active_tool = state.active_tool;
  mojom_state->active_model = state.active_model;

  mojom_state->disabled_tools = state.disabled_tools;
  mojom_state->disabled_models = state.disabled_models;
  mojom_state->disabled_input_types = state.disabled_input_types;

  mojom_state->allowed_tools = state.allowed_tools;
  mojom_state->allowed_models = state.allowed_models;
  mojom_state->allowed_input_types = state.allowed_input_types;
  return mojom_state;
}

}  // namespace contextual_search
