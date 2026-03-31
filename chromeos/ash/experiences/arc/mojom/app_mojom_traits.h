// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_MOJOM_APP_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_MOJOM_APP_MOJOM_TRAITS_H_

#include <optional>

#include "base/notreached.h"
#include "chromeos/ash/experiences/arc/app/arc_playstore_search_request_state.h"
#include "chromeos/ash/experiences/arc/mojom/app.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::AppDiscoveryRequestState,
                  arc::ArcPlayStoreSearchRequestState> {
  using ArcState = arc::ArcPlayStoreSearchRequestState;
  using MojoState = arc::mojom::AppDiscoveryRequestState;

  static MojoState ToMojom(ArcState input) {
    switch (input) {
      case ArcState::SUCCESS:
        return MojoState::SUCCESS;
      case ArcState::CANCELED:
        return MojoState::CANCELED;
      case ArcState::ERROR_DEPRECATED:
        return MojoState::ERROR_DEPRECATED;
      case ArcState::PLAY_STORE_PROXY_NOT_AVAILABLE:
        return MojoState::PLAY_STORE_PROXY_NOT_AVAILABLE;
      case ArcState::FAILED_TO_CALL_CANCEL:
        return MojoState::FAILED_TO_CALL_CANCEL;
      case ArcState::FAILED_TO_CALL_FINDAPPS:
        return MojoState::FAILED_TO_CALL_FINDAPPS;
      case ArcState::REQUEST_HAS_INVALID_PARAMS:
        return MojoState::REQUEST_HAS_INVALID_PARAMS;
      case ArcState::REQUEST_TIMEOUT:
        return MojoState::REQUEST_TIMEOUT;
      case ArcState::PHONESKY_RESULT_REQUEST_CODE_UNMATCHED:
        return MojoState::PHONESKY_RESULT_REQUEST_CODE_UNMATCHED;
      case ArcState::PHONESKY_RESULT_SESSION_ID_UNMATCHED:
        return MojoState::PHONESKY_RESULT_SESSION_ID_UNMATCHED;
      case ArcState::PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED:
        return MojoState::PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED;
      case ArcState::PHONESKY_APP_DISCOVERY_NOT_AVAILABLE:
        return MojoState::PHONESKY_APP_DISCOVERY_NOT_AVAILABLE;
      case ArcState::PHONESKY_VERSION_NOT_SUPPORTED:
        return MojoState::PHONESKY_VERSION_NOT_SUPPORTED;
      case ArcState::PHONESKY_UNEXPECTED_EXCEPTION:
        return MojoState::PHONESKY_UNEXPECTED_EXCEPTION;
      case ArcState::PHONESKY_MALFORMED_QUERY:
        return MojoState::PHONESKY_MALFORMED_QUERY;
      case ArcState::PHONESKY_INTERNAL_ERROR:
        return MojoState::PHONESKY_INTERNAL_ERROR;
      case ArcState::PHONESKY_RESULT_INVALID_DATA:
        return MojoState::PHONESKY_RESULT_INVALID_DATA;
      case ArcState::CHROME_GOT_INVALID_RESULT:
      case ArcState::STATE_COUNT:
        break;
    }
    NOTREACHED();
  }

  static std::optional<ArcState> FromMojom(MojoState input) {
    switch (input) {
      case MojoState::SUCCESS:
        return ArcState::SUCCESS;
      case MojoState::CANCELED:
        return ArcState::CANCELED;
      case MojoState::ERROR_DEPRECATED:
        return ArcState::ERROR_DEPRECATED;
      case MojoState::PLAY_STORE_PROXY_NOT_AVAILABLE:
        return ArcState::PLAY_STORE_PROXY_NOT_AVAILABLE;
      case MojoState::FAILED_TO_CALL_CANCEL:
        return ArcState::FAILED_TO_CALL_CANCEL;
      case MojoState::FAILED_TO_CALL_FINDAPPS:
        return ArcState::FAILED_TO_CALL_FINDAPPS;
      case MojoState::REQUEST_HAS_INVALID_PARAMS:
        return ArcState::REQUEST_HAS_INVALID_PARAMS;
      case MojoState::REQUEST_TIMEOUT:
        return ArcState::REQUEST_TIMEOUT;
      case MojoState::PHONESKY_RESULT_REQUEST_CODE_UNMATCHED:
        return ArcState::PHONESKY_RESULT_REQUEST_CODE_UNMATCHED;
      case MojoState::PHONESKY_RESULT_SESSION_ID_UNMATCHED:
        return ArcState::PHONESKY_RESULT_SESSION_ID_UNMATCHED;
      case MojoState::PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED:
        return ArcState::PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED;
      case MojoState::PHONESKY_APP_DISCOVERY_NOT_AVAILABLE:
        return ArcState::PHONESKY_APP_DISCOVERY_NOT_AVAILABLE;
      case MojoState::PHONESKY_VERSION_NOT_SUPPORTED:
        return ArcState::PHONESKY_VERSION_NOT_SUPPORTED;
      case MojoState::PHONESKY_UNEXPECTED_EXCEPTION:
        return ArcState::PHONESKY_UNEXPECTED_EXCEPTION;
      case MojoState::PHONESKY_MALFORMED_QUERY:
        return ArcState::PHONESKY_MALFORMED_QUERY;
      case MojoState::PHONESKY_INTERNAL_ERROR:
        return ArcState::PHONESKY_INTERNAL_ERROR;
      case MojoState::PHONESKY_RESULT_INVALID_DATA:
        return ArcState::PHONESKY_RESULT_INVALID_DATA;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_MOJOM_APP_MOJOM_TRAITS_H_
