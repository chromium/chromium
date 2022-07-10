// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/observer_list_types.h"
#include "base/supports_user_data.h"
#include "base/types/id_type.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/segmentation_platform/public/trigger.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

class PrefRegistrySimple;

namespace segmentation_platform {
struct InputContext;
class ServiceProxy;
struct SegmentSelectionResult;
class TriggerContext;

using CallbackId = base::IdType32<class OnDemandSegmentSelectionCallbackTag>;

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

  // Called to get the selected segment synchronously. If none, returns empty
  // result.
  virtual SegmentSelectionResult GetCachedSegmentResult(
      const std::string& segmentation_key) = 0;

  // Given a client and a set of inputs, runs the required models on demand and
  // returns the result in the supplied callback.
  virtual void GetSelectedSegmentOnDemand(
      const std::string& segmentation_key,
      scoped_refptr<InputContext> input_context,
      SegmentSelectionCallback callback) = 0;

  // Called to register a callback that will be invoked on segment selection
  // on-demand. Returns a callback ID that can be used for unregister.
  using OnDemandSegmentSelectionCallback =
      base::RepeatingCallback<void(const SegmentSelectionResult&,
                                   const TriggerContext&)>;
  virtual CallbackId RegisterOnDemandSegmentSelectionCallback(
      const std::string& segmentation_key,
      const OnDemandSegmentSelectionCallback& callback) = 0;

  // Called to unregister the callback with the given callback_id.
  virtual void UnregisterOnDemandSegmentSelectionCallback(
      CallbackId callback_id,
      const std::string& segmentation_key) = 0;

  // Called when a trigger event happens.
  virtual void OnTrigger(std::unique_ptr<TriggerContext> trigger_context) = 0;

  // Called to enable or disable metrics collection. Must be explicitly called
  // on startup.
  virtual void EnableMetrics(bool signal_collection_allowed) = 0;

  // Called to get the proxy that is used for debugging purpose.
  virtual ServiceProxy* GetServiceProxy();

  // Returns true when platform finished initializing, and can execute models.
  // The `GetSelectedSegment()` calls work without full platform initialization
  // since they load results from previous sessions.
  virtual bool IsPlatformInitialized() = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SEGMENTATION_PLATFORM_SERVICE_H_
