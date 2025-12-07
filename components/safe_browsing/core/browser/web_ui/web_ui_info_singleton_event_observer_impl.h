// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_EVENT_OBSERVER_IMPL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_EVENT_OBSERVER_IMPL_H_

#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"

namespace safe_browsing {

// The implementation class for the WebUIInfoSingletonEventObserver class.
class WebUIInfoSingletonEventObserverImpl
    : public WebUIInfoSingletonEventObserver {
 public:
  explicit WebUIInfoSingletonEventObserverImpl(
      std::unique_ptr<WebUIInfoSingletonEventObserver::Delegate> delegate);
  ~WebUIInfoSingletonEventObserverImpl() override;

  // WebUIInfoSingletonEventObserver::
  void NotifyDownloadUrlCheckedJsListener(const std::vector<GURL>& gurl,
                                          DownloadCheckResult result) override;
  void NotifyClientDownloadRequestJsListener(
      ClientDownloadRequest* client_download_request) override;
  void NotifyClientDownloadResponseJsListener(
      ClientDownloadResponse* client_download_response) override;
  void NotifyClientPhishingRequestJsListener(
      const web_ui::ClientPhishingRequestAndToken& client_phishing_request)
      override;
  void NotifyClientPhishingResponseJsListener(
      ClientPhishingResponse* client_phishing_response) override;
  void NotifyCSBRRJsListener(ClientSafeBrowsingReportRequest* csbrr) override;
  void NotifyHitReportJsListener(HitReport* hit_report) override;
  void NotifyPGEventJsListener(
      const sync_pb::UserEventSpecifics& event) override;
  void NotifySecurityEventJsListener(
      const sync_pb::GaiaPasswordReuse& event) override;
  void NotifyPGPingJsListener(
      int token,
      const web_ui::LoginReputationClientRequestAndToken& request) override;
  void NotifyPGResponseJsListener(
      int token,
      const LoginReputationClientResponse& response) override;
  void NotifyURTLookupPingJsListener(
      int token,
      const web_ui::URTLookupRequest& request) override;
  void NotifyURTLookupResponseJsListener(
      int token,
      const RTLookupResponse& response) override;
  void NotifyHPRTLookupPingJsListener(
      int token,
      const web_ui::HPRTLookupRequest& request) override;
  void NotifyHPRTLookupResponseJsListener(
      int token,
      const V5::SearchHashesResponse& response) override;
  void NotifyLogMessageJsListener(const base::Time& timestamp,
                                  const std::string& message) override;
  void NotifyReportingEventJsListener(const base::Value::Dict& event) override;
  void NotifyReportingEventJsListener(
      const ::chrome::cros::reporting::proto::UploadEventsRequest& event,
      const base::Value::Dict& result) override;
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  void NotifyDeepScanJsListener(
      const std::string& token,
      const web_ui::DeepScanDebugData& request) override;
#endif
  void NotifyTailoredVerdictOverrideJsListener() override;

 private:
  std::unique_ptr<WebUIInfoSingletonEventObserver::Delegate> delegate_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_EVENT_OBSERVER_IMPL_H_
