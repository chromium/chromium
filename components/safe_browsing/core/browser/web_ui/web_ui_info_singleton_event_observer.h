// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_EVENT_OBSERVER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_EVENT_OBSERVER_H_

#include <memory>

#include "base/time/time.h"
#include "base/values.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "url/gurl.h"

namespace safe_browsing {
struct HitReport;
namespace web_ui {
struct ClientPhishingRequestAndToken;
struct DeepScanDebugData;
struct HPRTLookupRequest;
struct LoginReputationClientRequestAndToken;
struct URTLookupRequest;
}  // namespace web_ui

// The observer class that is notified when a change occurs within the
// WebUIInfoSingleton object and forwards the change onto the
// SafeBrowsingUIHandler via the Delegate object.
class WebUIInfoSingletonEventObserver {
 public:
  class Delegate {
   public:
    explicit Delegate();
    virtual ~Delegate();

    virtual base::Value::Dict GetFormattedTailoredVerdictOverride() = 0;

    virtual void SendEventToHandler(std::string_view event_name,
                                    base::Value value) = 0;
    virtual void SendEventToHandler(std::string_view event_name,
                                    base::Value::List& list) = 0;
    virtual void SendEventToHandler(std::string_view event_name,
                                    base::Value ::Dict dict) = 0;
  };

  virtual ~WebUIInfoSingletonEventObserver() = default;

  // Called when a new download URL is checked while one or more WebUI tabs are
  // open.
  virtual void NotifyDownloadUrlCheckedJsListener(
      const std::vector<GURL>& gurl,
      DownloadCheckResult result) = 0;

  // Called when any new ClientDownloadRequest messages are sent while one or
  // more WebUI tabs are open.
  virtual void NotifyClientDownloadRequestJsListener(
      ClientDownloadRequest* client_download_request) = 0;

  // Called when any new ClientDownloadResponse messages are received while one
  // or more WebUI tabs are open.
  virtual void NotifyClientDownloadResponseJsListener(
      ClientDownloadResponse* client_download_response) = 0;

  // Called when any new ClientPhishingRequest messages are sent (potentially
  // with token in header) while one or more WebUI tabs are open.
  virtual void NotifyClientPhishingRequestJsListener(
      const web_ui::ClientPhishingRequestAndToken& client_phishing_request) = 0;

  // Called when any new ClientPhishingResponse messages are received while one
  // or more WebUI tabs are open.
  virtual void NotifyClientPhishingResponseJsListener(
      ClientPhishingResponse* client_phishing_response) = 0;

  // Get the new ThreatDetails messages sent from ThreatDetails when a ping is
  // sent, while one or more WebUI tabs are opened.
  virtual void NotifyCSBRRJsListener(
      ClientSafeBrowsingReportRequest* csbrr) = 0;

  // Get the new HitReport messages sent from PingManager when a ping is
  // sent, while one or more WebUI tabs are opened.
  virtual void NotifyHitReportJsListener(HitReport* hit_report) = 0;

  // Called when any new PhishGuard events are sent while one or more WebUI tabs
  // are open.
  virtual void NotifyPGEventJsListener(
      const sync_pb::UserEventSpecifics& event) = 0;

  // Called when any new Security events are sent while one or more WebUI tabs
  // are open.
  virtual void NotifySecurityEventJsListener(
      const sync_pb::GaiaPasswordReuse& event) = 0;

  // Called when any new PhishGuard pings are sent while one or more WebUI tabs
  // are open.
  virtual void NotifyPGPingJsListener(
      int token,
      const web_ui::LoginReputationClientRequestAndToken& request) = 0;

  // Called when any new PhishGuard responses are received while one or more
  // WebUI tabs are open.
  virtual void NotifyPGResponseJsListener(
      int token,
      const LoginReputationClientResponse& response) = 0;

  // Called when any new URL real time lookup pings are sent while one or more
  // WebUI tabs are open.
  virtual void NotifyURTLookupPingJsListener(
      int token,
      const web_ui::URTLookupRequest& request) = 0;

  // Called when any new URL real time lookup responses are received while one
  // or more WebUI tabs are open.
  virtual void NotifyURTLookupResponseJsListener(
      int token,
      const RTLookupResponse& response) = 0;

  // Called when any new hash-prefix real-time lookup pings are sent while one
  // or more WebUI tabs are open.
  virtual void NotifyHPRTLookupPingJsListener(
      int token,
      const web_ui::HPRTLookupRequest& request) = 0;

  // Called when any new hash-prefix real-time lookup responses are received
  // while one or more WebUI tabs are open.
  virtual void NotifyHPRTLookupResponseJsListener(
      int token,
      const V5::SearchHashesResponse& response) = 0;

  // Called when any new log messages are received while one or more WebUI tabs
  // are open.
  virtual void NotifyLogMessageJsListener(const base::Time& timestamp,
                                          const std::string& message) = 0;

  // Called when any new reporting events are sent while one or more WebUI tabs
  // are open.
  virtual void NotifyReportingEventJsListener(
      const base::Value::Dict& event) = 0;
  virtual void NotifyReportingEventJsListener(
      const ::chrome::cros::reporting::proto::UploadEventsRequest& event,
      const base::Value::Dict& result) = 0;

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  // Called when any deep scans are updated while one or more WebUI
  // tabs are open.
  virtual void NotifyDeepScanJsListener(
      const std::string& token,
      const web_ui::DeepScanDebugData& request) = 0;
#endif

  // Notifies the WebUI instance that a change in tailored verdict override
  // occurred if the change did not originate from the instance.
  virtual void NotifyTailoredVerdictOverrideJsListener() = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_WEB_UI_INFO_SINGLETON_EVENT_OBSERVER_H_
