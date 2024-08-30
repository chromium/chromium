// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_MOJOM_TRAITS_H_

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_feedback_preview.h"
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
              lobster::mojom::StatusCode::kRestrictedRegion}});
    return lobster_status_code_map.at(error_code);
  }

  static bool FromMojom(lobster::mojom::StatusCode input,
                        ash::LobsterErrorCode* out) {
    switch (input) {
      case lobster::mojom::StatusCode::kBlockedOutputs:
        *out = ash::LobsterErrorCode::kBlockedOutputs;
        return true;
      case lobster::mojom::StatusCode::kNoInternetConnection:
        *out = ash::LobsterErrorCode::kNoInternetConnection;
        return true;
      case lobster::mojom::StatusCode::kUnknown:
        *out = ash::LobsterErrorCode::kUnknown;
        return true;
      case lobster::mojom::StatusCode::kResourceExhausted:
        *out = ash::LobsterErrorCode::kResourceExhausted;
        return true;
      case lobster::mojom::StatusCode::kInvalidArgument:
        *out = ash::LobsterErrorCode::kInvalidArgument;
        return true;
      case lobster::mojom::StatusCode::kBackendFailure:
        *out = ash::LobsterErrorCode::kBackendFailure;
        return true;
      case lobster::mojom::StatusCode::kUnsupportedLanguage:
        *out = ash::LobsterErrorCode::kUnsupportedLanguage;
        return true;
      case lobster::mojom::StatusCode::kRestrictedRegion:
        *out = ash::LobsterErrorCode::kRestrictedRegion;
        return true;
      case lobster::mojom::StatusCode::kOk:
        LOG(ERROR) << "Can not convert Lobster mojom OK status code into "
                      "LobsterErrorCode";
        return false;
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

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_MOJOM_TRAITS_H_
