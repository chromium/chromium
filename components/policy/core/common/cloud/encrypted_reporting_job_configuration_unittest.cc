// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"

#include <cstddef>
#include <optional>
#include <string_view>

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
#include "components/reporting/util/encrypted_reporting_json_keys.h"
#include "components/version_info/version_info.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

using ::testing::_;
using ::testing::ByRef;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::IsEmpty;
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

constexpr char kDmToken[] = "fake-dm-token";
constexpr char kClientId[] = "fake-client-id";
constexpr char kServerUrl[] = "https://example.com/reporting";

uint64_t GetNextSequenceId() {
  static uint64_t kSequencingId = 0;
  return kSequencingId++;
}

class RequestPayloadBuilder {
 public:
  explicit RequestPayloadBuilder(bool attach_encryption_settings = false,
                                 bool request_configuration_file = false,
                                 bool client_automated_test = false) {
    if (attach_encryption_settings) {
      payload_.Set(reporting::json_keys::kAttachEncryptionSettings, true);
    }
    if (request_configuration_file) {
      payload_.Set(reporting::json_keys::kConfigurationFileVersion, 1234);
    }
    if (client_automated_test) {
      payload_.Set(reporting::json_keys::kSource, "tast");
    }
    payload_.Set(reporting::json_keys::kEncryptedRecordList,
                 base::Value::List());
  }

  RequestPayloadBuilder& AddRecord(const base::Value& record) {
    base::Value::List* records_list =
        payload_.FindList(reporting::json_keys::kEncryptedRecordList);
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
      std::optional<base::Value> upload_failure) {
    base::Value::Dict response;

    response.Set(reporting::json_keys::kLastSucceedUploadedRecord,
                 sequence_information.Clone());

    if (upload_failure.has_value()) {
      response.Set(reporting::json_keys::kFirstFailedUploadedRecord,
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
    return GetPath(reporting::json_keys::kFirstFailedUploadedRecord,
                   GetFailedUploadSequencingIdPath());
  }

  static std::string GetUploadFailureFailedUploadGenerationIdPath() {
    return GetPath(reporting::json_keys::kFirstFailedUploadedRecord,
                   GetFailedUploadGenerationIdPath());
  }

  static std::string GetUploadFailureFailedUploadPriorityPath() {
    return GetPath(reporting::json_keys::kFirstFailedUploadedRecord,
                   GetFailedUploadPriorityPath());
  }

  static std::string GetUploadFailureStatusCodePath() {
    return GetPath(reporting::json_keys::kFirstFailedUploadedRecord,
                   GetFailureStatusCodePath());
  }

 private:
  static std::string GetPath(std::string_view base, std::string_view leaf) {
    return base::JoinString({base, leaf}, ".");
  }

  static std::string GetFailedUploadSequencingIdPath() {
    return GetPath(reporting::json_keys::kFailedUploadedRecord,
                   reporting::json_keys::kSequencingId);
  }

  static std::string GetFailedUploadGenerationIdPath() {
    return GetPath(reporting::json_keys::kFailedUploadedRecord,
                   reporting::json_keys::kGenerationId);
  }

  static std::string GetFailedUploadPriorityPath() {
    return GetPath(reporting::json_keys::kFailedUploadedRecord,
                   reporting::json_keys::kPriority);
  }

  static std::string GetFailureStatusCodePath() {
    return GetPath(reporting::json_keys::kFailedUploadedRecord,
                   reporting::json_keys::kErrorCode);
  }

  static std::string GetFailureStatusMessagePath() {
    return GetPath(reporting::json_keys::kFailedUploadedRecord,
                   reporting::json_keys::kErrorMessage);
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
                                           std::optional<base::Value::Dict>)>;
  using MockUploadResponseCb =
      MockFunction<void(int /*net_error*/, int /*response_code*/)>;
  struct TestUpload {
    std::unique_ptr<EncryptedReportingJobConfiguration> configuration;
    std::unique_ptr<StrictMock<MockCompleteCb>> completion_cb;
    std::unique_ptr<StrictMock<MockUploadResponseCb>> upload_response_cb;
    base::Value::Dict response;
    DeviceManagementService::Job job;
  };

  void SetUp() override {
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
        *record_value.GetDict().FindDict(
            reporting::json_keys::kSequenceInformation),
        std::nullopt);
    test_upload.completion_cb = std::make_unique<StrictMock<MockCompleteCb>>();
    test_upload.upload_response_cb =
        std::make_unique<StrictMock<MockUploadResponseCb>>();
    test_upload.configuration =
        std::make_unique<EncryptedReportingJobConfiguration>(
            shared_url_loader_factory_, kServerUrl,
            RequestPayloadBuilder().AddRecord(record_value).Build(),
            client_.dm_token(), client_.client_id(),
            base::BindOnce(
                &MockUploadResponseCb::Call,
                base::Unretained(test_upload.upload_response_cb.get())),
            base::BindOnce(&MockCompleteCb::Call,
                           base::Unretained(test_upload.completion_cb.get())));
    return test_upload;
  }

  base::Value GenerateSingleRecord(std::string_view encrypted_wrapped_record,
                                   ::reporting::Priority priority = kPriority) {
    base::Value::Dict record_dictionary;
    std::string base64_encode = base::Base64Encode(encrypted_wrapped_record);
    record_dictionary.Set(reporting::json_keys::kEncryptedWrappedRecord,
                          base64_encode);

    base::Value::Dict* const sequencing_dictionary =
        record_dictionary.EnsureDict(
            reporting::json_keys::kSequenceInformation);
    sequencing_dictionary->Set(reporting::json_keys::kSequencingId,
                               base::NumberToString(GetNextSequenceId()));
    sequencing_dictionary->Set(reporting::json_keys::kGenerationId,
                               base::NumberToString(kGenerationId));
    sequencing_dictionary->Set(reporting::json_keys::kPriority, priority);

    base::Value::Dict* const encryption_info_dictionary =
        record_dictionary.EnsureDict(reporting::json_keys::kEncryptionInfo);
    encryption_info_dictionary->Set(reporting::json_keys::kEncryptionKey,
                                    kEncryptionKeyValue);
    encryption_info_dictionary->Set(reporting::json_keys::kPublicKeyId,
                                    base::NumberToString(kPublicKeyIdValue));

    return base::Value(std::move(record_dictionary));
  }

  static base::Value::Dict GenerateContext(std::string_view key,
                                           std::string_view value) {
    base::Value::Dict context;
    context.SetByDottedPath(key, value);
    return context;
  }

  base::Value::List* GetRecordList(
      EncryptedReportingJobConfiguration* configuration) {
    base::Value* const payload = GetPayload(configuration);
    auto* const record_list =
        payload->GetDict().FindList(reporting::json_keys::kEncryptedRecordList);
    EXPECT_THAT(record_list, NotNull());
    return record_list;
  }

  bool GetAttachEncryptionSettings(
      EncryptedReportingJobConfiguration* configuration) {
    base::Value* const payload = GetPayload(configuration);
    const auto attach_encryption_settings = payload->GetDict().FindBool(
        reporting::json_keys::kAttachEncryptionSettings);
    return attach_encryption_settings.has_value() &&
           attach_encryption_settings.value();
  }

  bool VerifyConfigurationFileVersion(
      EncryptedReportingJobConfiguration* configuration) {
    base::Value* const payload = GetPayload(configuration);
    auto* request_configuration_file = payload->GetDict().Find(
        reporting::json_keys::kConfigurationFileVersion);
    return request_configuration_file->is_int() &&
           request_configuration_file->GetIfInt() == 1234;
  }

  bool VerifySourceIsTast(EncryptedReportingJobConfiguration* configuration) {
    base::Value* const payload = GetPayload(configuration);
    auto* client_automated_test =
        payload->GetDict().Find(reporting::json_keys::kSource);
    return client_automated_test->is_string() &&
           *client_automated_test->GetIfString() == "tast";
  }

  base::Value* GetPayload(EncryptedReportingJobConfiguration* configuration) {
    std::optional<base::Value> payload_result =
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
          ash::system::kSerialNumberKey, "fake_serial_number");
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
      shared_url_loader_factory_, kServerUrl, RequestPayloadBuilder().Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
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
      shared_url_loader_factory_, kServerUrl, RequestPayloadBuilder().Build(),
      /*dm_token=*/"", /*client_id=*/"",
      /*response_cb=*/base::DoNothing(),
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
      shared_url_loader_factory_, kServerUrl,
      RequestPayloadBuilder().AddRecord(record_value).Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, ElementsAre(Eq(ByRef(record_value))));

  auto* const encrypted_wrapped_record = (*record_list)[0].GetDict().FindString(
      reporting::json_keys::kEncryptedWrappedRecord);
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
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, Eq(ByRef(records)));

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
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, IsEmpty());

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
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, Eq(ByRef(records)));

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
}

TEST_F(EncryptedReportingJobConfigurationTest,
       AllowsAttachConfigurationFileAlone) {
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/false,
                                /*request_configuration_file=*/true};
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, IsEmpty());

  EXPECT_TRUE(VerifyConfigurationFileVersion(&configuration));
}

TEST_F(EncryptedReportingJobConfigurationTest,
       AllowsAttachConfigurationFileAndEncryptionSettingsWithoutRecords) {
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/true,
                                /*request_configuration_file=*/true};
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, IsEmpty());

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
  EXPECT_TRUE(VerifyConfigurationFileVersion(&configuration));
}

TEST_F(
    EncryptedReportingJobConfigurationTest,
    CorrectlyAddsMultipleRecordsWithAttachConfigurationFileAndAttachEncryptionKey) {
  const std::vector<std::string> kEncryptedWrappedRecords{
      "T", "E", "S", "T", "_", "I", "N", "F", "O"};
  base::Value::List records;
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/true,
                                /*request_configuration_file=*/true};
  for (auto value : kEncryptedWrappedRecords) {
    records.Append(GenerateSingleRecord(value));
    builder.AddRecord(records.back());
  }

  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, Eq(ByRef(records)));

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
  EXPECT_TRUE(VerifyConfigurationFileVersion(&configuration));
}

TEST_F(EncryptedReportingJobConfigurationTest, AllowsSourceTastAlone) {
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/false,
                                /*request_configuration_file=*/false,
                                /*client_automated_test=*/true};
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, IsEmpty());

  EXPECT_TRUE(VerifySourceIsTast(&configuration));
}

TEST_F(
    EncryptedReportingJobConfigurationTest,
    AllowsAttachConfigurationFileEncryptionSettingsAndSourceTastWithoutRecords) {
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/true,
                                /*request_configuration_file=*/true,
                                /*client_automated_test=*/true};
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, IsEmpty());

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
  EXPECT_TRUE(VerifyConfigurationFileVersion(&configuration));
  EXPECT_TRUE(VerifySourceIsTast(&configuration));
}

TEST_F(
    EncryptedReportingJobConfigurationTest,
    CorrectlyAddsMultipleRecordsWithAttachConfigurationFileAttachEncryptionKeyAndSourceTast) {
  const std::vector<std::string> kEncryptedWrappedRecords{
      "T", "E", "S", "T", "_", "I", "N", "F", "O"};
  base::Value::List records;
  RequestPayloadBuilder builder{/*attach_encryption_settings=*/true,
                                /*request_configuration_file=*/true,
                                /*client_automated_test=*/true};
  for (auto value : kEncryptedWrappedRecords) {
    records.Append(GenerateSingleRecord(value));
    builder.AddRecord(records.back());
  }

  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, builder.Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));

  auto* const record_list = GetRecordList(&configuration);
  EXPECT_THAT(*record_list, Eq(ByRef(records)));

  EXPECT_TRUE(GetAttachEncryptionSettings(&configuration));
  EXPECT_TRUE(VerifyConfigurationFileVersion(&configuration));
  EXPECT_TRUE(VerifySourceIsTast(&configuration));
}

// Ensures that the context can be updated.
TEST_F(EncryptedReportingJobConfigurationTest, CorrectlyAddsAndUpdatesContext) {
  StrictMock<MockCompleteCb> completion_cb;
  EXPECT_CALL(completion_cb, Call(_, _, _, _)).Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, RequestPayloadBuilder().Build(),
      client_.dm_token(), client_.client_id(),
      /*response_cb=*/base::DoNothing(),
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
  EXPECT_CALL(*upload.upload_response_cb, Call(Eq(net::OK), Eq(net::HTTP_OK)))
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
                                  testing::Eq(std::nullopt)))
      .Times(1);
  StrictMock<MockUploadResponseCb> upload_response_cb;
  EXPECT_CALL(upload_response_cb, Call(Eq(net::ERR_CONNECTION_RESET), _))
      .Times(1);
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, RequestPayloadBuilder().Build(),
      client_.dm_token(), client_.client_id(),
      base::BindOnce(&MockUploadResponseCb::Call,
                     base::Unretained(&upload_response_cb)),
      base::BindOnce(&MockCompleteCb::Call, base::Unretained(&completion_cb)));
  configuration.OnURLLoadComplete(&job, net::ERR_CONNECTION_RESET,
                                  0 /* ignored */, "");
}
TEST_F(EncryptedReportingJobConfigurationTest, ManagedDeviceUmaName) {
  // Non-null cloud policy client indicates device is unmanaged.
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, RequestPayloadBuilder().Build(),
      client_.dm_token(), client_.client_id(), base::DoNothing(),
      base::DoNothing());

  EXPECT_EQ(configuration.GetUmaName(),
            "Browser.ERP.ManagedUploadEncryptedReport");
}

TEST_F(EncryptedReportingJobConfigurationTest, UnmanagedDeviceUmaName) {
  // Null cloud policy client indicates device is unmanaged.
  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, RequestPayloadBuilder().Build(),
      /*dm_token=*/"", /*client_id=*/"", base::DoNothing(), base::DoNothing());

  EXPECT_EQ(configuration.GetUmaName(),
            "Browser.ERP.UnmanagedUploadEncryptedReport");
}

TEST_F(EncryptedReportingJobConfigurationTest, PayloadTopLevelFields) {
  static constexpr char kInvalidKey[] = "invalid";

  base::Value::Dict request;
  request.Set(reporting::json_keys::kEncryptedRecordList, base::Value::List());
  request.Set(reporting::json_keys::kConfigurationFileVersion, 1234);
  request.Set(reporting::json_keys::kAttachEncryptionSettings, true);
  request.Set(reporting::json_keys::kSource, "tast");
  request.Set(reporting::json_keys::kDevice, base::Value::Dict());
  request.Set(reporting::json_keys::kBrowser, base::Value::Dict());
  request.Set(kInvalidKey, base::Value::Dict());
  request.Set(reporting::json_keys::kRequestId, "request-id");

  EncryptedReportingJobConfiguration configuration(
      shared_url_loader_factory_, kServerUrl, std::move(request),
      client_.dm_token(), client_.client_id(), base::DoNothing(),
      base::DoNothing());

  std::optional<base::Value> payload =
      base::JSONReader::Read(configuration.GetPayload());

  ASSERT_TRUE(payload);
  ASSERT_TRUE(payload->is_dict());
  EXPECT_TRUE(
      payload->GetDict().FindList(reporting::json_keys::kEncryptedRecordList));
  EXPECT_TRUE(payload->GetDict().FindBool(
      reporting::json_keys::kAttachEncryptionSettings));
  EXPECT_TRUE(payload->GetDict()
                  .FindInt(reporting::json_keys::kConfigurationFileVersion)
                  .has_value());
  EXPECT_TRUE(payload->GetDict().FindString(reporting::json_keys::kSource));
  EXPECT_TRUE(payload->GetDict().FindDict(reporting::json_keys::kDevice));
  EXPECT_TRUE(payload->GetDict().FindDict(reporting::json_keys::kBrowser));
  EXPECT_FALSE(payload->GetDict().FindDict(kInvalidKey));
  EXPECT_TRUE(payload->GetDict().FindString(reporting::json_keys::kRequestId));
}
}  // namespace policy
