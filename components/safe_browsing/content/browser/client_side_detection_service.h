// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing backends for
// client-side phishing detection.  This class is used to fetch the client-side
// model and send it to all renderers.  This class is also used to send a ping
// back to Google to verify if a particular site is really phishing or not.
//
// This class is not thread-safe and expects all calls to be made on the UI
// thread.  We also expect that the calling thread runs a message loop.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "net/base/ip_address.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class ClientPhishingRequest;
class ClientSideDetectionHost;

// Enum used to keep stats on classification using threshold comparison.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SBClientDetectionClassifyThresholdsResult {
  kSuccess = 0,
  kModelSizeMismatch = 1,
  kModelLabelNotFound = 2,
  kMaxValue = kModelLabelNotFound,
};

// Main service which pushes models to the renderers, responds to classification
// requests. This owns two ModelLoader objects.
class ClientSideDetectionService
    : public KeyedService,
      public content::RenderProcessHostCreationObserver,
      public content::RenderProcessHostObserver {
 public:
  // void(GURL phishing_url, bool is_phishing,
  // std::optional<net::HttpStatusCode> response_code).
  typedef base::OnceCallback<
      void(GURL, bool, std::optional<net::HttpStatusCode>)>
      ClientReportPhishingRequestCallback;

  // Delegate which allows to provide embedder specific implementations.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the pref service associated with the current profile.
    virtual PrefService* GetPrefs() = 0;
    // Returns the main URLLoaderFactory.
    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetURLLoaderFactory() = 0;
    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetSafeBrowsingURLLoaderFactory() = 0;
    virtual bool ShouldSendModelToBrowserContext(
        content::BrowserContext* context) = 0;
  };

  ClientSideDetectionService(
      std::unique_ptr<Delegate> delegate,
      optimization_guide::OptimizationGuideModelProvider* opt_guide);

  ClientSideDetectionService(const ClientSideDetectionService&) = delete;
  ClientSideDetectionService& operator=(const ClientSideDetectionService&) =
      delete;

  ~ClientSideDetectionService() override;

  void Shutdown() override;

  bool enabled() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return enabled_;
  }

  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           base::Time start_time,
                           std::unique_ptr<std::string> response_body);

  // Sends a request to the SafeBrowsing servers with the ClientPhishingRequest.
  // The URL scheme of the |url()| in the request should be HTTP.  This method
  // takes ownership of the |verdict| as well as the |callback| and calls the
  // the callback once the result has come back from the server or if an error
  // occurs during the fetch.  If the service is disabled or an error occurs the
  // phishing verdict will always be false.  The callback is always called after
  // SendClientReportPhishingRequest() returns and on the same thread as
  // SendClientReportPhishingRequest() was called.  You may set |callback| to
  // NULL if you don't care about the server verdict.  If |access_token| is not
  // empty, it is set in the "Authorization: Bearer" header.
  virtual void SendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> verdict,
      ClientReportPhishingRequestCallback callback,
      const std::string& access_token);

  // Returns true if the given IP address falls within a private
  // (unroutable) network block.  Pages which are hosted on these IP addresses
  // are exempt from client-side phishing detection.  This is called by the
  // ClientSideDetectionHost prior to sending the renderer a
  // SafeBrowsingMsg_StartPhishingDetection IPC.
  virtual bool IsPrivateIPAddress(const net::IPAddress& address) const;

  // Returns true if the given IP address does not refer to remote content. For
  // example, local files and chrome:// pages will create navigations that
  // return true.
  virtual bool IsLocalResource(const net::IPAddress& address) const;

  // Returns true and sets is_phishing if url is in the cache and valid.
  virtual bool GetValidCachedResult(const GURL& url, bool* is_phishing);

  // Returns true if we have sent at least kMaxReportsPerInterval phishing
  // reports in the last kReportsInterval.
  virtual bool AtPhishingReportLimit();

  // Sends a model to each renderer.
  virtual void SendModelToRenderers();

  // Returns the model type (protobuf or flatbuffer). Virtual so that mock
  // implementation can override it.
  virtual CSDModelType GetModelType();

  // Returns the ReadOnlySharedMemoryRegion for the flatbuffer model. Virtual so
  // that mock implementation can override it.
  virtual base::ReadOnlySharedMemoryRegion GetModelSharedMemoryRegion();

  // Returns the TfLite model file. Virtual so that mock implementation can
  // override it.
  virtual const base::File& GetVisualTfLiteModel();

  // Returns the Image Embedding model file. Virtual so that mock implementation
  // can override it.
  virtual const base::File& GetImageEmbeddingModel();

  virtual bool IsModelMetadataImageEmbeddingVersionMatching();

  // Returns the visual TFLite model thresholds from the model class
  virtual const base::flat_map<std::string, TfLiteModelMetadata::Threshold>&
  GetVisualTfLiteModelThresholds();

  // Compare the scores from classification to TFLite model thresholds
  void ClassifyPhishingThroughThresholds(ClientPhishingRequest* verdict);

  // Overrides the SharedURLLoaderFactory
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Sends a model to each renderer.
  void SetPhishingModel(content::RenderProcessHost* rph,
                        bool new_renderer_process_host);

  // Returns a WeakPtr for this service.
  base::WeakPtr<ClientSideDetectionService> GetWeakPtr();

  // Checks whether the model class has a model available or not. Virtual so
  // that mock classes can override it.
  virtual bool IsModelAvailable();

  // Checks whether the model class has an image embedding model available or
  // not.
  bool HasImageEmbeddingModel();

  // For testing the model in browser test.
  void SetModelAndVisualTfLiteForTesting(const base::FilePath& model,
                                         const base::FilePath& visual_tf_lite);

  bool IsSubscribedToImageEmbeddingModelUpdates();

  base::CallbackListSubscription RegisterCallbackForModelUpdates(
      base::RepeatingClosure callback);

  // Returns the trigger model version to be used in cache for CSD-Phishing
  // debugging metadata.
  int GetTriggerModelVersion();

 private:
  friend class ClientSideDetectionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           SetEnabledAndRefreshState);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           ServiceObjectDeletedBeforeCallbackDone);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           SendClientReportPhishingRequest);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           GetNumReportTestWhenPrefsPreloaded);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest, GetNumReportTest);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest, GetNumReportTestESB);
  FRIEND_TEST_ALL_PREFIXES(ClientSideDetectionServiceTest,
                           TestModelFollowsPrefs);

  // CacheState holds all information necessary to respond to a caller without
  // actually making a HTTP request.
  struct CacheState {
    bool is_phishing;
    base::Time timestamp;

    CacheState(bool phish, base::Time time);
  };

  static const char kClientReportPhishingUrl[];
  static const int kMaxReportsPerInterval;
  static const int kReportsIntervalDays;
  static const int kNegativeCacheIntervalDays;
  static const int kPositiveCacheIntervalMinutes;

  // Called when the prefs have changed in a way we may need to respond to. May
  // enable or disable the service and refresh the state of all renderers.
  // Disabling cancels any pending requests; existing ClientSideDetectionHosts
  // will have their callbacks called with "false" verdicts.  Enabling starts
  // downloading the model after a delay.  In all cases, each render process is
  // updated to match the state
  void OnPrefsUpdated();

  // Starts sending the request to the client-side detection frontends.
  // This method takes ownership of both pointers.
  void StartClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> request,
      ClientReportPhishingRequestCallback callback,
      const std::string& access_token);

  // Called by OnURLFetchComplete to handle the server response from
  // sending the client-side phishing request.
  void HandlePhishingVerdict(network::SimpleURLLoader* source,
                             const GURL& url,
                             int net_error,
                             std::optional<net::HttpStatusCode> response_code,
                             const std::string& data);

  // Invalidate cache results which are no longer useful.
  void UpdateCache();

  // Get the number of phishing reports that we have sent over kReportsInterval.
  int GetPhishingNumReports();

  // Returns true if we can successfully add a phishing report to
  // |phishing_report_times_| and stores the result in prefs. Returns false if
  // we're at the ping limit or prefs is null.
  bool AddPhishingReport(base::Time timestamp);

  // Populates |phishing_report_times_| with the data stored in local prefs.
  void LoadPhishingReportTimesFromPrefs();

  // Returns the URL that will be used for phishing requests.
  static GURL GetClientReportUrl(const std::string& report_url);

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* rph) override;

  //  content::RenderProcessHostObserver
  void RenderProcessHostDestroyed(content::RenderProcessHost* rph) override;
  void RenderProcessReady(content::RenderProcessHost* rph) override;

  // Whether the service is running or not.  When the service is not running,
  // it won't download the model nor report detected phishing URLs.
  bool enabled_ = false;

  // Whether the service is in extended reporting mode or not. This affects the
  // choice of model.
  bool extended_reporting_ = false;

  // Whether the trigger models have been sent or not. This is used to determine
  // whether an empty model in the model class determines whether the models
  // haven't been sent or we should clear the models in the scorer because they
  // have been sent.
  bool sent_trigger_models_ = false;

  // This is to keep track of the trigger model version that was last sent to
  // the renderer host processes. This is used to determine, when the image
  // embedding model arrives, whether a new scorer should be made with all
  // models or the image embedding model can be attached to the current scorer.
  // This is also used to add to CSD-Phishing debugging metadata to PhishGuard
  // pings.
  int trigger_model_version_ = 0;

  // Map of client report phishing request to the corresponding callback that
  // has to be invoked when the request is done.
  struct ClientPhishingReportInfo;
  std::map<const network::SimpleURLLoader*,
           std::unique_ptr<ClientPhishingReportInfo>>
      client_phishing_reports_;

  // Cache of completed requests. Used to satisfy requests for the same urls
  // as long as the next request falls within our caching window (which is
  // determined by kNegativeCacheInterval and kPositiveCacheInterval). The
  // size of this cache is limited by kMaxReportsPerDay *
  // ceil(InDays(max(kNegativeCacheInterval, kPositiveCacheInterval))).
  // TODO(gcasto): Serialize this so that it doesn't reset on browser restart.
  std::map<GURL, std::unique_ptr<CacheState>> cache_;

  // Timestamp of when we sent a phishing request. Used to limit the number
  // of phishing requests that we send in a day.
  std::deque<base::Time> phishing_report_times_;

  // The URLLoaderFactory we use to issue network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // PrefChangeRegistrar used to track when the Safe Browsing pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  std::unique_ptr<Delegate> delegate_;

  base::CallbackListSubscription update_model_subscription_;

  std::unique_ptr<ClientSidePhishingModel> client_side_phishing_model_;
  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      observed_render_process_hosts_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to asynchronously call the callbacks for
  // SendClientReportPhishingRequest.
  base::WeakPtrFactory<ClientSideDetectionService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_H_
