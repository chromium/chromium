// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_PROTO_CONVERSIONS_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_PROTO_CONVERSIONS_H_

#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/proto/entry.pb.h"
#include "components/download/internal/background_service/proto/request.pb.h"
#include "components/download/internal/background_service/proto/scheduling.pb.h"

namespace download {

class ProtoConversions {
 public:
  static Entry EntryFromProto(const protodb::Entry& proto);

  static protodb::Entry EntryToProto(const Entry& entry);

  static std::unique_ptr<std::vector<Entry>> EntryVectorFromProto(
      std::unique_ptr<std::vector<protodb::Entry>> proto);

  static std::unique_ptr<std::vector<protodb::Entry>> EntryVectorToProto(
      std::unique_ptr<std::vector<Entry>> entries);

 protected:
  static protodb::Entry_State RequestStateToProto(Entry::State state);
  static Entry::State RequestStateFromProto(protodb::Entry_State state);

  static protodb::DownloadClient DownloadClientToProto(DownloadClient client);
  static DownloadClient DownloadClientFromProto(protodb::DownloadClient client);

  static SchedulingParams::NetworkRequirements NetworkRequirementsFromProto(
      protodb::SchedulingParams_NetworkRequirements network_requirements);
  static protodb::SchedulingParams_NetworkRequirements
  NetworkRequirementsToProto(
      SchedulingParams::NetworkRequirements network_requirements);

  static SchedulingParams::BatteryRequirements BatteryRequirementsFromProto(
      protodb::SchedulingParams_BatteryRequirements battery_requirements);
  static protodb::SchedulingParams_BatteryRequirements
  BatteryRequirementsToProto(
      SchedulingParams::BatteryRequirements battery_requirements);

  static SchedulingParams::Priority SchedulingPriorityFromProto(
      protodb::SchedulingParams_Priority priority);
  static protodb::SchedulingParams_Priority SchedulingPriorityToProto(
      SchedulingParams::Priority priority);

  static SchedulingParams SchedulingParamsFromProto(
      const protodb::SchedulingParams& proto);
  static void SchedulingParamsToProto(const SchedulingParams& scheduling_params,
                                      protodb::SchedulingParams* proto);

  static RequestParams RequestParamsFromProto(
      const protodb::RequestParams& proto);
  static void RequestParamsToProto(const RequestParams& request_params,
                                   protodb::RequestParams* proto);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_PROTO_CONVERSIONS_H_
