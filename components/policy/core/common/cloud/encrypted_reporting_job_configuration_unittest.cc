// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/system/fake_statistics_provider.h"
#endif

using testing::StrictMock;

namespace policy {

namespace {
constexpr uint64_t kGenerationId = 4321;
constexpr ::reporting::Priority kPriority = ::reporting::Priority::IMMEDIATE;

// Default values for EncryptionInfo
constexpr char kEncryptionKey[] = "abcdef";
constexpr uint64_t kPublicKeyId = 9876;

// Default context paths
constexpr char kProfileStringPath[] = "profile.string";
constexpr char kProfileIntPath[] = "profile.int";

// Keys for ResponseValueBuilder
// Keys for internal dictionaries
constexpr char kLastSucceedUploadedRecordKey[] = "lastSucceedUploadedRecord";
constexpr char kFirstFailedUploadedRecordKey[] = "firstFailedUploadedRecord";

// Keys for internal SequencingInformation dictionaries.
constexpr char kSequencingIdKey[] = "sequencingId";
constexpr char kGenerationIdKey[] = "generationId";
constexpr char kPriorityKey[] = "priority";

// Keys for FirstFailedUploadRecord values.
constexpr char kFailedUploadedRecord[] = "failedUploadedRecord";
constexpr char kFailureStatus[] = "failureStatus";

// Keys for FirstFailedUploadRecord Status dictionary
constexpr char kCodeKey[] = "code";
constexpr char kMessageKey[] = "message";

}  // namespace

class ResponseValueBuilder {
 public:
  static base::Optional<base::Value> CreateUploadFailure(
      const ::reporting::SequencingInformation& sequencing_information) {
    if (!sequencing_information.has_sequencing_id() ||
        !sequencing_information.has_generation_id() ||
        !sequencing_information.has_priority()) {
      return base::nullopt;
    }

    base::Value upload_failure{base::Value::Type::DICTIONARY};
    upload_failure.SetKey(
        kFailedUploadedRecord,
        BuildSequencingInformationValue(sequencing_information));

    // Set to internal error (error::INTERNAL == 13).
    upload_failure.SetIntKey(GetFailureStatusCodePath(), 13);
    upload_failure.SetStringKey(GetFailureStatusMessagePath(),
                                "FailingForTests");
    return upload_failure;
  }

  static base::Value CreateResponse(
      const ::reporting::SequencingInformation& sequencing_information,
      base::Optional<base::Value> upload_failure) {
    base::Value response{base::Value::Type::DICTIONARY};

    if (sequencing_information.has_sequencing_id() &&
        sequencing_information.has_generation_id() &&
        sequencing_information.has_priority()) {
      response.SetKey(kLastSucceedUploadedRecordKey,
                      BuildSequencingInformationValue(sequencing_information));
    }

    if (upload_failure.has_value()) {
      response.SetKey(kFirstFailedUploadedRecordKey,
                      std::move(upload_failure.value()));
    }
    return response;
  }

  static std::string CreateResponseString(const base::Value& response) {
    std::string response_string;
    base::JSONWriter::Write(response, &response_string);
    return response_string;
  }

  static std::string GetUploadFailureFailedUploadSequencingIdPath() {
    return GetPath(kFirstFailedUploadedRecordKey,
                   GetFailedUploadSequencingIdPath());
  }

  static std::string GetUploadFailureFailedUploadGenerationIdPath() {
    return GetPath(kFirstFailedUploadedRecordKey,
                   GetFailedUploadGenerationIdPath());
  }

  static std::string GetUploadFailureFailedUploadPriorityPath() {
    return GetPath(kFirstFailedUploadedRecordKey,
                   GetFailedUploadPriorityPath());
  }

  static std::string GetUploadFailureStatusCodePath() {
    return GetPath(kFirstFailedUploadedRecordKey, GetFailureStatusCodePath());
  }

 private:
  static base::Value BuildSequencingInformationValue(
      const ::reporting::SequencingInformation& sequencing_information) {
    base::Value sequencing_information_value{base::Value::Type::DICTIONARY};
    sequencing_information_value.SetIntKey(
        kSequencingIdKey, sequencing_information.sequencing_id());
    sequencing_information_value.SetIntKey(
        kGenerationIdKey, sequencing_information.generation_id());
    sequencing_information_value.SetIntKey(kPriorityKey,
                                           sequencing_information.priority());
    return sequencing_information_value;
  }

  static std::string GetPath(base::StringPiece base, base::StringPiece leaf) {
    return base::JoinString({base, leaf}, ".");
  }

  static std::string GetFailedUploadSequencingIdPath() {
    return GetPath(kFailedUploadedRecord, kSequencingIdKey);
  }

  static std::string GetFailedUploadGenerationIdPath() {
    return GetPath(kFailedUploadedRecord, kGenerationIdKey);
  }

  static std::string GetFailedUploadPriorityPath() {
    return GetPath(kFailedUploadedRecord, kPriorityKey);
  }

  static std::string GetFailureStatusCodePath() {
    return GetPath(kFailureStatus, kCodeKey);
  }

  static std::string GetFailureStatusMessagePath() {
    return GetPath(kFailureStatus, kMessageKey);
  }
};

class MockCallbackObserver {
 public:
  MockCallbackObserver() = default;

  MOCK_METHOD4(OnURLLoadComplete,
               void(DeviceManagementService::Job* job,
                    DeviceManagementStatus code,
                    int net_error,
                    const base::Value&));
};

class EncryptedReportingJobConfigurationTest : public testing::Test {
 public:
  EncryptedReportingJobConfigurationTest()
      : client_(&service_),
#if defined(OS_CHROMEOS)
        fake_serial_number_(&fake_statistics_provider_),
#endif
        configuration_(
            &client_,
            service_.configuration()->GetEncryptedReportingServerUrl(),
            base::BindOnce(&MockCallbackObserver::OnURLLoadComplete,
                           base::Unretained(&callback_observer_))) {
  }

 protected:
  static ::reporting::EncryptedRecord GenerateEncryptedRecord(
      const std::string& encrypted_wrapped_record) {
    ::reporting::EncryptedRecord record;
    record.set_encrypted_wrapped_record(encrypted_wrapped_record);

    auto* const sequencing_information =
        record.mutable_sequencing_information();
    sequencing_information->set_sequencing_id(GetNextSequenceId());
    sequencing_information->set_generation_id(kGenerationId);
    sequencing_information->set_priority(kPriority);

    auto* const encryption_info = record.mutable_encryption_info();
    encryption_info->set_encryption_key(kEncryptionKey);
    encryption_info->set_public_key_id(kPublicKeyId);

    return record;
  }

  static base::Optional<base::Value> GenerateEncryptedRecordDictionary(
      const std::string& encrypted_wrapped_record) {
    return EncryptedRecordDictionaryBuilder::ConvertEncryptedRecordProtoToValue(
        GenerateEncryptedRecord(encrypted_wrapped_record));
  }

  static base::Optional<base::Value> GenerateEncryptedRecordDictionary(
      const ::reporting::EncryptedRecord& record) {
    return EncryptedRecordDictionaryBuilder::ConvertEncryptedRecordProtoToValue(
        record);
  }

  static base::Value GenerateContext(base::StringPiece string, int int_val) {
    base::Value context{base::Value::Type::DICTIONARY};
    context.SetStringKey(kProfileStringPath, string);
    context.SetIntKey(kProfileIntPath, int_val);
    return context;
  }

  void GetRecordList(base::Value** record_list) {
    base::Value* payload = GetPayload();

    *record_list = payload->FindListKey(
        EncryptedReportingJobConfiguration::GetEncryptedRecordListKey());
    ASSERT_TRUE(*record_list);
  }

  base::Value* GetPayload() {
    base::Optional<base::Value> payload_result =
        base::JSONReader::Read(configuration_.GetPayload());

    EXPECT_TRUE(payload_result.has_value());
    payload_ = std::move(payload_result.value());
    return &payload_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  MockDeviceManagementService service_;
  MockCloudPolicyClient client_;
  StrictMock<MockCallbackObserver> callback_observer_;
  DeviceManagementService::Job job_;

#if defined(OS_CHROMEOS)
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  class ScopedFakeSerialNumber {
   public:
    ScopedFakeSerialNumber(chromeos::system::ScopedFakeStatisticsProvider*
                               fake_statistics_provider) {
      // The fake serial number must be set before |configuration_| is
      // constructed below.
      fake_statistics_provider->SetMachineStatistic(
          chromeos::system::kSerialNumberKeyForTest, "fake_serial_number");
    }
  };
  ScopedFakeSerialNumber fake_serial_number_;
#endif

  EncryptedReportingJobConfiguration configuration_;

 private:
  using EncryptedRecordDictionaryBuilder =
      EncryptedReportingJobConfiguration::EncryptedRecordDictionaryBuilder;

  static uint64_t GetNextSequenceId() {
    static uint64_t kSequencingId = 0;
    return kSequencingId++;
  }

  base::Value payload_;
};

// Validates that the non-Record portions of the payload are generated
// correctly.
TEST_F(EncryptedReportingJobConfigurationTest, ValidatePayload) {
  EXPECT_CALL(callback_observer_, OnURLLoadComplete).Times(1);
  auto* payload = GetPayload();

  EXPECT_FALSE(GetDeviceName().empty());
  EXPECT_EQ(
      *payload->FindStringPath(ReportingJobConfigurationBase::
                                   DeviceDictionaryBuilder::GetNamePath()),
      GetDeviceName());
  EXPECT_EQ(
      *payload->FindStringPath(ReportingJobConfigurationBase::
                                   DeviceDictionaryBuilder::GetClientIdPath()),
      client_.client_id());
  EXPECT_EQ(*payload->FindStringPath(
                ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                    GetOSPlatformPath()),
            GetOSPlatform());
  EXPECT_EQ(
      *payload->FindStringPath(ReportingJobConfigurationBase::
                                   DeviceDictionaryBuilder::GetOSVersionPath()),
      GetOSVersion());

  EXPECT_EQ(*payload->FindStringPath(
                ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                    GetMachineUserPath()),
            GetOSUsername());
  EXPECT_EQ(*payload->FindStringPath(
                ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                    GetChromeVersionPath()),
            version_info::GetVersionNumber());
}

// Ensures that records are added correctly.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddEncryptedRecord) {
  EXPECT_CALL(callback_observer_, OnURLLoadComplete).Times(1);
  const std::string kEncryptedWrappedRecord = "TEST_INFO";

  ::reporting::EncryptedRecord record =
      GenerateEncryptedRecord(kEncryptedWrappedRecord);

  configuration_.AddEncryptedRecord(record);

  base::Value* record_list = nullptr;
  GetRecordList(&record_list);

  EXPECT_EQ(record_list->GetList().size(), 1u);

  base::Optional<base::Value> record_value_result =
      GenerateEncryptedRecordDictionary(record);
  ASSERT_TRUE(record_value_result.has_value());
  EXPECT_EQ(record_list->GetList()[0], record_value_result.value());
}

// Ensures that multiple records can be added to the request.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddsMultipleRecords) {
  EXPECT_CALL(callback_observer_, OnURLLoadComplete).Times(1);
  const std::vector<std::string> kEncryptedWrappedRecords{
      "T", "E", "S", "T", "_", "I", "N", "F", "O"};

  std::vector<::reporting::EncryptedRecord> records;
  for (auto value : kEncryptedWrappedRecords) {
    records.push_back(GenerateEncryptedRecord(value));
    configuration_.AddEncryptedRecord(records.back());
  }

  base::Value* record_list = nullptr;
  GetRecordList(&record_list);

  EXPECT_EQ(record_list->GetList().size(), records.size());

  size_t counter = 0;
  for (const auto& record : records) {
    base::Optional<base::Value> record_value_result =
        GenerateEncryptedRecordDictionary(record);
    ASSERT_TRUE(record_value_result.has_value());
    EXPECT_EQ(record_list->GetList()[counter++], record_value_result.value());
  }
}

// Ensures that the context can be updated.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddsContext) {
  EXPECT_CALL(callback_observer_, OnURLLoadComplete).Times(1);
  const std::string kTestString = "Frankenstein";
  const int kTestInt = 1701;

  base::Value context = GenerateContext(kTestString, kTestInt);

  configuration_.UpdateContext(context);

  base::Value* payload = GetPayload();

  EXPECT_EQ(*payload->FindStringKey(kProfileStringPath), kTestString);
  EXPECT_EQ(*payload->FindIntKey(kProfileIntPath), kTestInt);
}

// Ensures that the last context added overrides previous values, without losing
// unchanged values.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyOverwritesContext) {
  EXPECT_CALL(callback_observer_, OnURLLoadComplete).Times(1);
  const std::string kTestString = "Frankenstein";
  const int kTestInt = 1701;

  base::Value context = GenerateContext(kTestString, kTestInt);

  configuration_.UpdateContext(context);

  const std::string kTestString2 = "Wolverine";
  base::Value context2 = GenerateContext(kTestString2, kTestInt);
  configuration_.UpdateContext(context2);

  base::Value* payload = GetPayload();

  EXPECT_EQ(*payload->FindStringKey(kProfileStringPath), kTestString2);
  EXPECT_EQ(*payload->FindIntKey(kProfileIntPath), kTestInt);
}

// Ensures that upload success is handled correctly.
TEST_F(EncryptedReportingJobConfigurationTest, OnURLLoadComplete_Success) {
  const std::string kTestString = "Frankenstein";
  const int kTestInt = 1701;

  base::Value context = GenerateContext(kTestString, kTestInt);
  configuration_.UpdateContext(context);

  const std::string kEncryptedWrappedRecord = "TEST_INFO";

  ::reporting::EncryptedRecord record =
      GenerateEncryptedRecord(kEncryptedWrappedRecord);

  configuration_.AddEncryptedRecord(record);

  base::Value response = ResponseValueBuilder::CreateResponse(
      record.sequencing_information(), base::nullopt);
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_SUCCESS, net::OK,
                                testing::Eq(testing::ByRef(response))));
  configuration_.OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kSuccess,
      ResponseValueBuilder::CreateResponseString(response));
}

// Ensures that upload failure is handled correctly.
TEST_F(EncryptedReportingJobConfigurationTest, OnURLLoadComplete_NetError) {
  base::Value empty_response;
  int net_error = net::ERR_CONNECTION_RESET;
  EXPECT_CALL(callback_observer_,
              OnURLLoadComplete(&job_, DM_STATUS_REQUEST_FAILED, net_error,
                                testing::Eq(testing::ByRef(empty_response))));
  configuration_.OnURLLoadComplete(&job_, net_error, 0, "");
}

}  // namespace policy
