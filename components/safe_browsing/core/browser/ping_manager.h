// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PING_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PING_MANAGER_H_

// A class that reports basic safebrowsing statistics to Google's SafeBrowsing
// servers.
#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/files/file_util.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "components/safe_browsing/core/browser/safe_browsing_hats_delegate.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class PingManager : public KeyedService {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ReportThreatDetailsResult {
    SUCCESS = 0,
    // There was a problem serializing the report to a string.
    SERIALIZATION_ERROR = 1,
    // The report is empty, so it is not sent.
    EMPTY_REPORT = 2,
    kMaxValue = EMPTY_REPORT,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PersistThreatDetailsResult {
    // The task to persist the report has posted. The actual file write
    // operation may still fail.
    kPersistTaskPosted = 0,
    // There was a problem serializing the report to a string.
    kSerializationError = 1,
    // The report is empty, so it is not sent.
    kEmptyReport = 2,
    kMaxValue = kEmptyReport,
  };

  // Interface via which a client of this class can surface relevant events in
  // WebUI. All methods must be called on the UI thread.
  class WebUIDelegate {
   public:
    virtual ~WebUIDelegate() = default;

    // Track a client safe browsing report being sent.
    virtual void AddToCSBRRsSent(
        std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr) = 0;

    // Track a hit report being sent.
    virtual void AddToHitReportsSent(std::unique_ptr<HitReport> hit_report) = 0;
  };

  // Helper class to read/write a report on disk.
  class Persister {
   public:
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    enum class WriteResult {
      kSuccess = 0,
      kFailedCreateDirectory = 1,
      kFailedWriteFile = 2,
      kMaxValue = kFailedWriteFile,
    };

    explicit Persister(const base::FilePath& persister_root_path);
    Persister(const Persister&) = delete;
    Persister& operator=(const Persister&) = delete;

    ~Persister() = default;

    // Writes |serialized_report| to a new file in |dir_path_|.
    void WriteReport(const std::string& serialized_report);

    // Reads all persisted reports in |dir_path_|. The reports are deleted
    // regardless of whether the read was successful or not.
    // Returns a list of string representation of the reports.
    std::vector<std::string> ReadAndDeleteReports();

   private:
    // The directory that the files will be written in.
    base::FilePath dir_path_;
  };

  explicit PingManager(
      const V4ProtocolConfig& config,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      base::RepeatingCallback<bool()> get_should_fetch_access_token,
      WebUIDelegate* webui_delegate,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::RepeatingCallback<ChromeUserPopulation()>
          get_user_population_callback,
      base::RepeatingCallback<ChromeUserPopulation::PageLoadToken(GURL)>
          get_page_load_token_callback,
      std::unique_ptr<SafeBrowsingHatsDelegate> hats_delegate,
      const base::FilePath& persister_root_path,
      base::RepeatingCallback<bool()> get_should_send_persisted_report);
  PingManager(const PingManager&) = delete;
  PingManager& operator=(const PingManager&) = delete;

  ~PingManager() override;

  // Create an instance of the safe browsing ping manager.
  static std::unique_ptr<PingManager> Create(
      const V4ProtocolConfig& config,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher,
      base::RepeatingCallback<bool()> get_should_fetch_access_token,
      WebUIDelegate* webui_delegate,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::RepeatingCallback<ChromeUserPopulation()>
          get_user_population_callback,
      base::RepeatingCallback<ChromeUserPopulation::PageLoadToken(GURL)>
          get_page_load_token_callback,
      std::unique_ptr<SafeBrowsingHatsDelegate> hats_delegate,
      const base::FilePath& persister_root_path,
      base::RepeatingCallback<bool()> get_should_send_persisted_report);

  void OnURLLoaderComplete(network::SimpleURLLoader* source,
                           std::unique_ptr<std::string> response_body);
  void OnThreatDetailsReportURLLoaderComplete(
      network::SimpleURLLoader* source,
      bool has_access_token,
      std::unique_ptr<std::string> response_body);

  // Report to Google when a SafeBrowsing warning is shown to the user.
  // |hit_report.threat_type| should be one of the types known by
  // SafeBrowsingtHitUrl. This method will also sanitize the URLs in the report
  // before sending it.
  void ReportSafeBrowsingHit(
      std::unique_ptr<safe_browsing::HitReport> hit_report);

  // Sends a detailed threat report after performing validation, sanitizing
  // contained URLs, and adding extra details to the report. The returned object
  // provides details on whether the report was successful.
  virtual ReportThreatDetailsResult ReportThreatDetails(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report);

  // Similar to |ReportThreatDetails|, but persists the report on disk and sends
  // it on next startup.
  virtual PersistThreatDetailsResult PersistThreatDetailsAndReportOnNextStartup(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report);

  // Launches a survey and attaches ThreatDetails to the survey response.
  virtual void AttachThreatDetailsAndLaunchSurvey(
      std::unique_ptr<ClientSafeBrowsingReportRequest> report);

  // Only used for tests
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  void SetTokenFetcherForTesting(
      std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher);
  void SetHatsDelegateForTesting(
      std::unique_ptr<SafeBrowsingHatsDelegate> hats_delegate);

 protected:
  friend class PingManagerTest;

 private:
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestSafeBrowsingHitUrl);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestThreatDetailsUrl);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestReportThreatDetails);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestReportSafeBrowsingHit);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestSanitizeHitReport);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, TestSanitizeThreatDetailsReport);

  const V4ProtocolConfig config_;

  using Reports = std::set<std::unique_ptr<network::SimpleURLLoader>,
                           base::UniquePtrComparator>;

  // Generates URL for reporting safe browsing hits.
  GURL SafeBrowsingHitUrl(safe_browsing::HitReport* hit_report) const;

  // Generates URL for reporting threat details for users who opt-in.
  GURL ThreatDetailsUrl() const;

  // Sanitizes the URLs in the client safe browsing report.
  void SanitizeThreatDetailsReport(
      safe_browsing::ClientSafeBrowsingReportRequest* report);

  // Sanitizes the URLs in the hit report.
  void SanitizeHitReport(HitReport* hit_report);

  // Once the user's access_token has been fetched by ReportThreatDetails (or
  // intentionally not fetched), attaches the token and sends the report.
  void ReportThreatDetailsOnGotAccessToken(const std::string& serialized_report,
                                           const std::string& access_token);

  // Reads persisted reports from disk.
  void ReadPersistedReports();

  // Sends `serialized_reports` to Safe Browsing.
  void OnReadPersistedReportsDone(std::vector<std::string> serialized_reports);

  // Track outstanding SafeBrowsing report fetchers for clean up.
  // We add both "hit" and "detail" fetchers in this set.
  Reports safebrowsing_reports_;

  // Used to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The token fetcher used for getting access token.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // Determines whether it's relevant to fetch the access token for the user
  // based on whether they're a signed-in ESB user.
  base::RepeatingCallback<bool()> get_should_fetch_access_token_;

  // WebUIInfoSingleton extends PingManager::WebUIDelegate to enable the
  // workaround of calling methods in WebUIInfoSingleton without /core having a
  // dependency on /content.
  raw_ptr<WebUIDelegate> webui_delegate_;

  // The task runner for the UI thread.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // Pulls the user population.
  base::RepeatingCallback<ChromeUserPopulation()> get_user_population_callback_;

  // Pulls the page load token.
  base::RepeatingCallback<ChromeUserPopulation::PageLoadToken(GURL)>
      get_page_load_token_callback_;

  // Launches HaTS surveys.
  std::unique_ptr<SafeBrowsingHatsDelegate> hats_delegate_;

  base::SequenceBound<Persister> persister_;

  // Determines whether the user has opted in to send persisted reports.
  base::RepeatingCallback<bool()> get_should_send_persisted_report_;

  base::WeakPtrFactory<PingManager> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PING_MANAGER_H_
