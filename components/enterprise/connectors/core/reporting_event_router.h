// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_ROUTER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_ROUTER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/enterprise/data_controls/core/browser/clipboard_context.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "components/enterprise/data_controls/core/browser/verdict.h"
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

namespace enterprise_connectors {

// An event router that collects safe browsing events and then sends
// events to reporting server.
class ReportingEventRouter : public KeyedService {
  using ReferrerChain =
      google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;
  using FrameUrlChain = google::protobuf::RepeatedPtrField<std::string>;

 public:
  explicit ReportingEventRouter(RealtimeReportingClientBase* reporting_client);

  ReportingEventRouter(const ReportingEventRouter&) = delete;
  ReportingEventRouter& operator=(const ReportingEventRouter&) = delete;
  ReportingEventRouter(ReportingEventRouter&&) = delete;
  ReportingEventRouter& operator=(ReportingEventRouter&&) = delete;

  ~ReportingEventRouter() override;

  bool IsEventEnabled(const std::string& event);

  virtual void OnLoginEvent(const GURL& url,
                            bool is_federated,
                            const url::SchemeHostPort& federated_origin,
                            const std::u16string& username);

  virtual void OnPasswordBreach(
      const std::string& trigger,
      const std::vector<std::pair<GURL, std::u16string>>& identities);

  // Notifies listeners that the user reused a protected password.
  // - `url` is the URL where the password was reused
  // - `user_name` is the user associated with the reused password
  // - `is_phising_url` is whether the URL is thought to be a phishing one
  // - `warning_shown` is whether a warning dialog was shown to the user
  void OnPasswordReuse(const GURL& url,
                       const std::string& user_name,
                       bool is_phishing_url,
                       bool warning_shown);

  // Notifies listeners that the user changed the password associated with
  // `user_name`
  void OnPasswordChanged(const std::string& user_name);

  // Notifies listeners about events related to Url Filtering Interstitials.
  // Virtual for tests.
  virtual void OnUrlFilteringInterstitial(
      const GURL& url,
      const std::string& threat_type,
      const safe_browsing::RTLookupResponse& response,
      const ReferrerChain& referrer_chain);

  // Notifies listeners that the user clicked-through a security interstitial.
  void OnSecurityInterstitialProceeded(const GURL& url,
                                       const std::string& reason,
                                       int net_error_code,
                                       const ReferrerChain& referrer_chain);

  // Notifies listeners that the user saw a security interstitial.
  void OnSecurityInterstitialShown(const GURL& url,
                                   const std::string& reason,
                                   int net_error_code,
                                   bool proceed_anyway_disabled,
                                   const ReferrerChain& referrer_chain);

  // Notifies listeners that deep scanning failed, for the given |reason|.
  void OnUnscannedFileEvent(const GURL& url,
                            const GURL& tab_url,
                            const std::string& source,
                            const std::string& destination,
                            const std::string& file_name,
                            const std::string& download_digest_sha256,
                            const std::string& mime_type,
                            const std::string& trigger,
                            const std::string& reason,
                            const std::string& content_transfer_method,
                            const int64_t content_size,
                            EventResult event_result);

  // Notifies listeners that the analysis connector detected a violation.
  void OnSensitiveDataEvent(const GURL& url,
                            const GURL& tab_url,
                            const std::string& source,
                            const std::string& destination,
                            const std::string& file_name,
                            const std::string& download_digest_sha256,
                            const std::string& mime_type,
                            const std::string& trigger,
                            const std::string& scan_id,
                            const std::string& content_transfer_method,
                            const std::string& source_email,
                            const std::string& content_area_account_email,
                            std::optional<std::u16string> user_justification,
                            const ContentAnalysisResponse::Result& result,
                            const int64_t content_size,
                            const ReferrerChain& referrer_chain,
                            const FrameUrlChain& frame_url_chain,
                            EventResult event_result);

  // Notifies listeners that safe browsing detected a dangerous download
  // - |url| is the download URL
  // - |file_name| is the path on disk
  // - |download_digest_sha256| is the hex-encoded SHA256
  // - |threat_type| is the danger type of the download.
  void OnDangerousDownloadEvent(const GURL& url,
                                const GURL& tab_url,
                                const std::string& file_name,
                                const std::string& download_digest_sha256,
                                const download::DownloadDangerType danger_type,
                                const std::string& mime_type,
                                const std::string& trigger,
                                const std::string& scan_id,
                                const int64_t content_size,
                                const ReferrerChain& referrer_chain,
                                const FrameUrlChain& frame_url_chain,
                                EventResult event_result);

  // Notifies listeners that deep scanning detected a dangerous download.
  //
  // `DangerousDownloadEvent` maps to `Malware transfer event` on the server
  // side, which means the event can be triggered from download, upload or file
  // transfer (CrOS only).
  void OnDangerousDownloadEvent(const GURL& url,
                                const GURL& tab_url,
                                const std::string& source,
                                const std::string& destination,
                                const std::string& file_name,
                                const std::string& download_digest_sha256,
                                const std::string& threat_type,
                                const std::string& mime_type,
                                const std::string& trigger,
                                const std::string& scan_id,
                                const std::string& content_transfer_method,
                                const int64_t content_size,
                                const ReferrerChain& referrer_chain,
                                const FrameUrlChain& frame_url_chain,
                                EventResult event_result);

  // Notifies listeners that the analysis connector detected a violation.
  void OnAnalysisConnectorResult(const GURL& url,
                                 const GURL& tab_url,
                                 const std::string& source,
                                 const std::string& destination,
                                 const std::string& file_name,
                                 const std::string& download_digest_sha256,
                                 const std::string& mime_type,
                                 const std::string& trigger,
                                 const std::string& scan_id,
                                 const std::string& content_transfer_method,
                                 const std::string& source_email,
                                 const std::string& content_area_account_email,
                                 const ContentAnalysisResponse::Result& result,
                                 const int64_t content_size,
                                 const ReferrerChain& referrer_chain,
                                 const FrameUrlChain& frame_url_chain,
                                 EventResult event_result);
#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  // Converts `source` into a `CopiedTextSource`. `CopiedTextSource::context` is
  // always populated, but `CopiedTextSource::url` may be left empty depending
  // on the policies that are set and broader clipboard copy context.
  //
  // This function should only be used to obtain a clipboard source for paste
  // reports and scans.
  static std::string GetClipboardSourceString(
      const enterprise_connectors::ContentMetaData::CopiedTextSource& source);

  // Helper functions to simplify calling `OnDataControlsSensitiveDataEvent`.
  virtual void ReportCopy(const data_controls::ClipboardContext& context,
                          const data_controls::Verdict& verdict);
  virtual void ReportCopyWarningBypassed(
      const data_controls::ClipboardContext& context,
      const data_controls::Verdict& verdict);
  virtual void ReportPaste(const data_controls::ClipboardContext& context,
                           const data_controls::Verdict& verdict);
  virtual void ReportPasteWarningBypassed(
      const data_controls::ClipboardContext& context,
      const data_controls::Verdict& verdict);
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

 private:
  FRIEND_TEST_ALL_PREFIXES(ReportingEventRouterTest,
                           TestOnDataControlsSensitiveDataEvent);
  FRIEND_TEST_ALL_PREFIXES(IOSReportingEventRouterTest,
                           TestOnDataControlsSensitiveDataEvent);

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  // Helper function to help bridge between public Data Controls reporting
  // methods and `OnDataControlsSensitiveDataEvent()`.
  void ReportCopyOrPaste(const data_controls::ClipboardContext& context,
                         const data_controls::Verdict& verdict,
                         const std::string& trigger,
                         enterprise_connectors::EventResult result);

  // Helper function to report sensitive data event that were caused by
  // triggering a Data Controls rule. This is similar to
  // `OnSensitiveDataEvent()` with a signature more suited to Data Controls as
  // opposed to scanning related events.
  void OnDataControlsSensitiveDataEvent(
      const GURL& url,
      const GURL& tab_url,
      const std::string& source,
      const std::string& destination,
      const std::string& mime_type,
      const std::string& trigger,
      const std::string& source_active_user_email,
      const std::string& content_area_account_email,
      const data_controls::Verdict::TriggeredRules& triggered_rules,
      EventResult event_result,
      int64_t content_size);
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

  // Returns filename with full path if full path is required;
  // Otherwise returns only the basename without full path.
  static std::string GetFileName(const std::string& filename,
                                 const bool include_full_path);

  raw_ptr<RealtimeReportingClientBase> reporting_client_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_EVENT_ROUTER_H_
