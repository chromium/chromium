// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/quick_pair_process.h"

#include <cstdint>
#include <vector>

#include "ash/quick_pair/fast_pair_handshake/fast_pair_encryption.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "chromeos/ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::array<uint8_t, 16> kResponseBytes = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};

const std::array<uint8_t, 16> kAesKeyBytes = {
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6,
    0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

const std::array<uint8_t, 16> kPasskeyBytes = {
    0x02, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D,
    0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6};

class FakeQuickPairProcessManager
    : public ash::quick_pair::QuickPairProcessManager {
 public:
  class FakeQuickPairProcessReference
      : public ash::quick_pair::QuickPairProcessManager::ProcessReference {
   public:
    FakeQuickPairProcessReference(
        mojo::SharedRemote<ash::quick_pair::mojom::FastPairDataParser>
            data_parser_remote)
        : data_parser_remote_(data_parser_remote) {}

    ~FakeQuickPairProcessReference() override = default;

    const mojo::SharedRemote<ash::quick_pair::mojom::FastPairDataParser>&
    GetFastPairDataParser() const override {
      return data_parser_remote_;
    }

   private:
    mojo::SharedRemote<ash::quick_pair::mojom::FastPairDataParser>
        data_parser_remote_;
  };

  FakeQuickPairProcessManager() {
    data_parser_ = std::make_unique<ash::quick_pair::FastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());

    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_enviornment_.GetMainThreadTaskRunner());
  }

  ~FakeQuickPairProcessManager() override = default;

  std::unique_ptr<ProcessReference> GetProcessReference(
      ProcessStoppedCallback on_process_stopped_callback) override {
    return std::make_unique<FakeQuickPairProcessReference>(data_parser_remote_);
  }

 private:
  base::test::SingleThreadTaskEnvironment task_enviornment_;
  mojo::SharedRemote<ash::quick_pair::mojom::FastPairDataParser>
      data_parser_remote_;
  mojo::PendingRemote<ash::quick_pair::mojom::FastPairDataParser>
      fast_pair_data_parser_;
  std::unique_ptr<ash::quick_pair::FastPairDataParser> data_parser_;
  ProcessStoppedCallback on_process_stopped_callback_;
};

}  // namespace

namespace ash {
namespace quick_pair {
namespace quick_pair_process {

class QuickPairProcessTest : public testing::Test {};

TEST_F(QuickPairProcessTest,
       GetHexModelIdFromServiceData_NoValueIfNoProcessManagerSet) {
  GetHexModelIdFromServiceData(
      std::vector<uint8_t>(),
      base::BindLambdaForTesting([](const std::optional<std::string>& result) {
        EXPECT_FALSE(result.has_value());
      }),
      base::DoNothing());
}

TEST_F(QuickPairProcessTest,
       ParseDecryptedResponse_NoValueIfNoProcessManagerSet) {
  ParseDecryptedResponse(
      std::vector<uint8_t>(), std::vector<uint8_t>(),
      base::BindLambdaForTesting(
          [](const std::optional<DecryptedResponse>& result) {
            EXPECT_FALSE(result.has_value());
          }),
      base::DoNothing());
}

TEST_F(QuickPairProcessTest,
       ParseDecryptedPasskey_NoValueIfNoProcessManagerSet) {
  ParseDecryptedPasskey(std::vector<uint8_t>(), std::vector<uint8_t>(),
                        base::BindLambdaForTesting(
                            [](const std::optional<DecryptedPasskey>& result) {
                              EXPECT_FALSE(result.has_value());
                            }),
                        base::DoNothing());
}

TEST_F(QuickPairProcessTest,
       ParseNotDiscoverableAdvertisement_NoValueIfNoProcessManagerSet) {
  ParseNotDiscoverableAdvertisement(
      /*service_data=*/std::vector<uint8_t>(), /*address=*/"",
      base::BindLambdaForTesting(
          [](const std::optional<NotDiscoverableAdvertisement>& result) {
            EXPECT_FALSE(result.has_value());
          }),
      base::DoNothing());
}

TEST_F(QuickPairProcessTest,
       ParseMessageStreamMessages_NoValueIfNoProcessManagerSet) {
  ParseMessageStreamMessages(
      std::vector<uint8_t>(),
      base::BindLambdaForTesting(
          [](std::vector<mojom::MessageStreamMessagePtr> messages) {
            EXPECT_TRUE(messages.empty());
          }),
      base::DoNothing());
}

TEST_F(QuickPairProcessTest, ParseDecryptedResponse_ValueIfProcessManagerSet) {
  auto process_manager = std::make_unique<FakeQuickPairProcessManager>();
  quick_pair_process::SetProcessManager(process_manager.get());

  const std::array<uint8_t, 16> encrypted_response_bytes =
      fast_pair_encryption::EncryptBytes(kAesKeyBytes, kResponseBytes);
  base::RunLoop run_loop;
  ParseDecryptedResponse(
      std::vector<uint8_t>(kAesKeyBytes.begin(), kAesKeyBytes.end()),
      std::vector<uint8_t>(encrypted_response_bytes.begin(),
                           encrypted_response_bytes.end()),
      base::BindLambdaForTesting(
          [&run_loop](const std::optional<DecryptedResponse>& result) {
            EXPECT_TRUE(result.has_value());
            run_loop.Quit();
          }),
      base::DoNothing());
  run_loop.Run();
}

TEST_F(QuickPairProcessTest, ParseDecryptedPasskey_ValueIfProcessManagerSet) {
  auto process_manager = std::make_unique<FakeQuickPairProcessManager>();
  quick_pair_process::SetProcessManager(process_manager.get());

  const std::array<uint8_t, 16> encrypted_passkey_bytes =
      fast_pair_encryption::EncryptBytes(kAesKeyBytes, kPasskeyBytes);
  base::RunLoop run_loop;
  ParseDecryptedPasskey(
      std::vector<uint8_t>(kAesKeyBytes.begin(), kAesKeyBytes.end()),
      std::vector<uint8_t>(encrypted_passkey_bytes.begin(),
                           encrypted_passkey_bytes.end()),
      base::BindLambdaForTesting(
          [&run_loop](const std::optional<DecryptedPasskey>& result) {
            EXPECT_TRUE(result.has_value());
            run_loop.Quit();
          }),
      base::DoNothing());
  run_loop.Run();
}

}  // namespace quick_pair_process
}  // namespace quick_pair
}  // namespace ash
