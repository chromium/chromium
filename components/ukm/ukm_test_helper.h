// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_TEST_HELPER_H_
#define COMPONENTS_UKM_UKM_TEST_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "components/ukm/ukm_service.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/metrics_proto/ukm/report.pb.h"

namespace ukm {

// Helper class that provides access to UkmService internals. This class is a
// friend of UkmService and UkmRecorderImpl.
class UkmTestHelper {
 public:
  explicit UkmTestHelper(UkmService* ukm_service);
  ~UkmTestHelper() = default;

  // Returns true if |ukm_service_| records extensions.
  bool IsExtensionRecordingEnabled() const;

  // Returns true if |ukm_service_| has recording enabled.
  bool IsRecordingEnabled() const;

  // Returns true if |ukm_service_| if the following feature is enabled:
  // kReportUserNoisedUserBirthYearAndGender.
  static bool IsReportUserNoisedUserBirthYearAndGenderEnabled();

  // Returns |ukm_service_|'s client ID.
  uint64_t GetClientId();

  // Creates and returns a UKM report if there are unsent logs from which a
  // report can be generated. Returns nullptr otherwise.
  std::unique_ptr<Report> GetUkmReport();

  // Returns the UkmSource corresponding to |source_id|, if present; otherwise,
  // returns nullptr.
  UkmSource* GetSource(SourceId source_id);

  // Returns true if |ukm_service_| has a source corresponding to |source_id|.
  bool HasSource(SourceId source_id);

  // Returns true if UkmSource denoted by |source_id| is marked as obsolete.
  bool IsSourceObsolete(SourceId source_id);

  // Adds a dummy source with |source_id| to |ukm_service_|.
  void RecordSourceForTesting(SourceId source_id);

  // Creates a log and stores it in |ukm_service_|'s UnsentLogStore.
  void BuildAndStoreLog();

  // Reeturns true if |ukm_service_| has logs to send.
  bool HasUnsentLogs();

 private:
  UkmService* const ukm_service_;
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_TEST_HELPER_H_
