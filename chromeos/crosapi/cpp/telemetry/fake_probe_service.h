// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_TELEMETRY_FAKE_PROBE_SERVICE_H_
#define CHROMEOS_CROSAPI_CPP_TELEMETRY_FAKE_PROBE_SERVICE_H_

#include <vector>

#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

class FakeProbeService : public crosapi::mojom::TelemetryProbeService {
 public:
  FakeProbeService();
  FakeProbeService(const FakeProbeService&) = delete;
  FakeProbeService& operator=(const FakeProbeService&) = delete;
  ~FakeProbeService() override;

  void BindPendingReceiver(
      mojo::PendingReceiver<crosapi::mojom::TelemetryProbeService> receiver);

  mojo::PendingRemote<crosapi::mojom::TelemetryProbeService>
  BindNewPipeAndPassRemote();

  // crosapi::mojom::TelemetryProbeService overrides.
  void ProbeTelemetryInfo(
      const std::vector<crosapi::mojom::ProbeCategoryEnum>& categories,
      ProbeTelemetryInfoCallback callback) override;

  void GetOemData(GetOemDataCallback callback) override;

  // Sets the return value for |ProbeTelemetryInfo|.
  void SetProbeTelemetryInfoResponse(
      crosapi::mojom::ProbeTelemetryInfoPtr response_info);

  // Sets the return value for |GetOemData|.
  void SetOemDataResponse(crosapi::mojom::ProbeOemDataPtr oem_data);

  const std::vector<crosapi::mojom::ProbeCategoryEnum>&
  GetLastRequestedCategories();

  int GetProbeTelemetryInfoCallCount();

 private:
  mojo::Receiver<crosapi::mojom::TelemetryProbeService> receiver_;

  // Response for a call to |ProbeTelemetryInfo|.
  crosapi::mojom::ProbeTelemetryInfoPtr telem_info_{
      crosapi::mojom::ProbeTelemetryInfo::New()};

  // Response for a call to |GetOemData|.
  crosapi::mojom::ProbeOemDataPtr oem_data_{
      crosapi::mojom::ProbeOemData::New()};

  std::vector<crosapi::mojom::ProbeCategoryEnum>
      probe_telemetry_info_requested_categories_;
  int probe_telemetry_info_call_count_ = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_CROSAPI_CPP_TELEMETRY_FAKE_PROBE_SERVICE_H_
