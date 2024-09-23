// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SERVICE_PROXY_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SERVICE_PROXY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"

namespace segmentation_platform {

using proto::SegmentId;

// A helper class to expose internals of the segmentationss service to a logging
// component and/or debug UI.
class ServiceProxy {
 public:
  // Status about a segment.
  struct SegmentStatus {
    SegmentStatus(SegmentId segment_id,
                  const std::string& segment_metadata,
                  const std::string& prediction_result,
                  base::Time prediction_timestamp,
                  bool can_execute_segment);
    SegmentId segment_id;
    std::string segment_metadata;
    std::string prediction_result;
    base::Time prediction_timestamp;
    bool can_execute_segment;
  };

  // Information about a client to the segmentation platform.
  struct ClientInfo {
    ClientInfo(const std::string& segmentation_key,
               std::optional<SegmentId> selected_segment);
    ~ClientInfo();
    ClientInfo(const ClientInfo& other);

    std::string segmentation_key;
    std::optional<SegmentId> selected_segment;
    std::vector<SegmentStatus> segment_status;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the servoice status changes.
    virtual void OnServiceStatusChanged(bool is_initialized, int status_flag) {}
    virtual void OnClientInfoAvailable(
        const std::vector<ClientInfo>& config_info) {}
  };

  virtual ~ServiceProxy() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  ServiceProxy(const ServiceProxy& other) = delete;
  ServiceProxy& operator=(const ServiceProxy& other) = delete;

  // Returns the current status of the segmentation service.
  virtual void GetServiceStatus() = 0;

  // Executes the given segment identified by |segment_id|.
  virtual void ExecuteModel(SegmentId segment_id) = 0;

  // Overwrites the result for the given segment identified by |segment_id|.
  // This will trigger a new round of segment selection and update the existing
  // result in Prefs.
  virtual void OverwriteResult(SegmentId segment_id, float result) = 0;

  // Sets the selected segment for the config identified by |segment_id|.
  virtual void SetSelectedSegment(const std::string& segmentation_key,
                                  SegmentId segment_id) = 0;

 protected:
  ServiceProxy() = default;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_SERVICE_PROXY_H_