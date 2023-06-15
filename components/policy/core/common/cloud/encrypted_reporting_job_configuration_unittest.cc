// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include <cstddef>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Ge;
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

constexpr char kDmToken[] = "fake-dm-token";
constexpr char kClientId[] = "fake-client-id";
constexpr char kServerUrl[] = "https://example.com/reporting";

uint64_t GetNextSequenceId() {
  static uint64_t kSequencingId = 0;
  return kSequencingId++;
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
  static base::Value::Dict CreateResponse(
      const base::Value::Dict& sequence_information,
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
      : fake_serial_number_(&fake_statistics_provider_)
#endif
  {
  }

 protected:
  using MockCompleteCb = MockFunction<void(DeviceManagementService::Job* upload,
                                           DeviceManagementStatus code,
                                           int response_code,
                                           absl::optional<base::Value::Dict>)>;
  struct TestUpload {
    std::unique_ptr<EncryptedReportingJobConfiguration> configuration;
    std::unique_ptr<StrictMock<MockCompleteCb>> completion_cb;
    base::Value::Dict response;
    DeviceManagementService::Job job;
  };

  void SetUp() override {
    EncryptedReportingJobConfiguration::ResetUploadsStateForTest();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    // Setup the cloud policy client with a default dm token and client id.
    client_.SetDMToken(kDmToken);
    client_.client_id_ = kClientId;
  }

  TestUpload CreateTestUpload(const base::Value& record_value) {
    TestUpload test_upload;
    test_upload.response = ResponseValueBuilder::CreateResponse(
        *record_value.GetDict().FindDict(kSequenceInformationKey),
        absl::nullopt);
    test_upload.completion_cb = std::make_unique<StrictMock<MockCompleteCb>>();
    test_upload.configuration =
        std::make_unique<EncryptedReportingJobConfiguration>(
            shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
            kServerUrl, RequestPayloadBuilder().AddRecord(record_value).Build(),
            &client_,
            base::BindOnce(&MockCompleteCb::Call,
                           base::Unretained(test_upload.completion_cb.get())));
    return test_upload;
  }

  base::Value GenerateSingleRecord(base::StringPiece encrypted_wrapped_record,
                                   ::reporting::Priority priority = kPriority) {
    base::Value::Dict record_dictionary;
    std::string base64_encode;
    base::Base64Encode(encrypted_wrapped_record, &base64_encode);
    record_dictionary.Set(kEncryptedWrappedRecordKey, base64_encode);

    base::Value::Dict* const sequencing_dictionary =
        record_dictionary.EnsureDict(kSequenceInformationKey);
    sequencing_dictionary->Set(kSequencingIdKey,
                               base::NumberToString(GetNextSequenceId()));
    sequencing_dictionary->Set(kGenerationIdKey,
                               base::NumberToString(kGenerationId));
    sequencing_dictionary->Set(kPriorityKey, priority);

    base::Value::Dict* const encryption_info_dictionary =
        record_dictionary.EnsureDict(kEncryptionInfoKey);
    encryption_info_dictionary->Set(kEncryptionKey, kEncryptionKeyValue);
    encryption_info_dictionary->Set(kPublicKeyIdKey,
                                    base::NumberToString(kPublicKeyIdValue));

    return base::Value(std::move(record_dictionary));
  }

  static base::Value::Dict GenerateContext(base::StringPiece key,
                                           base::StringPiece value) {
    base::Value::Dict context;
    context.SetByDottedPath(key, value);
    return context;
  }

  void GetRecordList(EncryptedReportingJobConfiguration* configuration,
                     base::Value::List** record_list) {
    base::Value* const payload = GetPayload(configuration);
    *record_list = payload->GetDict().FindList(kEncryptedRecordListKey);
    ASSERT_TRUE(*record_list);
  }

  bool GetAttachEncryptionSettings(
      EncryptedReportingJobConfiguration* configuration) {
    base::Value* const payload = GetPayload(configuration);
    const auto attach_encryption_settings =
        payload->GetDict().FindBool(kAttachEncryptionSettingsKey);
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

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  policy::MockCloudPolicyClient client_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  class ScopedFakeSerialNumber {
   public:
    explicit ScopedFakeSerialNumber(
        ash::system::ScopedFakeStatisticsProvider* fake_statistics_provider) {
      // The fake serial number must be set before |configuration| is
      // constructed below.
      fake_statistics_provider->SetMachineStatistic(
          ash::system::kSerialNumberKeyForTest, "fake_serial_number");
    }
  };
  ScopedFakeSerialNumber fake_serial_number_;
#endif

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

 private:
  base::Value payload_;
};

// Validates that the non-Record portions of the payload are generated
// correctly.
TEST_F(EncryptedReportingJobConfigurationTest, ValidatePayload) {
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, RequestPayloadBuilder().Build(), &client_,
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));
  auto* payload = GetPayload(&configuration);
  const base::Value::Dict& payload_dict = payload->GetDict();
  EXPECT_FALSE(GetDeviceName().empty());
  EXPECT_EQ(*payload_dict.FindStringByDottedPath(
                ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                    GetNamePath()),
            GetDeviceName());
  EXPECT_EQ(*payload_dict.FindStringByDottedPath(
                ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                    GetClientIdPath()),
            kClientId);
  EXPECT_EQ(*payload_dict.FindStringByDottedPath(
                ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                    GetOSPlatformPath()),
            GetOSPlatform());
  EXPECT_EQ(*payload_dict.FindStringByDottedPath(
                ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                    GetOSVersionPath()),
            GetOSVersion());

  EXPECT_EQ(*payload_dict.FindStringByDottedPath(
                ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                    GetMachineUserPath()),
            GetOSUsername());
  EXPECT_EQ(*payload_dict.FindStringByDottedPath(
                ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                    GetChromeVersionPath()),
            version_info::GetVersionNumber());
}

// Validates that the non-Record portions of the payload are generated
// correctly on unmanaged devices and do not contain any device info.
TEST_F(EncryptedReportingJobConfigurationTest,
       ValidatePayloadWithoutDeviceInfo) {
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);

  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::NoAuth(), kServerUrl,
      RequestPayloadBuilder().Build(), /*cloud_policy_client=*/nullptr,
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));
  auto* payload = GetPayload(&configuration);
  ASSERT_THAT(payload, NotNull());
  const base::Value::Dict& payload_dict = payload->GetDict();

  // Expect that the payload does not contain any device info
  EXPECT_THAT(payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetNamePath()),
              IsNull());
  EXPECT_THAT(payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetClientIdPath()),
              IsNull());
  EXPECT_THAT(payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetDMTokenPath()),
              IsNull());
  EXPECT_THAT(payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetOSPlatformPath()),
              IsNull());
  EXPECT_THAT(payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::DeviceDictionaryBuilder::
                      GetOSVersionPath()),
              IsNull());
  EXPECT_THAT(payload_dict.FindStringByDottedPath(
                  ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                      GetMachineUserPath()),
              IsNull());

  // Should still contain chrome version since that's not device info
  EXPECT_EQ(*payload_dict.FindStringByDottedPath(
                ReportingJobConfigurationBase::BrowserDictionaryBuilder::
                    GetChromeVersionPath()),
            version_info::GetVersionNumber());
}

// Ensures that records are added correctly and that the payload is Base64
// encoded.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddEncryptedRecord) {
  const std::string kEncryptedWrappedRecord = "TEST_INFO";
  base::Value record_value = GenerateSingleRecord(kEncryptedWrappedRecord);
  StrictMock<MockCompleteCb> completion_cb;

  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, RequestPayloadBuilder().AddRecord(record_value).Build(),
      &client_,
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  base::Value::List* record_list = nullptr;
  GetRecordList(&configuration, &record_list);
  EXPECT_EQ(record_list->size(), 1u);
  EXPECT_EQ((*record_list)[0], record_value);

  std::string* encrypted_wrapped_record =
      (*record_list)[0].GetDict().FindString(kEncryptedWrappedRecordKey);
  ASSERT_THAT(encrypted_wrapped_record, NotNull());

  std::string decoded_record;
  ASSERT_TRUE(base::Base64Decode(*encrypted_wrapped_record, &decoded_record));
  EXPECT_THAT(decoded_record, StrEq(kEncryptedWrappedRecord));
}

// Ensures that multiple records can be added to the request.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddsMultipleRecords) {
  const std::vector<std::string> kEncryptedWrappedRecords{
      "T", "E", "S", "T", "_", "I", "N", "F", "O"};
  base::Value::List records;
  RequestPayloadBuilder builder;
  for (auto value : kEncryptedWrappedRecords) {
    records.Append(GenerateSingleRecord(value));
    builder.AddRecord(records.back());
  }

  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, builder.Build(), &client_,
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  base::Value::List* record_list = nullptr;
  GetRecordList(&configuration, &record_list);

  EXPECT_EQ(record_list->size(), records.size());

  size_t counter = 0;
  for (const auto& record : records) {
    EXPECT_EQ((*record_list)[counter++], record);
  }

  EXPECT_FALSE(GetAttachEncryptionSettings(&configuration));
}

// Ensures that attach encryption settings request is included when no records
// are present.
TEST_F(EncryptedReportingJobConfigurationTest,
       AllowsAttachEncryptionSettingsAlone) {
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/true};
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, builder.Build(), &client_,
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  base::Value::List* record_list = nullptr;
  GetRecordList(&configuration, &record_list);

  EXPECT_TRUE(record_list->empty());

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
}

TEST_F(EncryptedReportingJobConfigurationTest,
       CorrectlyAddsMultipleRecordsWithAttachEncryptionSettings) {
  const std::vector<std::string> kEncryptedWrappedRecords{
      "T", "E", "S", "T", "_", "I", "N", "F", "O"};
  base::Value::List records;
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/true};
  for (auto value : kEncryptedWrappedRecords) {
    records.Append(GenerateSingleRecord(value));
    builder.AddRecord(records.back());
  }

  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, builder.Build(), &client_,
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  base::Value::List* record_list = nullptr;
  GetRecordList(&configuration, &record_list);

  EXPECT_EQ(record_list->size(), records.size());

  size_t counter = 0;
  for (const auto& record : records) {
    EXPECT_EQ((*record_list)[counter++], record);
  }

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
}

// Ensures that the context can be updated.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddsAndUpdatesContext) {
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, RequestPayloadBuilder().Build(), &client_,
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  const std::string kTestKey = "device.name";
  const std::string kTestValue = "1701-A";
  base::Value::Dict context = GenerateContext(kTestKey, kTestValue);
  configuration.UpdateContext(std::move(context));

  // Ensure the payload includes the path and value.
  base::Value* payload = GetPayload(&configuration);
  const base::Value::Dict& payload_dict = payload->GetDict();
  const std::string* good_result =
      payload_dict.FindStringByDottedPath(kTestKey);
  ASSERT_THAT(good_result, NotNull());
  EXPECT_THAT(*good_result, StrEq(kTestValue));

  // Add a path that isn't in the allow list.
  const std::string kBadTestKey = "profile.string";
  context = GenerateContext(kBadTestKey, kTestValue);
  configuration.UpdateContext(std::move(context));

  // Ensure that the path is removed from the payload.
  payload = GetPayload(&configuration);
  const std::string* bad_result =
      payload_dict.FindStringByDottedPath(kBadTestKey);
  EXPECT_THAT(bad_result, IsNull());

  // Ensure that adding a bad path hasn't destroyed the good path.
  good_result = payload_dict.FindStringByDottedPath(kTestKey);
  EXPECT_THAT(good_result, NotNull());
  EXPECT_EQ(*good_result, kTestValue);

  // Ensure that a good path can be overriden.
  const std::string kUpdatedTestValue = "1701-B";
  context = GenerateContext(kTestKey, kUpdatedTestValue);
  configuration.UpdateContext(std::move(context));
  payload = GetPayload(&configuration);
  good_result = payload_dict.FindStringByDottedPath(kTestKey);
  ASSERT_THAT(good_result, NotNull());
  EXPECT_EQ(*good_result, kUpdatedTestValue);
}

// Ensures that upload success is handled correctly.
TEST_F(EncryptedReportingJobConfigurationTest, OnURLLoadComplete_Success) {
  const std::string kEncryptedWrappedRecord = "TEST_INFO";
  base::Value record_value = GenerateSingleRecord(kEncryptedWrappedRecord);
  auto upload = CreateTestUpload(record_value);

  EXPECT_CALL(*upload.completion_cb, Call(&upload.job, DM_STATUS_SUCCESS,
                                          DeviceManagementService::kSuccess,
                                          Eq(ByRef(upload.response))))
      .Times(1);

  const std::string kTestString = "device.clientId";
  const std::string kTestInt = "1701-A";
  base::Value::Dict context = GenerateContext(kTestString, kTestInt);
  upload.configuration->UpdateContext(std::move(context));

  upload.configuration->OnURLLoadComplete(
      &upload.job, net::OK, DeviceManagementService::kSuccess,
      ResponseValueBuilder::CreateResponseString(upload.response));
}

// Ensures that upload failure is handled correctly.
TEST_F(EncryptedReportingJobConfigurationTest, OnURLLoadComplete_NetError) {
  StrictMock<MockCompleteCb> completion_cb;
  DeviceManagementService::Job job;
  EXPECT_CALL(completion_cb, Call(&job, DM_STATUS_REQUEST_FAILED, _,
                                  testing::Eq(absl::nullopt)))
      .Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, RequestPayloadBuilder().Build(), &client_,
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));
  configuration.OnURLLoadComplete(&job, net::ERR_CONNECTION_RESET,
                                  0 /* ignored */, "");
}

TEST_F(EncryptedReportingJobConfigurationTest,
       IdenticalUploadRetriesThrottled) {
  const size_t kTotalRetries = 10;
  const std::string kEncryptedWrappedRecord = "TEST_INFO";
  base::Value record_value =
      GenerateSingleRecord(kEncryptedWrappedRecord, kPriority);

  base::TimeDelta expected_delay_after = base::Seconds(10);
  for (size_t i = 0; i < kTotalRetries; ++i) {
    auto upload = CreateTestUpload(record_value);
    // Expect upload to fail with a temporary error, to justify a retry.
    EXPECT_CALL(*upload.completion_cb,
                Call(&upload.job, DM_STATUS_TEMPORARY_UNAVAILABLE,
                     DeviceManagementService::kServiceUnavailable,
                     Eq(ByRef(upload.response))))
        .Times(1);

    auto allowed_delay = upload.configuration->WhenIsAllowedToProceed();
    if (i == 0) {
      // First upload allowed immediately.
      EXPECT_FALSE(allowed_delay.is_positive());
    } else {
      // Further uploads allowed with delay.
      EXPECT_THAT(allowed_delay, Ge(expected_delay_after));
      // Double the expectation for the next retry.
      expected_delay_after *= 2;
      // Move forward to allow.
      task_environment_.FastForwardBy(allowed_delay - base::Seconds(1));
      EXPECT_TRUE(upload.configuration->WhenIsAllowedToProceed().is_positive());
      task_environment_.FastForwardBy(base::Seconds(1));
    }

    EXPECT_FALSE(upload.configuration->WhenIsAllowedToProceed().is_positive());
    upload.configuration->AccountForAllowedJob();
    // Process temporary error response code.
    upload.configuration->OnURLLoadComplete(
        &upload.job, net::OK, DeviceManagementService::kServiceUnavailable,
        ResponseValueBuilder::CreateResponseString(upload.response));
  }
}

TEST_F(EncryptedReportingJobConfigurationTest, UploadsSequenceThrottled) {
  const size_t kTotalRetries = 10;
  const std::string kEncryptedWrappedRecord = "TEST_INFO";

  std::vector<TestUpload> uploads;
  base::TimeDelta expected_delay_after = base::Seconds(10);
  for (size_t i = 0; i < kTotalRetries; ++i) {
    // Create new record with next seq id.
    base::Value record_value =
        GenerateSingleRecord(kEncryptedWrappedRecord, kPriority);

    uploads.emplace_back(CreateTestUpload(record_value));
    auto allowed_delay = uploads.back().configuration->WhenIsAllowedToProceed();
    if (i == 0) {
      EXPECT_FALSE(allowed_delay.is_positive());
      // Next retry not before 10 sec.
    } else {
      EXPECT_THAT(allowed_delay, Ge(expected_delay_after));
      // Double the expectation for the next upload.
      expected_delay_after *= 2;
      // Move forward to allow.
      task_environment_.FastForwardBy(allowed_delay - base::Seconds(1));
      EXPECT_TRUE(
          uploads.back().configuration->WhenIsAllowedToProceed().is_positive());
      task_environment_.FastForwardBy(base::Seconds(1));
    }

    EXPECT_FALSE(
        uploads.back().configuration->WhenIsAllowedToProceed().is_positive());
    uploads.back().configuration->AccountForAllowedJob();
  }

  // Now complete all created uploads.
  for (auto& upload : uploads) {
    EXPECT_CALL(*upload.completion_cb, Call(&upload.job, DM_STATUS_SUCCESS,
                                            DeviceManagementService::kSuccess,
                                            Eq(ByRef(upload.response))))
        .Times(1);
    upload.configuration->OnURLLoadComplete(
        &upload.job, net::OK, DeviceManagementService::kSuccess,
        ResponseValueBuilder::CreateResponseString(upload.response));
  }
}

TEST_F(EncryptedReportingJobConfigurationTest,
       SecurityUploadsSequenceNotThrottled) {
  const size_t kTotalRetries = 10;
  const std::string kEncryptedWrappedRecord = "TEST_INFO";

  std::vector<TestUpload> uploads;
  for (size_t i = 0; i < kTotalRetries; ++i) {
    // Create new record with next seq id.
    base::Value record_value = GenerateSingleRecord(
        kEncryptedWrappedRecord, ::reporting::Priority::SECURITY);

    uploads.emplace_back(CreateTestUpload(record_value));
    auto allowed_delay = uploads.back().configuration->WhenIsAllowedToProceed();
    EXPECT_FALSE(allowed_delay.is_positive());
    uploads.back().configuration->AccountForAllowedJob();
  }

  // Now complete all created uploads.
  for (auto& upload : uploads) {
    EXPECT_CALL(*upload.completion_cb, Call(&upload.job, DM_STATUS_SUCCESS,
                                            DeviceManagementService::kSuccess,
                                            Eq(ByRef(upload.response))))
        .Times(1);
    upload.configuration->OnURLLoadComplete(
        &upload.job, net::OK, DeviceManagementService::kSuccess,
        ResponseValueBuilder::CreateResponseString(upload.response));
  }
}

TEST_F(EncryptedReportingJobConfigurationTest, FailedUploadsSequenceThrottled) {
  const size_t kTotalRetries = 10;
  const std::string kEncryptedWrappedRecord = "TEST_INFO";

  for (size_t i = 0; i < kTotalRetries; ++i) {
    // Create new record with next seq id.
    base::Value record_value =
        GenerateSingleRecord(kEncryptedWrappedRecord, kPriority);

    auto upload = CreateTestUpload(record_value);

    EXPECT_CALL(*upload.completion_cb,
                Call(&upload.job, DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID,
                     DeviceManagementService::kInvalidAuthCookieOrDMToken,
                     Eq(ByRef(upload.response))))
        .Times(1);

    auto allowed_delay = upload.configuration->WhenIsAllowedToProceed();
    if (i == 0) {
      // The very first upload is allowed.
      EXPECT_FALSE(allowed_delay.is_positive());
    } else {
      EXPECT_THAT(allowed_delay, Ge(base::Days(1)));
      // Move forward to allow.
      task_environment_.FastForwardBy(allowed_delay - base::Seconds(1));
      EXPECT_TRUE(upload.configuration->WhenIsAllowedToProceed().is_positive());
      task_environment_.FastForwardBy(base::Seconds(1));
    }

    EXPECT_FALSE(upload.configuration->WhenIsAllowedToProceed().is_positive());
    upload.configuration->AccountForAllowedJob();
    upload.configuration->OnURLLoadComplete(
        &upload.job, net::OK,
        DeviceManagementService::kInvalidAuthCookieOrDMToken,
        ResponseValueBuilder::CreateResponseString(upload.response));
  }
}

TEST_F(EncryptedReportingJobConfigurationTest, UmaName) {
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, RequestPayloadBuilder().Build(), &client_, base::DoNothing());

  EXPECT_EQ(configuration.GetUmaName(), "Browser.ERP.UploadEncryptedReport");
}

TEST_F(EncryptedReportingJobConfigurationTest, PayloadTopLevelFields) {
  static constexpr char kDeviceKey[] = "device";
  static constexpr char kBrowserKey[] = "browser";
  static constexpr char kRequestId[] = "requestId";
  static constexpr char kInvalidKey[] = "invalid";

  base::Value::Dict request;
  request.Set(kEncryptedRecordListKey, base::Value::List());
  request.Set(kAttachEncryptionSettingsKey, true);
  request.Set(kDeviceKey, base::Value::Dict());
  request.Set(kBrowserKey, base::Value::Dict());
  request.Set(kInvalidKey, base::Value::Dict());
  request.Set(kRequestId, "request-id");

  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, DMAuth::FromDMToken(client_.dm_token()),
      kServerUrl, std::move(request), &client_, base::DoNothing());

  absl::optional<base::Value> payload =
      base::JSONReader::Read(configuration.GetPayload());

  ASSERT_TRUE(payload);
  ASSERT_TRUE(payload->is_dict());
  EXPECT_TRUE(payload->GetDict().FindList(kEncryptedRecordListKey));
  EXPECT_TRUE(payload->GetDict().FindBool(kAttachEncryptionSettingsKey));
  EXPECT_TRUE(payload->GetDict().FindDict(kDeviceKey));
  EXPECT_TRUE(payload->GetDict().FindDict(kBrowserKey));
  EXPECT_FALSE(payload->GetDict().FindDict(kInvalidKey));
  EXPECT_TRUE(payload->GetDict().FindString(kRequestId));
}
}  // namespace policy
