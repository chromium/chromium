// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_MOJOM_APP_MOJOM_TRAITS_H_
#define COMPONENTS_ARC_MOJOM_APP_MOJOM_TRAITS_H_

#include "components/arc/app/arc_playstore_search_request_state.h"
#include "components/arc/mojom/app.mojom-shared.h"

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
    return MojoState::SUCCESS;
  }

  static bool FromMojom(MojoState input, ArcState* out) {
    switch (input) {
      case MojoState::SUCCESS:
        *out = ArcState::SUCCESS;
        return true;
      case MojoState::CANCELED:
        *out = ArcState::CANCELED;
        return true;
      case MojoState::ERROR_DEPRECATED:
        *out = ArcState::ERROR_DEPRECATED;
        return true;
      case MojoState::PLAY_STORE_PROXY_NOT_AVAILABLE:
        *out = ArcState::PLAY_STORE_PROXY_NOT_AVAILABLE;
        return true;
      case MojoState::FAILED_TO_CALL_CANCEL:
        *out = ArcState::FAILED_TO_CALL_CANCEL;
        return true;
      case MojoState::FAILED_TO_CALL_FINDAPPS:
        *out = ArcState::FAILED_TO_CALL_FINDAPPS;
        return true;
      case MojoState::REQUEST_HAS_INVALID_PARAMS:
        *out = ArcState::REQUEST_HAS_INVALID_PARAMS;
        return true;
      case MojoState::REQUEST_TIMEOUT:
        *out = ArcState::REQUEST_TIMEOUT;
        return true;
      case MojoState::PHONESKY_RESULT_REQUEST_CODE_UNMATCHED:
        return true;
        *out = ArcState::PHONESKY_RESULT_REQUEST_CODE_UNMATCHED;
        return true;
      case MojoState::PHONESKY_RESULT_SESSION_ID_UNMATCHED:
        *out = ArcState::PHONESKY_RESULT_SESSION_ID_UNMATCHED;
        return true;
      case MojoState::PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED:
        *out = ArcState::PHONESKY_REQUEST_REQUEST_CODE_UNMATCHED;
        return true;
      case MojoState::PHONESKY_APP_DISCOVERY_NOT_AVAILABLE:
        *out = ArcState::PHONESKY_APP_DISCOVERY_NOT_AVAILABLE;
        return true;
      case MojoState::PHONESKY_VERSION_NOT_SUPPORTED:
        *out = ArcState::PHONESKY_VERSION_NOT_SUPPORTED;
        return true;
      case MojoState::PHONESKY_UNEXPECTED_EXCEPTION:
        *out = ArcState::PHONESKY_UNEXPECTED_EXCEPTION;
        return true;
      case MojoState::PHONESKY_MALFORMED_QUERY:
        *out = ArcState::PHONESKY_MALFORMED_QUERY;
        return true;
      case MojoState::PHONESKY_INTERNAL_ERROR:
        *out = ArcState::PHONESKY_INTERNAL_ERROR;
        return true;
      case MojoState::PHONESKY_RESULT_INVALID_DATA:
        *out = ArcState::PHONESKY_RESULT_INVALID_DATA;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_MOJOM_APP_MOJOM_TRAITS_H_
