// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

struct ClientSidePhishingModelSingletonTrait;

enum class CSDModelType { kNone = 0, kProtobuf = 1, kFlatbuffer = 2 };

// This holds the currently active client side phishing detection model.
//
// The data to populate it is fetched periodically from Google to get the most
// up-to-date model. We assume it is updated at most every few hours.
//
// This class is not thread safe. In particular GetModelStr() returns a
// string reference, which assumes the string won't be used and updated
// at the same time.

class ClientSidePhishingModel {
 public:
  virtual ~ClientSidePhishingModel();

  static ClientSidePhishingModel* GetInstance();  // Singleton

  // Register a callback to be notified whenever the model changes. All
  // notifications will occur on the UI thread.
  base::CallbackListSubscription RegisterCallback(
      base::RepeatingCallback<void()> callback);

  // Returns whether we currently have a model.
  bool IsEnabled() const;

  // Returns model type (protobuf or flatbuffer).
  CSDModelType GetModelType() const;

  // Returns the model string, as a serialized protobuf or flatbuffer.
  const std::string& GetModelStr() const;

  // Returns the shared memory region for the flatbuffer.
  base::ReadOnlySharedMemoryRegion GetModelSharedMemoryRegion() const;

  // Updates the internal model string, when one is received from a component
  // update.
  void PopulateFromDynamicUpdate(const std::string& model_str,
                                 base::File visual_tflite_model);

  const base::File& GetVisualTfLiteModel() const;

  // Overrides the model string for use in tests.
  void SetModelStrForTesting(const std::string& model_str);
  void SetVisualTfLiteModelForTesting(base::File file);
  // Overrides model type.
  void SetModelTypeForTesting(CSDModelType model_type);
  // Removes mapping.
  void ClearMappedRegionForTesting();
  // Get flatbuffer memory address.
  void* GetFlatBufferMemoryAddressForTesting();
  // Notifies all the callbacks of a change in model.
  void NotifyCallbacksOfUpdateForTesting();

  const base::flat_map<std::string, TfLiteModelMetadata::Threshold>&
  GetVisualTfLiteModelThresholds() const;

  // Called to check the command line and maybe override the current model.
  void MaybeOverrideModel();

 private:
  static const int kInitialClientModelFetchDelayMs;

  ClientSidePhishingModel();

  void NotifyCallbacksOnUI();

  // Callback when the local file overriding the model has been read.
  void OnGetOverridenModelData(
      CSDModelType model_type,
      std::pair<std::string, base::File> model_and_tflite);

  // The list of callbacks to notify when a new model is ready. Protected by
  // lock_. Will always be notified on the UI thread.
  base::RepeatingCallbackList<void()> callbacks_;

  // Model protobuf string. Protected by lock_.
  std::string model_str_;

  // Visual TFLite model file. Protected by lock_.
  base::File visual_tflite_model_;

  // Thresholds in visual TFLite model file to be used for comparison after
  // visual classification
  base::flat_map<std::string, TfLiteModelMetadata::Threshold> thresholds_;

  // Model type as inferred by feature flag. Protected by lock_.
  CSDModelType model_type_ = CSDModelType::kNone;

  // MappedReadOnlyRegion where the flatbuffer has been copied to. Protected by
  // lock_.
  base::MappedReadOnlyRegion mapped_region_ = base::MappedReadOnlyRegion();

  mutable base::Lock lock_;

  friend struct ClientSidePhishingModelSingletonTrait;
  FRIEND_TEST_ALL_PREFIXES(ClientSidePhishingModelTest, CanOverrideWithFlag);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_CLIENT_SIDE_PHISHING_MODEL_H_
