// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/crowdstrike_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_number_conversions.h"
#include "base/test/test_reg_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace device_signals {

namespace {

constexpr base::FilePath::CharType kFakeFileName[] =
    FILE_PATH_LITERAL("data.zta");

constexpr char kValidFakeJwtZtaContent[] =
    "eyJhbGciOiJSUzI1NiIsImtpZCI6InYxIiwidHlwIjoiSldUIn0."
    "eyJhc3Nlc3NtZW50Ijp7Im92ZXJhbGwiOjU1LCJvcyI6NTAsInNlbnNvcl9jb25maWciOjYwLC"
    "J2ZXJzaW9uIjoiIn0sImV4cCI6MTYxMjg5NTk2NywiaWF0IjoxNjEyODkyMzY3LCJwbGF0Zm9y"
    "bSI6IldpbmRvd3MgMTAiLCJzZXJpYWxfbnVtYmVyIjoic2VyaWFsTnVtYmVyMTIzIiwic3ViIj"
    "oiYmVlZmJlZWZiZWVmYmVlZmJlZWZiZWVmYmVlZjExMTEiLCJjaWQiOiJhYmNkZWYxMjM0NTY3"
    "ODkiLCJ0eXAiOiJjcm93ZHN0cmlrZS16dGErand0In0."
    "Kn8vntxGzDH9D97eIp2JqchPUsrom4qudliIhRlpn1RpjC5ILX2u9hLqdR6yKSVh9VtA2QWx"
    "7EdB_1JeVZFb37sE9wDIq6vENctqH-CcCr2CK-4d8JeeHO_KEOK6xhWSRCD66f-"
    "ZYzYKG5xNlHToth4ef1lZqZyoJ3lLS0qv3uliAP_28c-stxioRUelh7p8lRIUQnS22b0Ud_"
    "LGKB0H7juFx9jdPSFBo31R63MvELMmneltQmjBrj5TKgG30NwAa_OKNsShlgM9kZQes-"
    "ms2RpfEq5UpJ5teDTdqpXtLUEwB7ROkfDhz6nhPHyfUh_S6ummIe_"
    "qLGYVq4dxDtSYnXFt5Etnip1KHBK5RXOBiFV11NahPFWRRd45CoX5mrD9PgL0JxtJLetShNT"
    "-nFktKEIbWtWX3OTiJn7SKnatGB-YRTKkTy0-"
    "2BlTITPM4Uqj3OVTbKimRYJ2bzdzyTc4Ls6FPih6I-"
    "j1KH1SKO80FyKXTUIbeYSGO3t3PcsVgUzVNUUXYdpwn7zHBEivuVgGw2hftdokK9ocx42Sad"
    "Pz_HnIvpt4JGXGOJMsemp4FeCT56hNKuCInN_zsFVe2O6xbZwU_8DTfIsfWNgErCroYr-"
    "Z6NSO6O6xaWojTiEsDSHFQ3lkpccscRZDz0rCluR-2xWUDWkrHht4FGRyCQz4NaM";

constexpr char kExpectedAgentId[] = "beefbeefbeefbeefbeefbeefbeef1111";
constexpr char kExpectedCustomerId[] = "abcdef123456789";

#if BUILDFLAG(IS_WIN)
constexpr wchar_t kCSAgentRegPath[] =
    L"SYSTEM\\CurrentControlSet\\services\\CSAgent\\Sim";

constexpr char kFakeHexCSCustomerId[] = "CABCDEF1234ABCD1234D";
constexpr char kFakeHexCSAgentId[] = "ADEBCA432156ABDC";

// CU is the registry value containing the customer ID.
constexpr wchar_t kCSCURegKey[] = L"CU";

// AG is the registry value containing the agent ID.
constexpr wchar_t kCSAGRegKey[] = L"AG";

void CreateRegistryKey() {
  base::win::RegKey key;
  LONG res = key.Create(HKEY_LOCAL_MACHINE, kCSAgentRegPath, KEY_WRITE);
  ASSERT_EQ(res, ERROR_SUCCESS);
}

void DeleteRegistryKey() {
  base::win::RegKey key(HKEY_LOCAL_MACHINE);
  LONG res = key.DeleteKey(kCSAgentRegPath);
  ASSERT_EQ(res, ERROR_SUCCESS);
}

void SetUpCrowdStrikeInfo(const std::optional<std::string>& customer_id,
                          const std::optional<std::string>& agent_id) {
  CreateRegistryKey();

  base::win::RegKey key;
  LONG res = key.Open(HKEY_LOCAL_MACHINE, kCSAgentRegPath, KEY_WRITE);
  ASSERT_EQ(res, ERROR_SUCCESS);

  if (customer_id) {
    // Have to Hex-decode the values before storing them.
    std::string decoded_customer_id;
    ASSERT_TRUE(base::HexStringToString(customer_id.value().c_str(),
                                        &decoded_customer_id));
    res = key.WriteValue(kCSCURegKey, decoded_customer_id.data(),
                         decoded_customer_id.size(), REG_BINARY);
    ASSERT_EQ(res, ERROR_SUCCESS);
  }

  if (agent_id) {
    // Have to Hex-decode the values before storing them.
    std::string decoded_agent_id;
    ASSERT_TRUE(
        base::HexStringToString(agent_id.value().c_str(), &decoded_agent_id));
    res = key.WriteValue(kCSAGRegKey, decoded_agent_id.data(),
                         decoded_agent_id.size(), REG_BINARY);
    ASSERT_EQ(res, ERROR_SUCCESS);
  }
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

class CrowdStrikeClientTest : public testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE);
#endif

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    client_ = CrowdStrikeClient::CreateForTesting(GetDataFilePath());
  }

  void CreateFakeFileWithContent(const std::string& file_content) {
    ASSERT_TRUE(base::WriteFile(GetDataFilePath(), file_content));
  }

  void DeleteFakeFile() { ASSERT_TRUE(base::DeleteFile(GetDataFilePath())); }

  base::FilePath GetDataFilePath() {
    return scoped_temp_dir_.GetPath().Append(kFakeFileName);
  }

  std::optional<CrowdStrikeSignals> GetSignals(
      std::optional<SignalCollectionError> expected_error = std::nullopt) {
    base::test::TestFuture<std::optional<CrowdStrikeSignals>,
                           std::optional<SignalCollectionError>>
        future;
    client_->GetIdentifiers(future.GetCallback());

    // Should not have an error if signals are expected to be returned.
    if (expected_error) {
      EXPECT_EQ(expected_error, future.Get<1>());
    } else {
      EXPECT_FALSE(future.Get<1>());
    }

    return future.Get<0>();
  }

  std::optional<SignalCollectionError> GetSignalCollectionError() {
    base::test::TestFuture<std::optional<CrowdStrikeSignals>,
                           std::optional<SignalCollectionError>>
        future;
    client_->GetIdentifiers(future.GetCallback());

    // Should not have signals if an error is expected to be returned.
    EXPECT_FALSE(future.Get<0>());

    return future.Get<1>();
  }

  void ValidateHistogram(std::optional<SignalsParsingError> error) {
    static constexpr char kCrowdStrikeErrorHistogram[] =
        "Enterprise.DeviceSignals.Collection.CrowdStrike.Error";
    if (error) {
      histogram_tester_.ExpectUniqueSample(kCrowdStrikeErrorHistogram,
                                           error.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(kCrowdStrikeErrorHistogram, 0);
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir scoped_temp_dir_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::HistogramTester histogram_tester_;

#if BUILDFLAG(IS_WIN)
  registry_util::RegistryOverrideManager registry_override_manager_;
#endif

  std::unique_ptr<CrowdStrikeClient> client_;
};

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile) {
  // Expect no signals and no error.
  EXPECT_FALSE(GetSignalCollectionError());

  // No value logged, not having the file available is not considered a failure.
  ValidateHistogram(std::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_EmptyFile) {
  CreateFakeFileWithContent("");

  // Expect no signals and no error.
  EXPECT_FALSE(GetSignalCollectionError());

  // No value logged, having an empty file is not considered a failure.
  ValidateHistogram(std::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_NotJwt) {
  CreateFakeFileWithContent("some.random.content");

  const auto& error = GetSignalCollectionError();
  ASSERT_TRUE(error);
  EXPECT_EQ(error.value(), SignalCollectionError::kParsingFailed);

  ValidateHistogram(SignalsParsingError::kJsonParsingFailed);
}

TEST_F(CrowdStrikeClientTest, Identifiers_MaxDataSize) {
  std::string content(33 * 1024, 'a');
  CreateFakeFileWithContent(content);

  const auto& error = GetSignalCollectionError();
  ASSERT_TRUE(error);
  EXPECT_EQ(error.value(), SignalCollectionError::kParsingFailed);

  ValidateHistogram(SignalsParsingError::kHitMaxDataSize);
}

TEST_F(CrowdStrikeClientTest, Identifiers_DecodingFailed) {
  CreateFakeFileWithContent("some.random%%.content");

  const auto& error = GetSignalCollectionError();
  ASSERT_TRUE(error);
  EXPECT_EQ(error.value(), SignalCollectionError::kParsingFailed);

  ValidateHistogram(SignalsParsingError::kBase64DecodingFailed);
}

TEST_F(CrowdStrikeClientTest, Identifiers_MissingJwtSection) {
  constexpr char kFakeJwtZtaContent[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6InYxIiwidHlwIjoiSldUIn0."
      "eyJhc3Nlc3NtZW50Ijp7Im92ZXJhbGwiOjU1LCJvcyI6NTAsInNlbnNvcl9jb25maWciOjYw"
      "LCJ2ZXJzaW9uIjoiIn0sImV4cCI6MTYxMjg5NTk2NywiaWF0IjoxNjEyODkyMzY3LCJwbGF0"
      "Zm9ybSI6IldpbmRvd3MgMTAiLCJzZXJpYWxfbnVtYmVyIjoic2VyaWFsTnVtYmVyMTIzIiwi"
      "c3ViIjoiYmVlZmJlZWZiZWVmYmVlZmJlZWZiZWVmYmVlZjExMTEiLCJ0eXAiOiJjcm93ZHN0"
      "cmlrZS16dGErand0In0";
  CreateFakeFileWithContent(kFakeJwtZtaContent);

  const auto& error = GetSignalCollectionError();
  ASSERT_TRUE(error);
  EXPECT_EQ(error.value(), SignalCollectionError::kUnexpectedValue);

  ValidateHistogram(SignalsParsingError::kDataMalformed);
}

TEST_F(CrowdStrikeClientTest, Identifiers_MissingSub) {
  // JWT value where `sub` is missing from the payload.
  static constexpr char kFakeJwtZtaContent[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6InYxIiwidHlwIjoiSldUIn0."
      "eyJhc3Nlc3NtZW50Ijp7Im92ZXJhbGwiOjU1LCJvcyI6NTAsInNlbnNvcl9jb25maWciOjYw"
      "LCJ2ZXJzaW9uIjoiIn0sImV4cCI6MTYxMjg5NTk2NywiaWF0IjoxNjEyODkyMzY3LCJwbGF0"
      "Zm9ybSI6IldpbmRvd3MgMTAiLCJzZXJpYWxfbnVtYmVyIjoic2VyaWFsTnVtYmVyMTIzIiwi"
      "dHlwIjoiY3Jvd2RzdHJpa2UtenRhK2p3dCJ9."
      "Kn8vntxGzDH9D97eIp2JqchPUsrom4qudliIhRlpn1RpjC5ILX2u9hLqdR6yKSVh9VtA2QWx"
      "7EdB_1JeVZFb37sE9wDIq6vENctqH-CcCr2CK-4d8JeeHO_KEOK6xhWSRCD66f-"
      "ZYzYKG5xNlHToth4ef1lZqZyoJ3lLS0qv3uliAP_28c-stxioRUelh7p8lRIUQnS22b0Ud_"
      "LGKB0H7juFx9jdPSFBo31R63MvELMmneltQmjBrj5TKgG30NwAa_OKNsShlgM9kZQes-"
      "ms2RpfEq5UpJ5teDTdqpXtLUEwB7ROkfDhz6nhPHyfUh_S6ummIe_"
      "qLGYVq4dxDtSYnXFt5Etnip1KHBK5RXOBiFV11NahPFWRRd45CoX5mrD9PgL0JxtJLetShNT"
      "-nFktKEIbWtWX3OTiJn7SKnatGB-YRTKkTy0-"
      "2BlTITPM4Uqj3OVTbKimRYJ2bzdzyTc4Ls6FPih6I-"
      "j1KH1SKO80FyKXTUIbeYSGO3t3PcsVgUzVNUUXYdpwn7zHBEivuVgGw2hftdokK9ocx42Sad"
      "Pz_HnIvpt4JGXGOJMsemp4FeCT56hNKuCInN_zsFVe2O6xbZwU_8DTfIsfWNgErCroYr-"
      "Z6NSO6O6xaWojTiEsDSHFQ3lkpccscRZDz0rCluR-2xWUDWkrHht4FGRyCQz4NaM";
  CreateFakeFileWithContent(kFakeJwtZtaContent);

  const auto& error = GetSignalCollectionError();
  ASSERT_TRUE(error);
  EXPECT_EQ(error.value(), SignalCollectionError::kParsingFailed);

  ValidateHistogram(SignalsParsingError::kMissingRequiredProperty);
}

TEST_F(CrowdStrikeClientTest, Identifiers_Success) {
  CreateFakeFileWithContent(kValidFakeJwtZtaContent);
  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, kExpectedAgentId);
  EXPECT_EQ(signals->customer_id, kExpectedCustomerId);

  ValidateHistogram(std::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_Success_CachedValue) {
  CreateFakeFileWithContent(kValidFakeJwtZtaContent);
  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, kExpectedAgentId);
  EXPECT_EQ(signals->customer_id, kExpectedCustomerId);

  DeleteFakeFile();

  signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, kExpectedAgentId);
  EXPECT_EQ(signals->customer_id, kExpectedCustomerId);

  // Going beyond cache expiry without the data file should make
  // the client return no value.
  static constexpr int kBeyondCacheExpiryInHours = 2;
  task_environment_.FastForwardBy(base::Hours(kBeyondCacheExpiryInHours));

  EXPECT_FALSE(GetSignals());
}

#if BUILDFLAG(IS_WIN)

// Tests that only having the customer ID in the registry is treated
// as insufficient, and no value is returned.
TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_RegistryNoAgentId) {
  SetUpCrowdStrikeInfo(kFakeHexCSCustomerId, std::nullopt);

  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->customer_id, base::ToLowerASCII(kFakeHexCSCustomerId));
  EXPECT_TRUE(signals->agent_id.empty());
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_RegistryNoCustomerId) {
  SetUpCrowdStrikeInfo(std::nullopt, kFakeHexCSAgentId);

  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, base::ToLowerASCII(kFakeHexCSAgentId));
  EXPECT_TRUE(signals->customer_id.empty());

  DeleteRegistryKey();

  // Expect the value to not have been cached.
  EXPECT_FALSE(GetSignals());
}

TEST_F(CrowdStrikeClientTest, Identifiers_FileHasPrecendence) {
  SetUpCrowdStrikeInfo(kFakeHexCSCustomerId, kFakeHexCSAgentId);

  CreateFakeFileWithContent(kValidFakeJwtZtaContent);

  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, kExpectedAgentId);
  EXPECT_EQ(signals->customer_id, kExpectedCustomerId);
}

TEST_F(CrowdStrikeClientTest, Identifiers_DecodingFailed_RegistryFallback) {
  CreateFakeFileWithContent("some.random%%.content");
  SetUpCrowdStrikeInfo(kFakeHexCSCustomerId, kFakeHexCSAgentId);

  auto signals =
      GetSignals(/*expected_error=*/SignalCollectionError::kParsingFailed);

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->customer_id, base::ToLowerASCII(kFakeHexCSCustomerId));
  EXPECT_EQ(signals->agent_id, base::ToLowerASCII(kFakeHexCSAgentId));
  ValidateHistogram(SignalsParsingError::kBase64DecodingFailed);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace device_signals
