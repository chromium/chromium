// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/crowdstrike_client.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/scoped_platform_wrapper.h"
#include "components/device_signals/core/common/signals_features.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_number_conversions.h"
#include "base/test/test_reg_util_win.h"
#elif BUILDFLAG(IS_MAC)
#include "base/command_line.h"
#endif  // BUILDFLAG(IS_WIN)

namespace device_signals {

using testing::_;

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

// Overwrite the registry values for both agent and customer ID with empty
// strings, instead of simply removing the key itself since the mocked registry
// doesn't work well with local machine environments.
void DeleteRegistryValues() {
  base::win::RegKey key;
  LONG res = key.Open(HKEY_LOCAL_MACHINE, kCSAgentRegPath, KEY_WRITE);
  ASSERT_EQ(res, ERROR_SUCCESS);

  std::string empty_string = std::string();
  res = key.WriteValue(kCSCURegKey, empty_string.data(), empty_string.size(),
                       REG_BINARY);
  ASSERT_EQ(res, ERROR_SUCCESS);

  res = key.WriteValue(kCSAGRegKey, empty_string.data(), empty_string.size(),
                       REG_BINARY);
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
    scoped_feature_list_.InitAndEnableFeature(
        enterprise_signals::features::kDetectedAgentSignalCollectionEnabled);
  }

  void InitializeClient(bool empty_file = true) {
    client_ = CrowdStrikeClient::CreateForTesting(
        empty_file ? GetDataFilePath() : base::FilePath());
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
  testing::NiceMock<ScopedPlatformWrapper> scoped_platform_wrapper_;
  base::ScopedTempDir scoped_temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::HistogramTester histogram_tester_;

#if BUILDFLAG(IS_WIN)
  registry_util::RegistryOverrideManager registry_override_manager_;
#endif

  std::unique_ptr<CrowdStrikeClient> client_;
};

TEST_F(CrowdStrikeClientTest, Identifiers_EmptyFilePath) {
  InitializeClient(/*empty_file=*/true);
  // Expect no signals and no error.
  EXPECT_FALSE(GetSignalCollectionError());

  // No value logged, not having the file available is not considered a failure.
  ValidateHistogram(std::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile) {
  InitializeClient();
  // Expect no signals and no error.
  EXPECT_FALSE(GetSignalCollectionError());

  // No value logged, not having the file available is not considered a failure.
  ValidateHistogram(std::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_EmptyFile) {
  InitializeClient();
  CreateFakeFileWithContent("");

  // Expect no signals and no error.
  EXPECT_FALSE(GetSignalCollectionError());

  // No value logged, having an empty file is not considered a failure.
  ValidateHistogram(std::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_NotJwt) {
  InitializeClient();
  CreateFakeFileWithContent("some.random.content");

  const auto& error = GetSignalCollectionError();
  ASSERT_TRUE(error);
  EXPECT_EQ(error.value(), SignalCollectionError::kParsingFailed);

  ValidateHistogram(SignalsParsingError::kJsonParsingFailed);
}

TEST_F(CrowdStrikeClientTest, Identifiers_MaxDataSize) {
  InitializeClient();
  std::string content(33 * 1024, 'a');
  CreateFakeFileWithContent(content);

  const auto& error = GetSignalCollectionError();
  ASSERT_TRUE(error);
  EXPECT_EQ(error.value(), SignalCollectionError::kParsingFailed);

  ValidateHistogram(SignalsParsingError::kHitMaxDataSize);
}

TEST_F(CrowdStrikeClientTest, Identifiers_DecodingFailed) {
  InitializeClient();
  CreateFakeFileWithContent("some.random%%.content");

  const auto& error = GetSignalCollectionError();
  ASSERT_TRUE(error);
  EXPECT_EQ(error.value(), SignalCollectionError::kParsingFailed);

  ValidateHistogram(SignalsParsingError::kBase64DecodingFailed);
}

TEST_F(CrowdStrikeClientTest, Identifiers_MissingJwtSection) {
  InitializeClient();
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
  InitializeClient();
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
  InitializeClient();
  CreateFakeFileWithContent(kValidFakeJwtZtaContent);
  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, kExpectedAgentId);
  EXPECT_EQ(signals->customer_id, kExpectedCustomerId);

  ValidateHistogram(std::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_Success_CachedValue) {
  InitializeClient();
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
  InitializeClient();
  SetUpCrowdStrikeInfo(kFakeHexCSCustomerId, std::nullopt);

  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->customer_id, base::ToLowerASCII(kFakeHexCSCustomerId));
  EXPECT_TRUE(signals->agent_id.empty());
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_RegistryNoCustomerId) {
  InitializeClient();
  SetUpCrowdStrikeInfo(std::nullopt, kFakeHexCSAgentId);

  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, base::ToLowerASCII(kFakeHexCSAgentId));
  EXPECT_TRUE(signals->customer_id.empty());

  DeleteRegistryValues();

  // Expect the value to still be cached.
  signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, base::ToLowerASCII(kFakeHexCSAgentId));
  EXPECT_TRUE(signals->customer_id.empty());
}

TEST_F(CrowdStrikeClientTest, Identifiers_FileHasPrecendence) {
  InitializeClient();
  SetUpCrowdStrikeInfo(kFakeHexCSCustomerId, kFakeHexCSAgentId);

  CreateFakeFileWithContent(kValidFakeJwtZtaContent);

  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, kExpectedAgentId);
  EXPECT_EQ(signals->customer_id, kExpectedCustomerId);
}

TEST_F(CrowdStrikeClientTest, Identifiers_DecodingFailed_RegistryFallback) {
  InitializeClient();
  CreateFakeFileWithContent("some.random%%.content");
  SetUpCrowdStrikeInfo(kFakeHexCSCustomerId, kFakeHexCSAgentId);

  auto signals =
      GetSignals(/*expected_error=*/SignalCollectionError::kParsingFailed);

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->customer_id, base::ToLowerASCII(kFakeHexCSCustomerId));
  EXPECT_EQ(signals->agent_id, base::ToLowerASCII(kFakeHexCSAgentId));
  ValidateHistogram(SignalsParsingError::kBase64DecodingFailed);
}

#elif BUILDFLAG(IS_MAC)

namespace {

constexpr base::FilePath::CharType kFlaconCtlPath[] =
    FILE_PATH_LITERAL("/Applications/Falcon.app/Contents/Resources/falconctl");

base::CommandLine BuildExpectedCommandLine() {
  base::FilePath ctl_path(kFlaconCtlPath);
  base::CommandLine command(ctl_path);
  command.AppendArg("info");
  return command;
}

std::string GetPlistContent(std::optional<std::string> aid,
                            std::optional<std::string> cid) {
  static constexpr const char* kPlistPrefix = R"(
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>falcon_version</key>
    <string>7.26.19707.0</string>
    <key>rfm</key>
    <false/>
    <key>sensor_loaded</key>
    <true/>)";
  static constexpr const char* kPlistSuffix = R"(
</dict>
</plist>
  )";
  static constexpr const char* kPlistAidFormat = R"(
    <key>aid</key>
    <string>%s</string>)";
  static constexpr const char* kPlistCidFormat = R"(
    <key>cid</key>
    <string>%s</string>)";

  std::vector<std::string> tokens;
  tokens.push_back(kPlistPrefix);
  if (aid) {
    tokens.push_back(base::StringPrintf(kPlistAidFormat, aid.value()));
  }
  if (cid) {
    tokens.push_back(base::StringPrintf(kPlistCidFormat, cid.value()));
  }
  tokens.push_back(kPlistSuffix);

  return base::StrCat(tokens);
}

MATCHER_P(IsCommandLine, cmd_line, "") {
  return arg.GetCommandLineString() == cmd_line.GetCommandLineString();
}

}  // namespace

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_CommandFailed) {
  InitializeClient();

  EXPECT_CALL(scoped_platform_wrapper_,
              PathExists(base::FilePath(kFlaconCtlPath)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(scoped_platform_wrapper_,
              Execute(IsCommandLine(BuildExpectedCommandLine()), _))
      .WillOnce(testing::Return(false));

  auto signals = GetSignals();
  EXPECT_FALSE(signals);
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_InvalidPlist) {
  InitializeClient();

  EXPECT_CALL(scoped_platform_wrapper_,
              PathExists(base::FilePath(kFlaconCtlPath)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(scoped_platform_wrapper_,
              Execute(IsCommandLine(BuildExpectedCommandLine()), _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<1>("invalid plist"),
                               testing::Return(true)));

  auto signals = GetSignals();
  EXPECT_FALSE(signals);
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_BothIds) {
  InitializeClient();

  const auto& plist_content =
      GetPlistContent("AGENT_ID_123", "CUSTOMER_ID_456");

  EXPECT_CALL(scoped_platform_wrapper_,
              PathExists(base::FilePath(kFlaconCtlPath)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(scoped_platform_wrapper_,
              Execute(IsCommandLine(BuildExpectedCommandLine()), _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<1>(plist_content),
                               testing::Return(true)));

  auto signals = GetSignals();
  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, "agent_id_123");
  EXPECT_EQ(signals->customer_id, "customer_id_456");
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_FeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      enterprise_signals::features::kDetectedAgentSignalCollectionEnabled);

  InitializeClient();

  auto signals = GetSignals();
  ASSERT_FALSE(signals);
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_OnlyAgentId) {
  InitializeClient();

  const auto& plist_content = GetPlistContent("AGENT_ID_123", std::nullopt);

  EXPECT_CALL(scoped_platform_wrapper_,
              PathExists(base::FilePath(kFlaconCtlPath)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(scoped_platform_wrapper_,
              Execute(IsCommandLine(BuildExpectedCommandLine()), _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<1>(plist_content),
                               testing::Return(true)));

  auto signals = GetSignals();
  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, "agent_id_123");
  EXPECT_TRUE(signals->customer_id.empty());
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_OnlyCustomerId) {
  InitializeClient();

  const auto& plist_content = GetPlistContent(std::nullopt, "CUSTOMER_ID_456");

  EXPECT_CALL(scoped_platform_wrapper_,
              PathExists(base::FilePath(kFlaconCtlPath)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(scoped_platform_wrapper_,
              Execute(IsCommandLine(BuildExpectedCommandLine()), _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<1>(plist_content),
                               testing::Return(true)));

  auto signals = GetSignals();
  ASSERT_TRUE(signals);
  EXPECT_TRUE(signals->agent_id.empty());
  EXPECT_EQ(signals->customer_id, "customer_id_456");
}

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile_NoId) {
  InitializeClient();

  const auto& plist_content = GetPlistContent(std::nullopt, std::nullopt);

  EXPECT_CALL(scoped_platform_wrapper_,
              PathExists(base::FilePath(kFlaconCtlPath)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(scoped_platform_wrapper_,
              Execute(IsCommandLine(BuildExpectedCommandLine()), _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<1>(plist_content),
                               testing::Return(true)));

  auto signals = GetSignals();
  EXPECT_FALSE(signals);
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace device_signals
