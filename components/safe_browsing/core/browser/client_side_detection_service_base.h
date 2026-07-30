// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_BASE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_BASE_H_

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "net/base/ip_address.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace base {
class File;
}  // namespace base

class PrefService;

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace safe_browsing {

class ClientSidePhishingModel;
enum class CSDModelType;
struct TargetEmbedding;

// Enum used to keep stats on classification using threshold comparison.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SBClientDetectionClassifyThresholdsResult {
  kSuccess = 0,
  kModelSizeMismatch = 1,
  kModelLabelNotFound = 2,
  kMaxValue = kModelLabelNotFound,
};

// Base class for ClientSideDetectionService to remove Blink dependencies.
class ClientSideDetectionServiceBase : public KeyedService {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the pref service associated with the current profile.
    virtual PrefService* GetPrefs() = 0;
    // Returns the SafeBrowsing URLLoaderFactory.
    virtual scoped_refptr<network::SharedURLLoaderFactory>
    GetSafeBrowsingURLLoaderFactory() = 0;
  };

  // void(GURL phishing_url, bool is_phishing,
  // std::optional<net::HttpStatusCode> response_code,
  // std::optional<IntelligentScanVerdict> intelligent_scan_verdict).
  typedef base::OnceCallback<void(GURL,
                                  bool,
                                  std::optional<net::HttpStatusCode>,
                                  std::optional<IntelligentScanVerdict>)>
      ClientReportPhishingRequestCallback;

  static const int kReportsIntervalDays;
  static const int kMaxReportsPerInterval;
  static const int kNegativeCacheIntervalDays;
  static const int kPositiveCacheIntervalMinutes;

  // Allows building a service instance with the given `prefs` and `opt_guide`
  // (and no Delegate). For use in testing only.
  static std::unique_ptr<ClientSideDetectionServiceBase> CreateForTesting(
      PrefService* prefs,
      optimization_guide::OptimizationGuideModelProvider* opt_guide);

  ClientSideDetectionServiceBase(
      std::unique_ptr<Delegate> delegate,
      optimization_guide::OptimizationGuideModelProvider* opt_guide);

  ClientSideDetectionServiceBase(const ClientSideDetectionServiceBase&) =
      delete;
  ClientSideDetectionServiceBase& operator=(
      const ClientSideDetectionServiceBase&) = delete;

  ~ClientSideDetectionServiceBase() override;

  // KeyedService:
  void Shutdown() override;

  // Sends a request to the SafeBrowsing servers with the ClientPhishingRequest.
  // The URL scheme of the |url()| in the request should be HTTP.  This method
  // takes ownership of the |verdict| as well as the |callback| and calls the
  // callback once the result has come back from the server or if an error
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

  // Returns the ReadOnlySharedMemoryRegion for the flatbuffer model. Virtual so
  // that mock implementation can override it.
  virtual base::ReadOnlySharedMemoryRegion GetModelSharedMemoryRegion() const;

  // Registers a callback that will be invoked whenever a new client-side
  // phishing model has been downloaded and is ready to be distributed.
  virtual base::CallbackListSubscription RegisterCallbackForModelUpdates(
      base::RepeatingClosure callback);

  // Returns true if the given IP address falls within a private
  // (unroutable) network block.  Pages which are hosted on these IP addresses
  // are exempt from client-side phishing detection.  This is called by the
  // ClientSideDetectionHost prior to sending the renderer a
  // SafeBrowsingMsg_StartPhishingDetection IPC.
  virtual bool IsPrivateIPAddress(const net::IPAddress& address) const;

  // Returns true and sets is_phishing if url is in the cache and valid.
  virtual bool GetValidCachedResult(const GURL& url, bool* is_phishing);

  // Returns true if we have sent at least kMaxReportsPerInterval phishing
  // reports in the last kReportsInterval.
  virtual bool AtPhishingReportLimit();

  // Compares the scores from classification to TFLite model thresholds.
  virtual void ClassifyPhishingThroughThresholds(
      ClientPhishingRequest* verdict);

  // Performs visual embedding classification on the verdict.
  void ClassifyThroughEmbeddings(ClientPhishingRequest* verdict);

  // Returns the URL that will be used for phishing requests.
  static GURL GetClientReportUrl(const std::string& report_url);

  // Returns true if the service is running/enabled (i.e. when Safe Browsing is
  // enabled in user preferences).
  bool IsEnabled() const;

  // Returns the model type (protobuf or flatbuffer). Virtual so that mock
  // implementation can override it.
  virtual CSDModelType GetModelType() const;

  // Returns the visual TFLite model thresholds from the model class.
  virtual const std::vector<TfLiteModelMetadata::Threshold>&
  GetVisualTfLiteModelThresholds() const;

  virtual const std::vector<TargetEmbedding>& GetTargetImageEmbeddings() const;

  // Checks whether the model class has a model available or not. Virtual so
  // that mock classes can override it.
  virtual bool IsModelAvailable() const;

  // Checks whether the model class has an image embedding model available or
  // not.
  virtual bool HasImageEmbeddingModel() const;

  virtual bool IsModelMetadataImageEmbeddingVersionMatching() const;

  // Returns the trigger model version to be used in cache for CSD-Phishing
  // debugging metadata.
  virtual int GetTriggerModelVersion() const;

  // Returns the image embedding model version to be passed onto
  // |ClientPhishingRequest| when image embedding is called.
  int GetImageEmbeddingModelVersion() const;

  bool IsSubscribedToImageEmbeddingModelUpdates() const;
  bool IsSubscribedToImageClassifierModelUpdates() const;

  // For testing the model in browser test.
  void SetModelAndVisualTfLiteForTesting(const base::FilePath& model,
                                         const base::FilePath& visual_tf_lite);
  void SetTargetImageEmbeddingsForTesting(
      std::vector<TargetEmbedding> target_embeddings);

  base::WeakPtr<ClientSideDetectionServiceBase> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  ClientSideDetectionServiceBase(
      PrefService* prefs,
      optimization_guide::OptimizationGuideModelProvider* opt_guide);

  friend class ClientSideDetectionServiceTest;
  friend class ClientSideDetectionServiceBaseTest;

  // Map of client report phishing request to the corresponding callback that
  // has to be invoked when the request is done.
  struct ClientPhishingReportInfo {
    std::unique_ptr<network::SimpleURLLoader> loader;
    ClientReportPhishingRequestCallback callback;
    GURL phishing_url;
  };

  // CacheState holds all information necessary to respond to a caller without
  // actually making a HTTP request.
  struct CacheState {
    bool is_phishing;
    base::Time timestamp;

    CacheState(bool phish, base::Time time);
  };

  static const char kClientReportPhishingUrl[];

  // Returns the TfLite model file. Virtual so that mock implementation can
  // override it.
  virtual const base::File& GetVisualTfLiteModel() const;

  // Returns the Image Embedding model file. Virtual so that mock implementation
  // can override it.
  virtual const base::File& GetImageEmbeddingModel() const;

  virtual int GetClassificationInputWidth() const;
  virtual int GetClassificationInputHeight() const;
  virtual int GetImageEmbeddingInputWidth() const;
  virtual int GetImageEmbeddingInputHeight() const;

  // Hook called by the base class after sending a phishing report. Used to log
  // the `ClientPhishingRequest` on any open chrome://safe-browsing pages.
  virtual void DidSendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> request,
      const std::string& access_token) {}

  // Hook called by the base class after receiving a phishing response. Used to
  // log the `ClientPhishingResponse` on any open chrome://safe-browsing
  // pages.
  virtual void DidReceiveClientPhishingResponse(
      const ClientPhishingResponse& response) {}

  // Called when the phishing model files are updated on disk. Subclasses should
  // override this to process the new model files.
  virtual void OnModelUpdated() {}

  // Internal helper called when either the underlying phishing model or the
  // service state (e.g., enabled/disabled preferences) changes. It manages
  // subscriptions and delegates to the platform-specific `OnModelUpdated()`
  // hook.
  void OnModelAndServiceStateChanged();

  void OnURLLoaderComplete(network::SimpleURLLoader* url_loader,
                           base::Time start_time,
                           std::optional<std::string> response_body);

  void HandlePhishingVerdict(network::SimpleURLLoader* source,
                             const GURL& url,
                             int net_error,
                             std::optional<net::HttpStatusCode> response_code,
                             const std::string& data);

  // Helper methods to allow subclasses to mutate the cache safely.
  void AddCacheEntry(const GURL& url, bool is_phishing, base::Time timestamp);
  void ClearCache();

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

  void SetEnabled(bool enabled);

  // Called when the prefs have changed in a way we may need to respond to. May
  // enable or disable the service and refresh the state of all renderers.
  // Disabling cancels any pending requests; existing ClientSideDetectionHosts
  // will have their callbacks called with "false" verdicts.  Enabling starts
  // downloading the model after a delay.  In all cases, each renderer process
  // is updated to match the state.
  void OnPrefsUpdated();

  // Unsubscribes to model subscriptions. Currently we unsubscribe to the image
  // embedding model as well as the on device model depending on user
  // preferences.
  void UnsubscribeToModelSubscription();

  Delegate* delegate() const { return delegate_.get(); }

  PrefService* prefs() const { return prefs_; }

  std::unique_ptr<ClientSidePhishingModel> client_side_phishing_model_;

  std::map<const network::SimpleURLLoader*,
           std::unique_ptr<ClientPhishingReportInfo>>
      client_phishing_reports_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<Delegate> delegate_;

 private:
  // Cache of completed requests. Used to satisfy requests for the same urls
  // as long as the next request falls within our caching window (which is
  // determined by kNegativeCacheIntervalDays and
  // kPositiveCacheIntervalMinutes). The size of this cache is limited by
  // kMaxReportsPerDay * ceil(InDays(max(kNegativeCacheIntervalDays,
  // kPositiveCacheIntervalMinutes))).
  // TODO(gcasto): Serialize this so that it doesn't reset on browser restart.
  std::map<GURL, std::unique_ptr<CacheState>> cache_;

  raw_ptr<PrefService> prefs_ = nullptr;

  // Timestamp of when we sent a phishing request. Used to limit the number
  // of phishing requests that we send in a day.
  std::deque<base::Time> phishing_report_times_;

  // Whether the service is running or not.  When the service is not running,
  // it won't download the model nor report detected phishing URLs.
  bool enabled_ = false;

  // Whether the service is in extended reporting mode or not. This affects the
  // choice of model.
  bool extended_reporting_ = false;

  // PrefChangeRegistrar used to track when the Safe Browsing pref changes.
  PrefChangeRegistrar pref_change_registrar_;

  base::CallbackListSubscription update_model_subscription_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ClientSideDetectionServiceBase> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_CLIENT_SIDE_DETECTION_SERVICE_BASE_H_
