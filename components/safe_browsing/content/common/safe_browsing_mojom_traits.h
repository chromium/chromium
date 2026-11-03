// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_SAFE_BROWSING_MOJOM_TRAITS_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_SAFE_BROWSING_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom-shared.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<safe_browsing::mojom::WebRequestProtocolType,
                  safe_browsing::ExtensionTelemetryReportRequest::SignalInfo::
                      RemoteHostContactedInfo::RemoteHostInfo::ProtocolType> {
  using RemoteHostInfo = safe_browsing::ExtensionTelemetryReportRequest::
      SignalInfo::RemoteHostContactedInfo::RemoteHostInfo;

  static safe_browsing::mojom::WebRequestProtocolType ToMojom(
      RemoteHostInfo::ProtocolType input) {
    switch (input) {
      case RemoteHostInfo::HTTP_HTTPS:
        return safe_browsing::mojom::WebRequestProtocolType::kHttpHttps;
      case RemoteHostInfo::WEBSOCKET:
        return safe_browsing::mojom::WebRequestProtocolType::kWebSocket;
      case RemoteHostInfo::UNSPECIFIED:
        // The mojom enum has no unspecified value, and senders always pass a
        // concrete protocol type.
        break;
    }
    NOTREACHED();
  }

  static RemoteHostInfo::ProtocolType FromMojom(
      safe_browsing::mojom::WebRequestProtocolType input) {
    switch (input) {
      case safe_browsing::mojom::WebRequestProtocolType::kHttpHttps:
        return RemoteHostInfo::HTTP_HTTPS;
      case safe_browsing::mojom::WebRequestProtocolType::kWebSocket:
        return RemoteHostInfo::WEBSOCKET;
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<
    safe_browsing::mojom::WebRequestContactInitiatorType,
    safe_browsing::ExtensionTelemetryReportRequest::SignalInfo::
        RemoteHostContactedInfo::RemoteHostInfo::ContactInitiator> {
  using RemoteHostInfo = safe_browsing::ExtensionTelemetryReportRequest::
      SignalInfo::RemoteHostContactedInfo::RemoteHostInfo;

  static safe_browsing::mojom::WebRequestContactInitiatorType ToMojom(
      RemoteHostInfo::ContactInitiator input) {
    switch (input) {
      case RemoteHostInfo::EXTENSION:
        return safe_browsing::mojom::WebRequestContactInitiatorType::kExtension;
      case RemoteHostInfo::CONTENT_SCRIPT:
        return safe_browsing::mojom::WebRequestContactInitiatorType::
            kContentScript;
      case RemoteHostInfo::CONTACT_INITIATOR_UNSPECIFIED:
        // The mojom enum has no unspecified value, and senders always pass a
        // concrete contact initiator type.
        break;
    }
    NOTREACHED();
  }

  static RemoteHostInfo::ContactInitiator FromMojom(
      safe_browsing::mojom::WebRequestContactInitiatorType input) {
    switch (input) {
      case safe_browsing::mojom::WebRequestContactInitiatorType::kExtension:
        return RemoteHostInfo::EXTENSION;
      case safe_browsing::mojom::WebRequestContactInitiatorType::kContentScript:
        return RemoteHostInfo::CONTENT_SCRIPT;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_COMMON_SAFE_BROWSING_MOJOM_TRAITS_H_
