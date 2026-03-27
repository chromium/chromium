// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_MOJOM_TRAITS_H_

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_feedback_preview.h"
#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "base/base64.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ui/webui/ash/lobster/lobster.mojom.h"

namespace mojo {

template <>
struct EnumTraits<lobster::mojom::StatusCode, ash::LobsterErrorCode> {
  static lobster::mojom::StatusCode ToMojom(ash::LobsterErrorCode error_code) {
    static constexpr auto lobster_status_code_map =
        base::MakeFixedFlatMap<ash::LobsterErrorCode,
                               lobster::mojom::StatusCode>(
            {{ash::LobsterErrorCode::kBlockedOutputs,
              lobster::mojom::StatusCode::kBlockedOutputs},
             {ash::LobsterErrorCode::kNoInternetConnection,
              lobster::mojom::StatusCode::kNoInternetConnection},
             {ash::LobsterErrorCode::kUnknown,
              lobster::mojom::StatusCode::kUnknown},
             {ash::LobsterErrorCode::kResourceExhausted,
              lobster::mojom::StatusCode::kResourceExhausted},
             {ash::LobsterErrorCode::kInvalidArgument,
              lobster::mojom::StatusCode::kInvalidArgument},
             {ash::LobsterErrorCode::kBackendFailure,
              lobster::mojom::StatusCode::kBackendFailure},
             {ash::LobsterErrorCode::kUnsupportedLanguage,
              lobster::mojom::StatusCode::kUnsupportedLanguage},
             {ash::LobsterErrorCode::kRestrictedRegion,
              lobster::mojom::StatusCode::kRestrictedRegion},
             {ash::LobsterErrorCode::kContainsPeople,
              lobster::mojom::StatusCode::kContainsPeople}});
    return lobster_status_code_map.at(error_code);
  }

  static ash::LobsterErrorCode FromMojom(lobster::mojom::StatusCode input) {
    switch (input) {
      case lobster::mojom::StatusCode::kBlockedOutputs:
        return ash::LobsterErrorCode::kBlockedOutputs;
      case lobster::mojom::StatusCode::kNoInternetConnection:
        return ash::LobsterErrorCode::kNoInternetConnection;
      case lobster::mojom::StatusCode::kUnknown:
        return ash::LobsterErrorCode::kUnknown;
      case lobster::mojom::StatusCode::kResourceExhausted:
        return ash::LobsterErrorCode::kResourceExhausted;
      case lobster::mojom::StatusCode::kInvalidArgument:
        return ash::LobsterErrorCode::kInvalidArgument;
      case lobster::mojom::StatusCode::kBackendFailure:
        return ash::LobsterErrorCode::kBackendFailure;
      case lobster::mojom::StatusCode::kUnsupportedLanguage:
        return ash::LobsterErrorCode::kUnsupportedLanguage;
      case lobster::mojom::StatusCode::kRestrictedRegion:
        return ash::LobsterErrorCode::kRestrictedRegion;
      case lobster::mojom::StatusCode::kContainsPeople:
        return ash::LobsterErrorCode::kContainsPeople;
      case lobster::mojom::StatusCode::kOk:
        LOG(ERROR) << "Can not convert Lobster mojom OK status code into "
                      "LobsterErrorCode";
        NOTREACHED();
    }
  }
};

template <>
class StructTraits<lobster::mojom::FeedbackPreviewDataView,
                   ash::LobsterFeedbackPreview> {
 public:
  static const GURL preview_data_url(
      const ash::LobsterFeedbackPreview& feedback_preview) {
    return GURL(base::StrCat(
        {"data:image/jpeg;base64,",
         base::Base64Encode(feedback_preview.preview_image_bytes)}));
  }

  static const std::map<std::string, std::string>& fields(
      const ash::LobsterFeedbackPreview& feedback_preview) {
    return feedback_preview.fields;
  }

  static bool Read(lobster::mojom::FeedbackPreviewDataView data,
                   ash::LobsterFeedbackPreview* out) {
    // `LobsterFeedbackPreview` are only sent from C++ to WebUI, so
    // deserialization should never happen.
    return false;
  }
};

template <>
struct EnumTraits<lobster::mojom::WebUIMetricEvent, ash::LobsterMetricState> {
  static lobster::mojom::WebUIMetricEvent ToMojom(
      ash::LobsterMetricState error_code) {
    switch (error_code) {
      case ash::LobsterMetricState::kQueryPageImpression:
        return lobster::mojom::WebUIMetricEvent::kQueryPageImpression;
      case ash::LobsterMetricState::kRequestInitialCandidates:
        return lobster::mojom::WebUIMetricEvent::kRequestInitialCandidates;
      case ash::LobsterMetricState::kRequestInitialCandidatesSuccess:
        return lobster::mojom::WebUIMetricEvent::
            kRequestInitialCandidatesSuccess;
      case ash::LobsterMetricState::kRequestInitialCandidatesError:
        return lobster::mojom::WebUIMetricEvent::kRequestInitialCandidatesError;
      case ash::LobsterMetricState::kInitialCandidatesImpression:
        return lobster::mojom::WebUIMetricEvent::kInitialCandidatesImpression;
      case ash::LobsterMetricState::kRequestMoreCandidates:
        return lobster::mojom::WebUIMetricEvent::kRequestMoreCandidates;
      case ash::LobsterMetricState::kRequestMoreCandidatesSuccess:
        return lobster::mojom::WebUIMetricEvent::kRequestMoreCandidatesSuccess;
      case ash::LobsterMetricState::kRequestMoreCandidatesError:
        return lobster::mojom::WebUIMetricEvent::kRequestMoreCandidatesError;
      case ash::LobsterMetricState::kMoreCandidatesAppended:
        return lobster::mojom::WebUIMetricEvent::kMoreCandidatesAppended;
      case ash::LobsterMetricState::kFeedbackThumbsUp:
        return lobster::mojom::WebUIMetricEvent::kFeedbackThumbsUp;
      case ash::LobsterMetricState::kFeedbackThumbsDown:
        return lobster::mojom::WebUIMetricEvent::kFeedbackThumbsDown;
      case ash::LobsterMetricState::kShownOpportunity:
      case ash::LobsterMetricState::kBlocked:
      case ash::LobsterMetricState::kRightClickTriggerImpression:
      case ash::LobsterMetricState::kRightClickTriggerFired:
      case ash::LobsterMetricState::kRightClickTriggerNeedsConsent:
      case ash::LobsterMetricState::kQuickInsertTriggerImpression:
      case ash::LobsterMetricState::kQuickInsertTriggerFired:
      case ash::LobsterMetricState::kQuickInsertTriggerNeedsConsent:
      case ash::LobsterMetricState::kConsentScreenImpression:
      case ash::LobsterMetricState::kConsentAccepted:
      case ash::LobsterMetricState::kConsentRejected:
      case ash::LobsterMetricState::kCandidateDownload:
      case ash::LobsterMetricState::kCandidateDownloadSuccess:
      case ash::LobsterMetricState::kCandidateDownloadError:
      case ash::LobsterMetricState::kCommitAsDownload:
      case ash::LobsterMetricState::kCommitAsDownloadSuccess:
      case ash::LobsterMetricState::kCommitAsDownloadError:
      case ash::LobsterMetricState::kCommitAsInsert:
      case ash::LobsterMetricState::kCommitAsInsertSuccess:
      case ash::LobsterMetricState::kCommitAsInsertError:
      case ash::LobsterMetricState::kBlockedByConsent:
      case ash::LobsterMetricState::kBlockedByAccountCapabilities:
      case ash::LobsterMetricState::kBlockedByAccountType:
      case ash::LobsterMetricState::kBlockedByGeolocation:
      case ash::LobsterMetricState::kBlockedByInputField:
      case ash::LobsterMetricState::kBlockedBySettings:
      case ash::LobsterMetricState::kBlockedByInternetConnection:
      case ash::LobsterMetricState::kBlockedByInputMethod:
      case ash::LobsterMetricState::kBlockedByFeatureFlags:
      case ash::LobsterMetricState::kBlockedByHardware:
      case ash::LobsterMetricState::kBlockedByKioskMode:
      case ash::LobsterMetricState::kBlockedByFormFactor:
      case ash::LobsterMetricState::kBlockedByPolicy:
        return lobster::mojom::WebUIMetricEvent::kUnknown;
    }
  }

  static ash::LobsterMetricState FromMojom(
      lobster::mojom::WebUIMetricEvent mojom_metric_event) {
    switch (mojom_metric_event) {
      case lobster::mojom::WebUIMetricEvent::kUnknown:
        LOG(ERROR) << "Unknown Lobster WebUI Metric event";
        NOTREACHED();
      case lobster::mojom::WebUIMetricEvent::kQueryPageImpression:
        return ash::LobsterMetricState::kQueryPageImpression;
      case lobster::mojom::WebUIMetricEvent::kRequestInitialCandidates:
        return ash::LobsterMetricState::kRequestInitialCandidates;
      case lobster::mojom::WebUIMetricEvent::kRequestInitialCandidatesSuccess:
        return ash::LobsterMetricState::kRequestInitialCandidatesSuccess;
      case lobster::mojom::WebUIMetricEvent::kRequestInitialCandidatesError:
        return ash::LobsterMetricState::kRequestInitialCandidatesError;
      case lobster::mojom::WebUIMetricEvent::kInitialCandidatesImpression:
        return ash::LobsterMetricState::kInitialCandidatesImpression;
      case lobster::mojom::WebUIMetricEvent::kRequestMoreCandidates:
        return ash::LobsterMetricState::kRequestMoreCandidates;
      case lobster::mojom::WebUIMetricEvent::kRequestMoreCandidatesSuccess:
        return ash::LobsterMetricState::kRequestMoreCandidatesSuccess;
      case lobster::mojom::WebUIMetricEvent::kRequestMoreCandidatesError:
        return ash::LobsterMetricState::kRequestMoreCandidatesError;
      case lobster::mojom::WebUIMetricEvent::kMoreCandidatesAppended:
        return ash::LobsterMetricState::kMoreCandidatesAppended;
      case lobster::mojom::WebUIMetricEvent::kFeedbackThumbsUp:
        return ash::LobsterMetricState::kFeedbackThumbsUp;
      case lobster::mojom::WebUIMetricEvent::kFeedbackThumbsDown:
        return ash::LobsterMetricState::kFeedbackThumbsDown;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_MOJOM_TRAITS_H_
