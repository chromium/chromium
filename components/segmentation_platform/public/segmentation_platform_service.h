// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/metrics/histogram_base.h"
#include "base/supports_user_data.h"
#include "base/types/id_type.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/trigger.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

class PrefRegistrySimple;

namespace segmentation_platform {
class ServiceProxy;
struct SegmentSelectionResult;

using CallbackId = base::IdType32<class OnDemandSegmentSelectionCallbackTag>;

// Structure used to store data for training output collection.
// The name should be UMA histogram name or user action name. Currently this
// output will be appended to the training data, but the histogram name is not
// recorded. So, each model can only take one type of metric. Using 2 different
// metrics for same model would make it unclear what the value means while
// training.
struct TrainingLabels {
  TrainingLabels();
  ~TrainingLabels();

  // Name and sample of the output metric to be collected as training data.
  std::optional<std::pair<std::string, base::HistogramBase::Sample>>
      output_metric;

  TrainingLabels(const TrainingLabels& other);
};

// The core class of segmentation platform that integrates all the required
// pieces on the client side.
class SegmentationPlatformService : public KeyedService,
                                    public base::SupportsUserData {
 public:
#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type SegmentationPlatformService for the given
  // SegmentationPlatformService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      SegmentationPlatformService* segmentation_platform_service);
#endif  // BUILDFLAG(IS_ANDROID)
  using SuccessCallback = base::OnceCallback<void(bool)>;

  SegmentationPlatformService() = default;
  ~SegmentationPlatformService() override = default;

  // Disallow copy/assign.
  SegmentationPlatformService(const SegmentationPlatformService&) = delete;
  SegmentationPlatformService& operator=(const SegmentationPlatformService&) =
      delete;

  // Registers preferences used by this class in the provided |registry|.  This
  // should be called for the Profile registry.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Registers preferences used by this class in the provided |registry|.  This
  // should be called for the local state registry.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  using SegmentSelectionCallback =
      base::OnceCallback<void(const SegmentSelectionResult&)>;

  // Called to get the selected segment asynchronously. If none, returns empty
  // result.
  virtual void GetSelectedSegment(const std::string& segmentation_key,
                                  SegmentSelectionCallback callback) = 0;

  // Called to get the classification results for a given client. The
  // classification config must be defined in the associated model metadata.
  // Depending on the options and client config, it either runs the associated
  // model or uses unexpired cached results.
  virtual void GetClassificationResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      ClassificationResultCallback callback) = 0;

  // Get the result from the model execution, annotated with output config to
  // interpret the results. Depending on the options and client config, it
  // either runs the associated model or uses unexpired cached results. This API
  // is experimental and does not cleanly support transitions from a heuristic
  // to ML models. This API is not usable for most ML models since ML models
  // require normalization of the output values to make them usable.
  virtual void GetAnnotatedNumericResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      AnnotatedNumericResultCallback callback) = 0;

  // Called to get the selected segment synchronously. If none, returns empty
  // result.
  virtual SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) = 0;

  // Called to trigger training data collection for a given request ID. Request
  // IDs are given when |GetClassificationResult| is called. `param` is used to
  // pass one additional output feature to be uploaded as training data. It is
  // recommended that the additional feature is also recorded as UMA histogram.
  // Optionally set `ukm_source_id` to attach the training data to the right
  // URL. The source ID should be created by the caller. If the ID is invalid,
  // the data will be uploaded with a no-URL UKM source.
  virtual void CollectTrainingData(proto::SegmentId segment_id,
                                   TrainingRequestId request_id,
                                   const TrainingLabels& param,
                                   SuccessCallback callback) = 0;
  virtual void CollectTrainingData(proto::SegmentId segment_id,
                                   TrainingRequestId request_id,
                                   ukm::SourceId ukm_source_id,
                                   const TrainingLabels& param,
                                   SuccessCallback callback) = 0;

  // Called to enable or disable metrics collection. Must be explicitly called
  // on startup.
  virtual void EnableMetrics(bool signal_collection_allowed) = 0;

  // Called to get the proxy that is used for debugging purpose.
  virtual ServiceProxy* GetServiceProxy();

  // Get access to the segmentation databases using the client.
  // WARNING: This will return nullptr till `IsPlatformInitialized()` is false.
  // You can observe ServiceProxy to get notified when platform is initialized.
  // TODO(ssid): Remove the initialization requirement by handling waiting for
  // init internally.
  // TODO(ssid): Add a Java version of this API.
  virtual DatabaseClient* GetDatabaseClient();

  // Returns true when platform finished initializing, and can execute models.
  // The `GetSelectedSegment()` calls work without full platform initialization
  // since they load results from previous sessions.
  virtual bool IsPlatformInitialized() = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_
