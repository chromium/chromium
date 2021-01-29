// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_TEST_RAPPOR_SERVICE_H_
#define COMPONENTS_RAPPOR_TEST_RAPPOR_SERVICE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "components/prefs/testing_pref_service.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/rappor/test_log_uploader.h"

namespace rappor {

// This class duplicates the functionality of Sample, with a cohort initialized
// always to 0. It keeps a shadow object around which copies every flag field
// and string field set on the Sample.
class TestSample : public Sample {
 public:
  TestSample(RapporType type);
  ~TestSample() override;

  // Sample:
  void SetStringField(const std::string& field_name,
                      const std::string& value) override;
  void SetFlagsField(const std::string& field_name,
                     uint64_t flags,
                     size_t num_flags) override;

  void SetUInt64Field(const std::string& field_name,
                      uint64_t value,
                      NoiseLevel noise_level) override;

  struct Shadow {
    explicit Shadow(RapporType type);
    Shadow(const Shadow& other);
    ~Shadow();
    RapporType type;
    std::map<std::string, uint64_t> flag_fields;
    std::map<std::string, std::string> string_fields;
    std::map<std::string, std::pair<std::uint64_t, NoiseLevel>> uint64_fields;
  };

  Shadow GetShadow() { return shadow_; }

 private:
  Shadow shadow_;
};

// This class provides a simple instance that can be instantiated by tests
// and examined to check that metrics were recorded.  It assumes the most
// permissive settings so that any metric can be recorded.
class TestRapporServiceImpl : public RapporServiceImpl {
 public:
  TestRapporServiceImpl();

  ~TestRapporServiceImpl() override;

  // RapporServiceImpl:
  std::unique_ptr<Sample> CreateSample(RapporType type) override;
  void RecordSample(const std::string& metric_name,
                    std::unique_ptr<Sample> sample) override;
  void RecordSampleString(const std::string& metric_name,
                          RapporType type,
                          const std::string& sample) override;

  // Gets the number of reports that would be uploaded by this service.
  // This also clears the internal map of metrics as a biproduct, so if
  // comparing numbers of reports, the comparison should be from the last time
  // GetReportsCount() was called (not from the beginning of the test).
  int GetReportsCount();

  // Gets the reports proto that would be uploaded.
  // This clears the internal map of metrics.
  void GetReports(RapporReports* reports);

  // Gets the recorded sample for |metric_name|. This returns the shadow object
  // for the sample, which contains the string fields, flag fields, and type.
  // Limitation: if the metric was logged more than once, this will return the
  // latest sample that was logged.
  TestSample::Shadow* GetRecordedSampleForMetric(
      const std::string& metric_name);

  // Gets the recorded sample/type for a |metric_name|, and returns whether the
  // recorded metric was found. Limitation: if the metric was logged more than
  // once, this will return the latest sample that was logged.
  bool GetRecordedSampleForMetric(const std::string& metric_name,
                                  std::string* sample,
                                  RapporType* type);

  void set_is_incognito(bool is_incognito) { is_incognito_ = is_incognito; }

  TestingPrefServiceSimple* test_prefs() { return &test_prefs_; }

  rappor::TestLogUploader* test_uploader() { return test_uploader_; }

  base::TimeDelta next_rotation() { return next_rotation_; }

 protected:
  // Cancels the next call to OnLogInterval.
  void CancelNextLogRotation() override;

  // Schedules the next call to OnLogInterval.
  void ScheduleNextLogRotation(base::TimeDelta interval) override;

 private:
  // Used to keep track of recorded RAPPOR samples.
  struct RapporSample {
    RapporType type;
    std::string value;
  };
  typedef std::map<std::string, RapporSample> SamplesMap;
  SamplesMap samples_;

  // Recording a TestSample inserts its shadow into this map, which has all of
  // its fields copied.
  typedef std::map<std::string, TestSample::Shadow> ShadowMap;
  ShadowMap shadows_;

  TestingPrefServiceSimple test_prefs_;

  // Holds a weak ref to the uploader_ object.
  rappor::TestLogUploader* test_uploader_;

  // The last scheduled log rotation.
  base::TimeDelta next_rotation_;

  // Sets this to true to mock incognito state.
  bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(TestRapporServiceImpl);
};

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_TEST_RAPPOR_SERVICE_H_
