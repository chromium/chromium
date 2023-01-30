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
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/common/common_types.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

namespace {

constexpr base::FilePath::CharType kFakeFileName[] =
    FILE_PATH_LITERAL("data.zta");

}  // namespace

class CrowdStrikeClientTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  void CreateFakeFileWithContent(const std::string& file_content) {
    ASSERT_TRUE(base::WriteFile(GetDataFilePath(), file_content));
  }

  base::FilePath GetDataFilePath() {
    return scoped_temp_dir_.GetPath().Append(kFakeFileName);
  }

  absl::optional<CrowdStrikeSignals> GetSignals() {
    auto client = CrowdStrikeClient::CreateForTesting(GetDataFilePath());

    base::test::TestFuture<absl::optional<CrowdStrikeSignals>> future;
    client->GetIdentifiers(future.GetCallback());

    return future.Get();
  }

  void ValidateHistogram(absl::optional<SignalsParsingError> error) {
    static constexpr char kCrowdStrikeErrorHistogram[] =
        "Enterprise.DeviceSignals.Collection.CrowdStrike.Error";
    if (error) {
      histogram_tester_.ExpectUniqueSample(kCrowdStrikeErrorHistogram,
                                           error.value(), 1);
    } else {
      histogram_tester_.ExpectTotalCount(kCrowdStrikeErrorHistogram, 0);
    }
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  base::HistogramTester histogram_tester_;
};

TEST_F(CrowdStrikeClientTest, Identifiers_NoFile) {
  EXPECT_FALSE(GetSignals());

  // No value logged, not having the file available is not considered a failure.
  ValidateHistogram(absl::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_EmptyFile) {
  CreateFakeFileWithContent("");
  EXPECT_FALSE(GetSignals());

  // No value logged, having an empty file is not considered a failure.
  ValidateHistogram(absl::nullopt);
}

TEST_F(CrowdStrikeClientTest, Identifiers_NotJwt) {
  CreateFakeFileWithContent("some.random.content");
  EXPECT_FALSE(GetSignals());
  ValidateHistogram(SignalsParsingError::kJsonParsingFailed);
}

TEST_F(CrowdStrikeClientTest, Identifiers_MaxDataSize) {
  std::string content("a", 33 * 1024);
  CreateFakeFileWithContent(content);
  EXPECT_FALSE(GetSignals());
  ValidateHistogram(SignalsParsingError::kHitMaxDataSize);
}

TEST_F(CrowdStrikeClientTest, Identifiers_DecodingFailed) {
  CreateFakeFileWithContent("some.random%%.content");
  EXPECT_FALSE(GetSignals());
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
  EXPECT_FALSE(GetSignals());
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
  EXPECT_FALSE(GetSignals());
  ValidateHistogram(SignalsParsingError::kMissingRequiredProperty);
}

TEST_F(CrowdStrikeClientTest, Identifiers_Success) {
  static constexpr char kFakeJwtZtaContent[] =
      "eyJhbGciOiJSUzI1NiIsImtpZCI6InYxIiwidHlwIjoiSldUIn0."
      "eyJhc3Nlc3NtZW50Ijp7Im92ZXJhbGwiOjU1LCJvcyI6NTAsInNlbnNvcl9jb25maWciOjYw"
      "LCJ2ZXJzaW9uIjoiIn0sImV4cCI6MTYxMjg5NTk2NywiaWF0IjoxNjEyODkyMzY3LCJwbGF0"
      "Zm9ybSI6IldpbmRvd3MgMTAiLCJzZXJpYWxfbnVtYmVyIjoic2VyaWFsTnVtYmVyMTIzIiwi"
      "c3ViIjoiYmVlZmJlZWZiZWVmYmVlZmJlZWZiZWVmYmVlZjExMTEiLCJ0eXAiOiJjcm93ZHN0"
      "cmlrZS16dGErand0In0."
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
  static constexpr char kExpectedAgentId[] = "beefbeefbeefbeefbeefbeefbeef1111";
  CreateFakeFileWithContent(kFakeJwtZtaContent);
  auto signals = GetSignals();

  ASSERT_TRUE(signals);
  EXPECT_EQ(signals->agent_id, kExpectedAgentId);

  ValidateHistogram(absl::nullopt);
}

}  // namespace device_signals
