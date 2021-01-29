// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/test_rappor_service.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/rappor/byte_vector_utils.h"
#include "components/rappor/proto/rappor_metric.pb.h"
#include "components/rappor/public/rappor_parameters.h"
#include "components/rappor/rappor_prefs.h"
#include "components/rappor/test_log_uploader.h"

namespace rappor {

namespace {

bool MockIsIncognito(bool* is_incognito) {
  return *is_incognito;
}

}  // namespace

TestSample::TestSample(RapporType type)
    : Sample(0, internal::kRapporParametersForType[type]), shadow_(type) {}

TestSample::~TestSample() {}

void TestSample::SetStringField(const std::string& field_name,
                                const std::string& value) {
  shadow_.string_fields[field_name] = value;
  Sample::SetStringField(field_name, value);
}

void TestSample::SetFlagsField(const std::string& field_name,
                               uint64_t flags,
                               size_t num_flags) {
  shadow_.flag_fields[field_name] = flags;
  Sample::SetFlagsField(field_name, flags, num_flags);
}

void TestSample::SetUInt64Field(const std::string& field_name,
                                uint64_t value,
                                NoiseLevel noise_level) {
  shadow_.uint64_fields[field_name] =
      std::pair<std::uint64_t, NoiseLevel>(value, noise_level);
  Sample::SetUInt64Field(field_name, value, noise_level);
}

TestSample::Shadow::Shadow(RapporType type) : type(type) {}

TestSample::Shadow::Shadow(const TestSample::Shadow& other) {
  type = other.type;
  flag_fields = other.flag_fields;
  string_fields = other.string_fields;
  uint64_fields = other.uint64_fields;
}

TestSample::Shadow::~Shadow() = default;

TestRapporServiceImpl::TestRapporServiceImpl()
    : RapporServiceImpl(&test_prefs_,
                        base::BindRepeating(&MockIsIncognito, &is_incognito_)),
      next_rotation_(base::TimeDelta()),
      is_incognito_(false) {
  RegisterPrefs(test_prefs_.registry());
  test_uploader_ = new TestLogUploader();
  InitializeInternal(base::WrapUnique(test_uploader_), 0,
                     HmacByteVectorGenerator::GenerateEntropyInput());
  Update(true, true);
}

TestRapporServiceImpl::~TestRapporServiceImpl() {}

std::unique_ptr<Sample> TestRapporServiceImpl::CreateSample(RapporType type) {
  std::unique_ptr<TestSample> test_sample(new TestSample(type));
  return std::move(test_sample);
}

// Intercepts the sample being recorded and saves it in a test structure.
void TestRapporServiceImpl::RecordSample(const std::string& metric_name,
                                         std::unique_ptr<Sample> sample) {
  TestSample* test_sample = static_cast<TestSample*>(sample.get());
  // Erase the previous sample if we logged one.
  shadows_.erase(metric_name);
  shadows_.insert(std::pair<std::string, TestSample::Shadow>(
      metric_name, test_sample->GetShadow()));
  // Original version is still called.
  RapporServiceImpl::RecordSample(metric_name, std::move(sample));
}

void TestRapporServiceImpl::RecordSampleString(const std::string& metric_name,
                                               RapporType type,
                                               const std::string& sample) {
  // Save the recorded sample to the local structure.
  RapporSample rappor_sample;
  rappor_sample.type = type;
  rappor_sample.value = sample;
  samples_[metric_name] = rappor_sample;
  // Original version is still called.
  RapporServiceImpl::RecordSampleString(metric_name, type, sample);
}

int TestRapporServiceImpl::GetReportsCount() {
  RapporReports reports;
  ExportMetrics(&reports);
  return reports.report_size();
}

void TestRapporServiceImpl::GetReports(RapporReports* reports) {
  ExportMetrics(reports);
}

TestSample::Shadow* TestRapporServiceImpl::GetRecordedSampleForMetric(
    const std::string& metric_name) {
  auto it = shadows_.find(metric_name);
  if (it == shadows_.end())
    return nullptr;
  return &it->second;
}

bool TestRapporServiceImpl::GetRecordedSampleForMetric(
    const std::string& metric_name,
    std::string* sample,
    RapporType* type) {
  auto it = samples_.find(metric_name);
  if (it == samples_.end())
    return false;
  *sample = it->second.value;
  *type = it->second.type;
  return true;
}

// Cancel the next call to OnLogInterval.
void TestRapporServiceImpl::CancelNextLogRotation() {
  next_rotation_ = base::TimeDelta();
}

// Schedule the next call to OnLogInterval.
void TestRapporServiceImpl::ScheduleNextLogRotation(base::TimeDelta interval) {
  next_rotation_ = interval;
}

}  // namespace rappor
