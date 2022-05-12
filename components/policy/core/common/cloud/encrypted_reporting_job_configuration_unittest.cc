// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/system/fake_statistics_provider.h"
#endif

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::MockFunction;
using ::testing::NotNull;
using ::testing::ReturnPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace policy {

namespace {
constexpr int64_t kGenerationId = 4321;
constexpr ::reporting::Priority kPriority = ::reporting::Priority::IMMEDIATE;

// Default values for EncryptionInfo
constexpr char kEncryptionKeyValue[] = "abcdef";
constexpr uint64_t kPublicKeyIdValue = 9876;

// Keys for response internal dictionaries
constexpr char kLastSucceedUploadedRecordKey[] = "lastSucceedUploadedRecord";
constexpr char kFirstFailedUploadedRecordKey[] = "firstFailedUploadedRecord";

// UploadEncryptedReportingRequest list key
constexpr char kEncryptedRecordListKey[] = "encryptedRecord";

// Encryption settings request key
constexpr char kAttachEncryptionSettingsKey[] = "attachEncryptionSettings";

// Keys for EncryptedRecord
constexpr char kEncryptedWrappedRecordKey[] = "encryptedWrappedRecord";
constexpr char kSequenceInformationKey[] = "sequenceInformation";
constexpr char kEncryptionInfoKey[] = "encryptionInfo";

// Keys for internal encryption information dictionaries.
constexpr char kEncryptionKey[] = "encryptionKey";
constexpr char kPublicKeyIdKey[] = "publicKeyId";

// Keys for internal SequenceInformation dictionaries.
constexpr char kSequencingIdKey[] = "sequencingId";
constexpr char kGenerationIdKey[] = "generationId";
constexpr char kPriorityKey[] = "priority";

// Keys for FirstFailedUploadRecord values.
constexpr char kFailedUploadedRecord[] = "failedUploadedRecord";
constexpr char kFailureStatus[] = "failureStatus";

// Keys for FirstFailedUploadRecord Status dictionary
constexpr char kCodeKey[] = "code";
constexpr char kMessageKey[] = "message";

uint64_t GetNextSequenceId() {
  static uint64_t kSequencingId = 0;
  return kSequencingId++;
}

base::Value GenerateSingleRecord(base::StringPiece encrypted_wrapped_record) {
  base::Value record_dictionary{base::Value::Type::DICTIONARY};
  std::string base64_encode;
  base::Base64Encode(encrypted_wrapped_record, &base64_encode);
  record_dictionary.SetStringKey(kEncryptedWrappedRecordKey, base64_encode);

  base::Value* const sequencing_dictionary = record_dictionary.SetKey(
      kSequenceInformationKey, base::Value{base::Value::Type::DICTIONARY});
  sequencing_dictionary->SetStringKey(
      kSequencingIdKey, base::NumberToString(GetNextSequenceId()));
  sequencing_dictionary->SetStringKey(kGenerationIdKey,
                                      base::NumberToString(kGenerationId));
  sequencing_dictionary->SetIntKey(kPriorityKey, kPriority);

  base::Value* const encryption_info_dictionary = record_dictionary.SetKey(
      kEncryptionInfoKey, base::Value{base::Value::Type::DICTIONARY});
  encryption_info_dictionary->SetStringKey(kEncryptionKey, kEncryptionKeyValue);
  encryption_info_dictionary->SetStringKey(
      kPublicKeyIdKey, base::NumberToString(kPublicKeyIdValue));

  return record_dictionary;
}

class RequestPayloadBuilder {
 public:
  explicit RequestPayloadBuilder(bool attach_encryption_settings = false) {
    if (attach_encryption_settings) {
      payload_.Set(kAttachEncryptionSettingsKey, true);
    }
    payload_.Set(kEncryptedRecordListKey, base::Value::List());
  }

  RequestPayloadBuilder& AddRecord(const base::Value& record) {
    base::Value::List* records_list =
        payload_.FindList(kEncryptedRecordListKey);
    records_list->Append(record.Clone());
    return *this;
  }

  base::Value::Dict Build() { return std::move(payload_); }

 private:
  base::Value::Dict payload_;
};

class ResponseValueBuilder {
 public:
  static absl::optional<base::Value> CreateUploadFailure(
      const ::reporting::SequenceInformation& sequence_information) {
    if (!sequence_information.has_sequencing_id() ||
        !sequence_information.has_generation_id() ||
        !sequence_information.has_priority()) {
      return absl::nullopt;
    }

    base::Value upload_failure{base::Value::Type::DICTIONARY};
    upload_failure.SetKey(kFailedUploadedRecord,
                          BuildSequenceInformationValue(sequence_information));

    // Set to internal error (error::INTERNAL == 13).
    upload_failure.SetIntKey(GetFailureStatusCodePath(), 13);
    upload_failure.SetStringKey(GetFailureStatusMessagePath(),
                                "FailingForTests");
    return upload_failure;
  }

  static base::Value::Dict CreateResponse(
      const base::Value& sequence_information,
      absl::optional<base::Value> upload_failure) {
    base::Value::Dict response;

    response.Set(kLastSucceedUploadedRecordKey, sequence_information.Clone());

    if (upload_failure.has_value()) {
      response.Set(kFirstFailedUploadedRecordKey,
                   std::move(upload_failure.value()));
    }
    return response;
  }

  static std::string CreateResponseString(const base::Value::Dict& response) {
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
  static base::Value BuildSequenceInformationValue(
      const ::reporting::SequenceInformation& sequence_information) {
    base::Value sequence_information_value{base::Value::Type::DICTIONARY};
    sequence_information_value.SetIntKey(kSequencingIdKey,
                                         sequence_information.sequencing_id());
    sequence_information_value.SetIntKey(kGenerationIdKey,
                                         sequence_information.generation_id());
    sequence_information_value.SetIntKey(kPriorityKey,
                                         sequence_information.priority());
    return sequence_information_value;
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

}  // namespace

class EncryptedReportingJobConfigurationTest : public testing::Test {
 public:
  EncryptedReportingJobConfigurationTest()
      :
#if BUILDFLAG(IS_CHROMEOS_ASH)
        fake_serial_number_(&fake_statistics_provider_),
#endif
        client_(&service_) {
  }

 protected:
  using MockCompleteCb = MockFunction<void(DeviceManagementService::Job* job,
                                           DeviceManagementStatus code,
                                           int response_code,
                                           absl::optional<base::Value::Dict>)>;
  static base::Value::Dict GenerateContext(base::StringPiece key,
                                           base::StringPiece value) {
    base::Value::Dict context;
    context.SetByDottedPath(key, value);
    return context;
  }

  void GetRecordList(EncryptedReportingJobConfiguration* configuration,
                     base::Value** record_list) {
    base::Value* const payload = GetPayload(configuration);
    *record_list = payload->FindListKey(kEncryptedRecordListKey);
    ASSERT_TRUE(*record_list);
  }

  bool GetAttachEncryptionSettings(
      EncryptedReportingJobConfiguration* configuration) {
    base::Value* const payload = GetPayload(configuration);
    const auto attach_encryption_settings =
        payload->FindBoolKey(kAttachEncryptionSettingsKey);
    return attach_encryption_settings.has_value() &&
           attach_encryption_settings.value();
  }

  base::Value* GetPayload(EncryptedReportingJobConfiguration* configuration) {
    absl::optional<base::Value> payload_result =
        base::JSONReader::Read(configuration->GetPayload());

    EXPECT_TRUE(payload_result.has_value());
    payload_ = std::move(payload_result.value());
    return &payload_;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  class ScopedFakeSerialNumber {
   public:
    explicit ScopedFakeSerialNumber(
        chromeos::system::ScopedFakeStatisticsProvider*
            fake_statistics_provider) {
      // The fake serial number must be set before |configuration| is
      // constructed below.
      fake_statistics_provider->SetMachineStatistic(
          chromeos::system::kSerialNumberKeyForTest, "fake_serial_number");
    }
  };
  ScopedFakeSerialNumber fake_serial_number_;
#endif

  StrictMock<MockJobCreationHandler> job_creation_handler_;
  FakeDeviceManagementService service_{&job_creation_handler_};
  MockCloudPolicyClient client_;
  StrictMock<MockCompleteCb> complete_cb_;

  DeviceManagementService::Job job_;

 private:
  base::Value payload_;
};

// Validates that the non-Record portions of the payload are generated
// correctly.
TEST_F(EncryptedReportingJobConfigurationTest, ValidatePayload) {
  EXPECT_CALL(complete_cb_, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      &client_, service_.configuration()->GetEncryptedReportingServerUrl(),
      RequestPayloadBuilder().Build(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&complete_cb_)));
  auto* payload = GetPayload(&configuration);
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

// Ensures that records are added correctly and that the payload is Base64
// encoded.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddEncryptedRecord) {
  const std::string kEncryptedWrappedRecord = "TEST_INFO";
  base::Value record_value = GenerateSingleRecord(kEncryptedWrappedRecord);

  EXPECT_CALL(complete_cb_, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      &client_, service_.configuration()->GetEncryptedReportingServerUrl(),
      RequestPayloadBuilder().AddRecord(record_value).Build(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&complete_cb_)));

  base::Value* record_list = nullptr;
  GetRecordList(&configuration, &record_list);
  EXPECT_EQ(record_list->GetListDeprecated().size(), 1u);
  EXPECT_EQ(record_list->GetListDeprecated()[0], record_value);

  std::string* encrypted_wrapped_record =
      record_list->GetListDeprecated()[0].FindStringKey(
          kEncryptedWrappedRecordKey);
  ASSERT_THAT(encrypted_wrapped_record, NotNull());

  std::string decoded_record;
  ASSERT_TRUE(base::Base64Decode(*encrypted_wrapped_record, &decoded_record));
  EXPECT_THAT(decoded_record, StrEq(kEncryptedWrappedRecord));
}

// Ensures that multiple records can be added to the request.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddsMultipleRecords) {
  const std::vector<std::string> kEncryptedWrappedRecords{
      "T", "E", "S", "T", "_", "I", "N", "F", "O"};
  std::vector<base::Value> records;
  RequestPayloadBuilder builder;
  for (auto value : kEncryptedWrappedRecords) {
    records.push_back(GenerateSingleRecord(value));
    builder.AddRecord(records.back());
  }

  EXPECT_CALL(complete_cb_, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      &client_, service_.configuration()->GetEncryptedReportingServerUrl(),
      builder.Build(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&complete_cb_)));

  base::Value* record_list = nullptr;
  GetRecordList(&configuration, &record_list);

  EXPECT_EQ(record_list->GetListDeprecated().size(), records.size());

  size_t counter = 0;
  for (const auto& record : records) {
    EXPECT_EQ(record_list->GetListDeprecated()[counter++], record);
  }

  EXPECT_FALSE(GetAttachEncryptionSettings(&configuration));
}

// Ensures that attach encryption settings request is included when no records
// are present.
TEST_F(EncryptedReportingJobConfigurationTest,
       AllowsAttachEncryptionSettingsAlone) {
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/true};
  EXPECT_CALL(complete_cb_, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      &client_, service_.configuration()->GetEncryptedReportingServerUrl(),
      builder.Build(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&complete_cb_)));

  base::Value* record_list = nullptr;
  GetRecordList(&configuration, &record_list);

  EXPECT_TRUE(record_list->GetListDeprecated().empty());

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
}

TEST_F(EncryptedReportingJobConfigurationTest,
       CorrectlyAddsMultipleRecordsWithAttachEncryptionSettings) {
  const std::vector<std::string> kEncryptedWrappedRecords{
      "T", "E", "S", "T", "_", "I", "N", "F", "O"};
  std::vector<base::Value> records;
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/true};
  for (auto value : kEncryptedWrappedRecords) {
    records.push_back(GenerateSingleRecord(value));
    builder.AddRecord(records.back());
  }

  EXPECT_CALL(complete_cb_, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      &client_, service_.configuration()->GetEncryptedReportingServerUrl(),
      builder.Build(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&complete_cb_)));

  base::Value* record_list = nullptr;
  GetRecordList(&configuration, &record_list);

  EXPECT_EQ(record_list->GetListDeprecated().size(), records.size());

  size_t counter = 0;
  for (const auto& record : records) {
    EXPECT_EQ(record_list->GetListDeprecated()[counter++], record);
  }

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
}

// Ensures that the context can be updated.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddsAndUpdatesContext) {
  EXPECT_CALL(complete_cb_, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      &client_, service_.configuration()->GetEncryptedReportingServerUrl(),
      RequestPayloadBuilder().Build(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&complete_cb_)));

  const std::string kTestKey = "device.name";
  const std::string kTestValue = "1701-A";
  base::Value::Dict context = GenerateContext(kTestKey, kTestValue);
  configuration.UpdateContext(std::move(context));

  // Ensure the payload includes the path and value.
  base::Value* payload = GetPayload(&configuration);
  std::string* good_result = payload->FindStringPath(kTestKey);
  ASSERT_THAT(good_result, NotNull());
  EXPECT_EQ(*good_result, kTestValue);

  // Add a path that isn't in the allow list.
  const std::string kBadTestKey = "profile.string";
  context = GenerateContext(kBadTestKey, kTestValue);
  configuration.UpdateContext(std::move(context));

  // Ensure that the path is removed from the payload.
  payload = GetPayload(&configuration);
  const std::string* bad_result = payload->FindStringPath(kBadTestKey);
  EXPECT_THAT(bad_result, IsNull());

  // Ensure that adding a bad path hasn't destroyed the good path.
  good_result = payload->FindStringPath(kTestKey);
  EXPECT_THAT(good_result, NotNull());
  EXPECT_EQ(*good_result, kTestValue);

  // Ensure that a good path can be overriden.
  const std::string kUpdatedTestValue = "1701-B";
  context = GenerateContext(kTestKey, kUpdatedTestValue);
  configuration.UpdateContext(std::move(context));
  payload = GetPayload(&configuration);
  good_result = payload->FindStringPath(kTestKey);
  ASSERT_THAT(good_result, NotNull());
  EXPECT_EQ(*good_result, kUpdatedTestValue);
}

// Ensures that upload success is handled correctly.
TEST_F(EncryptedReportingJobConfigurationTest, OnURLLoadComplete_Success) {
  const std::string kEncryptedWrappedRecord = "TEST_INFO";
  base::Value record_value = GenerateSingleRecord(kEncryptedWrappedRecord);

  base::Value::Dict response = ResponseValueBuilder::CreateResponse(
      *record_value.FindDictKey(kSequenceInformationKey), absl::nullopt);

  EXPECT_CALL(complete_cb_,
              Call(&job_, DM_STATUS_SUCCESS, DeviceManagementService::kSuccess,
                   Eq(ByRef(response))))
      .Times(1);
  EncryptedReportingJobConfiguration configuration(
      &client_, service_.configuration()->GetEncryptedReportingServerUrl(),
      RequestPayloadBuilder().AddRecord(record_value).Build(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&complete_cb_)));

  const std::string kTestString = "device.clientId";
  const std::string kTestInt = "1701-A";
  base::Value::Dict context = GenerateContext(kTestString, kTestInt);
  configuration.UpdateContext(std::move(context));

  configuration.OnURLLoadComplete(
      &job_, net::OK, DeviceManagementService::kSuccess,
      ResponseValueBuilder::CreateResponseString(response));
}

// Ensures that upload failure is handled correctly.
TEST_F(EncryptedReportingJobConfigurationTest, OnURLLoadComplete_NetError) {
  EXPECT_CALL(complete_cb_, Call(&job_, DM_STATUS_REQUEST_FAILED, _,
                                 testing::Eq(absl::nullopt)))
      .Times(1);
  EncryptedReportingJobConfiguration configuration(
      &client_, service_.configuration()->GetEncryptedReportingServerUrl(),
      RequestPayloadBuilder().Build(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&complete_cb_)));
  configuration.OnURLLoadComplete(&job_, net::ERR_CONNECTION_RESET,
                                  0 /* ignored */, "");
}

}  // namespace policy
