// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer_impl.h"

#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_util.h"

namespace safe_browsing {

WebUIInfoSingletonEventObserver::Delegate::Delegate() = default;
WebUIInfoSingletonEventObserver::Delegate::~Delegate() = default;

WebUIInfoSingletonEventObserverImpl::WebUIInfoSingletonEventObserverImpl(
    std::unique_ptr<WebUIInfoSingletonEventObserver::Delegate> delegate)
    : delegate_(std::move(delegate)) {}
WebUIInfoSingletonEventObserverImpl::~WebUIInfoSingletonEventObserverImpl() =
    default;

void WebUIInfoSingletonEventObserverImpl::NotifyDownloadUrlCheckedJsListener(
    const std::vector<GURL>& urls,
    DownloadCheckResult result) {
  delegate_->SendEventToHandler(
      "download-url-checked-update",
      base::Value(web_ui::SerializeDownloadUrlChecked(urls, result)));
}

void WebUIInfoSingletonEventObserverImpl::NotifyClientDownloadRequestJsListener(
    ClientDownloadRequest* client_download_request) {
  delegate_->SendEventToHandler(
      "sent-client-download-requests-update",
      base::Value(
          web_ui::SerializeClientDownloadRequest(*client_download_request)));
}

void WebUIInfoSingletonEventObserverImpl::
    NotifyClientDownloadResponseJsListener(
        ClientDownloadResponse* client_download_response) {
  delegate_->SendEventToHandler(
      "received-client-download-responses-update",
      base::Value(
          web_ui::SerializeClientDownloadResponse(*client_download_response)));
}

void WebUIInfoSingletonEventObserverImpl::NotifyClientPhishingRequestJsListener(
    const web_ui::ClientPhishingRequestAndToken& client_phishing_request) {
  delegate_->SendEventToHandler(
      "sent-client-phishing-requests-update",
      base::Value(SerializeClientPhishingRequest(client_phishing_request)));
}

void WebUIInfoSingletonEventObserverImpl::
    NotifyClientPhishingResponseJsListener(
        ClientPhishingResponse* client_phishing_response) {
  delegate_->SendEventToHandler(
      "received-client-phishing-responses-update",
      base::Value(
          web_ui::SerializeClientPhishingResponse(*client_phishing_response)));
}

void WebUIInfoSingletonEventObserverImpl::NotifyCSBRRJsListener(
    ClientSafeBrowsingReportRequest* csbrr) {
  delegate_->SendEventToHandler("sent-csbrr-update",
                                base::Value(web_ui::SerializeCSBRR(*csbrr)));
}

void WebUIInfoSingletonEventObserverImpl::NotifyHitReportJsListener(
    HitReport* hit_report) {
  delegate_->SendEventToHandler(
      "sent-hit-report-list",
      base::Value(web_ui::SerializeHitReport(*hit_report)));
}

void WebUIInfoSingletonEventObserverImpl::NotifyPGEventJsListener(
    const sync_pb::UserEventSpecifics& event) {
  delegate_->SendEventToHandler("sent-pg-event",
                                web_ui::SerializePGEvent(event));
}

void WebUIInfoSingletonEventObserverImpl::NotifySecurityEventJsListener(
    const sync_pb::GaiaPasswordReuse& event) {
  delegate_->SendEventToHandler("sent-security-event",
                                web_ui::SerializeSecurityEvent(event));
}

void WebUIInfoSingletonEventObserverImpl::NotifyPGPingJsListener(
    int token,
    const web_ui::LoginReputationClientRequestAndToken& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializePGPing(request));

  delegate_->SendEventToHandler("pg-pings-update", request_list);
}

void WebUIInfoSingletonEventObserverImpl::NotifyPGResponseJsListener(
    int token,
    const LoginReputationClientResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(web_ui::SerializePGResponse(response));

  delegate_->SendEventToHandler("pg-responses-update", response_list);
}

void WebUIInfoSingletonEventObserverImpl::NotifyURTLookupPingJsListener(
    int token,
    const web_ui::URTLookupRequest& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializeURTLookupPing(request));

  delegate_->SendEventToHandler("urt-lookup-pings-update", request_list);
}

void WebUIInfoSingletonEventObserverImpl::NotifyURTLookupResponseJsListener(
    int token,
    const RTLookupResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(web_ui::SerializeURTLookupResponse(response));

  delegate_->SendEventToHandler("urt-lookup-responses-update", response_list);
}

void WebUIInfoSingletonEventObserverImpl::NotifyHPRTLookupPingJsListener(
    int token,
    const web_ui::HPRTLookupRequest& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializeHPRTLookupPing(request));

  delegate_->SendEventToHandler("hprt-lookup-pings-update", request_list);
}

void WebUIInfoSingletonEventObserverImpl::NotifyHPRTLookupResponseJsListener(
    int token,
    const V5::SearchHashesResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(web_ui::SerializeHPRTLookupResponse(response));

  delegate_->SendEventToHandler("hprt-lookup-responses-update", response_list);
}

void WebUIInfoSingletonEventObserverImpl::NotifyLogMessageJsListener(
    const base::Time& timestamp,
    const std::string& message) {
  delegate_->SendEventToHandler(
      "log-messages-update", web_ui::SerializeLogMessage(timestamp, message));
}

void WebUIInfoSingletonEventObserverImpl::NotifyReportingEventJsListener(
    const ::chrome::cros::reporting::proto::UploadEventsRequest& event,
    const base::Value::Dict& result) {
  delegate_->SendEventToHandler(
      "reporting-events-update",
      web_ui::SerializeUploadEventsRequest(event, result));
}

// TODO(crbug.com/443997643): Delete when
// UploadRealtimeReportingEventsUsingProto is cleaned up.
void WebUIInfoSingletonEventObserverImpl::NotifyReportingEventJsListener(
    const base::Value::Dict& event) {
  delegate_->SendEventToHandler("reporting-events-update",
                                web_ui::SerializeReportingEvent(event));
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
void WebUIInfoSingletonEventObserverImpl::NotifyDeepScanJsListener(
    const std::string& token,
    const web_ui::DeepScanDebugData& deep_scan_data) {
  delegate_->SendEventToHandler(
      "deep-scan-request-update",
      SerializeDeepScanDebugData(token, deep_scan_data));
}
#endif

void WebUIInfoSingletonEventObserverImpl::
    NotifyTailoredVerdictOverrideJsListener() {
  delegate_->SendEventToHandler(
      "tailored-verdict-override-update",
      delegate_->GetFormattedTailoredVerdictOverride());
}

}  // namespace safe_browsing
