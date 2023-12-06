// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"

#include <stddef.h>

#include <iterator>
#include <optional>

#include "ash/quick_pair/common/fast_pair/fast_pair_service_data_creator.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "chromeos/ash/services/quick_pair/public/cpp/not_discoverable_advertisement.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace {

constexpr int kNotDiscoverableAdvHeader = 0b00000110;
constexpr int kAccountKeyFilterHeader = 0b01100000;
constexpr int kAccountKeyFilterNoNotificationHeader = 0b01100010;
constexpr int kSaltHeader = 0b00010001;
constexpr int kSaltHeader2Bytes = 0b00100001;
constexpr int kSaltHeader3Bytes = 0b00110001;
const std::vector<uint8_t> kSaltBytes = {0x01};
const std::vector<uint8_t> kLargeSaltBytes = {0xC7, 0xC8};
const std::vector<uint8_t> kDeviceAddressBytes = {17, 18, 19, 20, 21, 22};
constexpr int kBatteryHeader = 0b00110011;
constexpr int kBatterHeaderNoNotification = 0b00110100;

const std::string kModelId = "112233";
const std::string kAccountKeyFilter = "112233445566";
const std::string kSalt = "01";
const std::string kLargeSalt = "C7C8";
const std::string kInvalidSalt = "C7C8C9";
const std::string kBattery = "01048F";
const std::string kDeviceAddress = "11:12:13:14:15:16";

std::vector<uint8_t> aes_key_bytes = {0xA0, 0xBA, 0xF0, 0xBB, 0x95, 0x1F,
                                      0xF7, 0xB6, 0xCF, 0x5E, 0x3F, 0x45,
                                      0x61, 0xC3, 0x32, 0x1D};

std::vector<uint8_t> EncryptBytes(const std::vector<uint8_t>& bytes) {
  AES_KEY aes_key;
  AES_set_encrypt_key(aes_key_bytes.data(), aes_key_bytes.size() * 8, &aes_key);
  uint8_t encrypted_bytes[16];
  AES_encrypt(bytes.data(), encrypted_bytes, &aes_key);
  return std::vector<uint8_t>(std::begin(encrypted_bytes),
                              std::end(encrypted_bytes));
}

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairDataParserTest : public testing::Test {
 public:
  void SetUp() override {
    data_parser_ = std::make_unique<FastPairDataParser>(
        remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::FastPairDataParser> remote_;
  std::unique_ptr<FastPairDataParser> data_parser_;
};

TEST_F(FastPairDataParserTest, DecryptResponseUnsuccessfully) {
  std::vector<uint8_t> response_bytes = {/*message_type=*/0x02,
                                         /*address_bytes=*/0x02,
                                         0x03,
                                         0x04,
                                         0x05,
                                         0x06,
                                         0x07,
                                         /*salt=*/0x08,
                                         0x09,
                                         0x0A,
                                         0x0B,
                                         0x0C,
                                         0x0D,
                                         0x0E,
                                         0x0F,
                                         0x00};
  std::vector<uint8_t> encrypted_bytes = EncryptBytes(response_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](const std::optional<DecryptedResponse>& response) {
        EXPECT_FALSE(response.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedResponse(aes_key_bytes, encrypted_bytes,
                                       std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, DecryptResponseSuccessfully) {
  std::vector<uint8_t> response_bytes;

  // Message type.
  uint8_t message_type = 0x01;
  response_bytes.push_back(message_type);

  // Address bytes.
  std::array<uint8_t, 6> address_bytes = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  base::ranges::copy(address_bytes, std::back_inserter(response_bytes));

  // Random salt
  std::array<uint8_t, 9> salt = {0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                 0x0D, 0x0E, 0x0F, 0x00};
  base::ranges::copy(salt, std::back_inserter(response_bytes));

  std::vector<uint8_t> encrypted_bytes = EncryptBytes(response_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop, &address_bytes,
       &salt](const std::optional<DecryptedResponse>& response) {
        EXPECT_TRUE(response.has_value());
        EXPECT_EQ(response->message_type,
                  FastPairMessageType::kKeyBasedPairingResponse);
        EXPECT_EQ(response->address_bytes, address_bytes);
        EXPECT_EQ(response->salt, salt);
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedResponse(aes_key_bytes, encrypted_bytes,
                                       std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, DecryptPasskeyUnsuccessfully) {
  std::vector<uint8_t> passkey_bytes = {/*message_type=*/0x04,
                                        /*passkey=*/0x02,
                                        0x03,
                                        0x04,
                                        /*salt=*/0x05,
                                        0x06,
                                        0x07,
                                        0x08,
                                        0x09,
                                        0x0A,
                                        0x0B,
                                        0x0C,
                                        0x0D,
                                        0x0E,
                                        0x0F,
                                        0x0E};
  std::vector<uint8_t> encrypted_bytes = EncryptBytes(passkey_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](const std::optional<DecryptedPasskey>& passkey) {
        EXPECT_FALSE(passkey.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes,
                                      std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, DecryptSeekerPasskeySuccessfully) {
  std::vector<uint8_t> passkey_bytes;
  // Message type.
  uint8_t message_type = 0x02;
  passkey_bytes.push_back(message_type);

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes.push_back(passkey >> 16);
  passkey_bytes.push_back(passkey >> 8);
  passkey_bytes.push_back(passkey);

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  base::ranges::copy(salt, std::back_inserter(passkey_bytes));

  std::vector<uint8_t> encrypted_bytes = EncryptBytes(passkey_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop, &passkey,
       &salt](const std::optional<DecryptedPasskey>& decrypted_passkey) {
        EXPECT_TRUE(decrypted_passkey.has_value());
        EXPECT_EQ(decrypted_passkey->message_type,
                  FastPairMessageType::kSeekersPasskey);
        EXPECT_EQ(decrypted_passkey->passkey, passkey);
        EXPECT_EQ(decrypted_passkey->salt, salt);
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes,
                                      std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, DecryptProviderPasskeySuccessfully) {
  std::vector<uint8_t> passkey_bytes;
  // Message type.
  uint8_t message_type = 0x03;
  passkey_bytes.push_back(message_type);

  // Passkey bytes.
  uint32_t passkey = 5;
  passkey_bytes.push_back(passkey >> 16);
  passkey_bytes.push_back(passkey >> 8);
  passkey_bytes.push_back(passkey);

  // Random salt
  std::array<uint8_t, 12> salt = {0x08, 0x09, 0x0A, 0x08, 0x09, 0x0E,
                                  0x0A, 0x0C, 0x0D, 0x0E, 0x05, 0x02};
  base::ranges::copy(salt, std::back_inserter(passkey_bytes));

  std::vector<uint8_t> encrypted_bytes = EncryptBytes(passkey_bytes);

  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop, &passkey,
       &salt](const std::optional<DecryptedPasskey>& decrypted_passkey) {
        EXPECT_TRUE(decrypted_passkey.has_value());
        EXPECT_EQ(decrypted_passkey->message_type,
                  FastPairMessageType::kProvidersPasskey);
        EXPECT_EQ(decrypted_passkey->passkey, passkey);
        EXPECT_EQ(decrypted_passkey->salt, salt);
        run_loop.Quit();
      });

  data_parser_->ParseDecryptedPasskey(aes_key_bytes, encrypted_bytes,
                                      std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_Empty) {
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(
      std::vector<uint8_t>(), kDeviceAddress, std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseNotDiscoverableAdvertisement_NoApplicibleData) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseNotDiscoverableAdvertisement_AccountKeyFilter) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kSaltBytes, advertisement->salt);
        EXPECT_TRUE(advertisement->show_ui);
        EXPECT_FALSE(advertisement->battery_notification.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseNotDiscoverableAdvertisement_AccountKeyFilterNoNotification) {
  std::vector<uint8_t> bytes =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader(kAccountKeyFilterNoNotificationHeader)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(kSaltHeader)
          .AddExtraField(kSalt)
          .Build()
          ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kSaltBytes, advertisement->salt);
        EXPECT_FALSE(advertisement->show_ui);
        EXPECT_FALSE(advertisement->battery_notification.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_WrongVersion) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00100000)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseNotDiscoverableAdvertisement_ZeroLengthExtraField) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField("")
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_WrongType) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(0b01100001)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_SaltTwoBytes) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader2Bytes)
                                   .AddExtraField(kLargeSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kLargeSaltBytes, advertisement->salt);
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_SaltTooLarge) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader3Bytes)
                                   .AddExtraField(kInvalidSalt)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_FALSE(advertisement.has_value());
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_Battery) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kSaltHeader)
                                   .AddExtraField(kSalt)
                                   .AddExtraFieldHeader(kBatteryHeader)
                                   .AddExtraField(kBattery)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kSaltBytes, advertisement->salt);
        EXPECT_TRUE(advertisement->show_ui);
        EXPECT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_TRUE(advertisement->battery_notification->show_ui);
        EXPECT_FALSE(
            advertisement->battery_notification->left_bud_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->left_bud_info.percentage,
                  1);
        EXPECT_FALSE(
            advertisement->battery_notification->right_bud_info.is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->right_bud_info.percentage, 4);
        EXPECT_TRUE(advertisement->battery_notification->case_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->case_info.percentage,
                  15);
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_MissingSalt) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(kNotDiscoverableAdvHeader)
                                   .SetModelId(kModelId)
                                   .AddExtraFieldHeader(kAccountKeyFilterHeader)
                                   .AddExtraField(kAccountKeyFilter)
                                   .AddExtraFieldHeader(kBatteryHeader)
                                   .AddExtraField(kBattery)
                                   .Build()
                                   ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kDeviceAddressBytes, advertisement->salt);
        EXPECT_TRUE(advertisement->show_ui);
        EXPECT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_TRUE(advertisement->battery_notification->show_ui);
        EXPECT_FALSE(
            advertisement->battery_notification->left_bud_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->left_bud_info.percentage,
                  1);
        EXPECT_FALSE(
            advertisement->battery_notification->right_bud_info.is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->right_bud_info.percentage, 4);
        EXPECT_TRUE(advertisement->battery_notification->case_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->case_info.percentage,
                  15);
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseNotDiscoverableAdvertisement_BatteryNoUi) {
  std::vector<uint8_t> bytes =
      FastPairServiceDataCreator::Builder()
          .SetHeader(kNotDiscoverableAdvHeader)
          .SetModelId(kModelId)
          .AddExtraFieldHeader(kAccountKeyFilterHeader)
          .AddExtraField(kAccountKeyFilter)
          .AddExtraFieldHeader(kSaltHeader)
          .AddExtraField(kSalt)
          .AddExtraFieldHeader(kBatterHeaderNoNotification)
          .AddExtraField(kBattery)
          .Build()
          ->CreateServiceData();
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](
          const std::optional<NotDiscoverableAdvertisement>& advertisement) {
        EXPECT_TRUE(advertisement.has_value());
        EXPECT_EQ(kAccountKeyFilter,
                  base::HexEncode(advertisement->account_key_filter));
        EXPECT_EQ(kSaltBytes, advertisement->salt);
        EXPECT_TRUE(advertisement->show_ui);
        EXPECT_TRUE(advertisement->battery_notification.has_value());
        EXPECT_FALSE(advertisement->battery_notification->show_ui);
        EXPECT_FALSE(
            advertisement->battery_notification->left_bud_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->left_bud_info.percentage,
                  1);
        EXPECT_FALSE(
            advertisement->battery_notification->right_bud_info.is_charging);
        EXPECT_EQ(
            advertisement->battery_notification->right_bud_info.percentage, 4);
        EXPECT_TRUE(advertisement->battery_notification->case_info.is_charging);
        EXPECT_EQ(advertisement->battery_notification->case_info.percentage,
                  15);
        run_loop.Quit();
      });

  data_parser_->ParseNotDiscoverableAdvertisement(bytes, kDeviceAddress,
                                                  std::move(callback));

  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_EnableSilenceMode) {
  std::vector<uint8_t> bytes = {// Bluetooth event
                                /*mesage_group=*/0x01,
                                // Enable silence mode
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_enable_silence_mode());
        EXPECT_TRUE(messages[0]->get_enable_silence_mode());
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_SilenceMode_AdditionalData) {
  std::vector<uint8_t> bytes = {// Bluetooth event
                                /*mesage_group=*/0x01,
                                // Enable silence mode
                                /*mesage_code=*/0x01,
                                // Invalid additional data
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x08};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_DisableSilenceMode) {
  std::vector<uint8_t> bytes = {// Bluetooth event
                                /*mesage_group=*/0x01,
                                // Disable silence mode
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_enable_silence_mode());
        EXPECT_FALSE(messages[0]->get_enable_silence_mode());
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_BluetoothInvalidMessageCode) {
  std::vector<uint8_t> bytes = {// Bluetooth event
                                /*mesage_group=*/0x01,
                                // Unknown message code
                                /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_CompanionAppLogBufferFull) {
  std::vector<uint8_t> bytes = {// Companion app event
                                /*mesage_group=*/0x02,
                                // Log buffer full
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_companion_app_log_buffer_full());
        EXPECT_TRUE(messages[0]->get_companion_app_log_buffer_full());
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_CompanionAppInvalidMessageCode) {
  std::vector<uint8_t> bytes = {// Companion app event
                                /*mesage_group=*/0x02,
                                // Unknown message code
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_CompanionAppLogBufferFull_AdditionalData) {
  std::vector<uint8_t> bytes = {// Companion App event
                                /*mesage_group=*/0x02,
                                // Log buffer full
                                /*mesage_code=*/0x01,
                                // Invalid additional data
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x08};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_ModelId) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Model ID
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x03,
                                // Model ID value
                                /*additional_data=*/0xAA, 0xBB, 0xCC};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_model_id());
        EXPECT_EQ(messages[0]->get_model_id(), "AABBCC");
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_BleAddress) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // BLE address updated
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x06,
                                // BLE Address value
                                /*additional_data=*/0xAA, 0xBB, 0xCC, 0xDD,
                                0xEE, 0xFF};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_ble_address_update());
        EXPECT_EQ(messages[0]->get_ble_address_update(), "AA:BB:CC:DD:EE:FF");
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_WrongAdditionalDataSize) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // BLE address updated
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x08,
                                // BLE address values are only 6 bytes
                                /*additional_data=*/0xAA, 0xBB, 0xCC, 0xDD,
                                0xEE, 0xFF};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_BatteryNotification) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Battery updated
                                /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00, 0x03,
                                // Right, Left, Case values
                                /*additional_data=*/0x57, 0x41, 0x7F};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_battery_update());
        EXPECT_EQ(messages[0]->get_battery_update()->left_bud_info->percentage,
                  87);
        EXPECT_EQ(messages[0]->get_battery_update()->right_bud_info->percentage,
                  65);
        EXPECT_EQ(messages[0]->get_battery_update()->case_info->percentage, -1);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_RemainingBatteryTime) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Remaining battery time
                                /*mesage_code=*/0x04,
                                /*additional_data_length=*/0x00, 0x01,
                                // Remaining battery time value
                                /*additional_data=*/0xF0};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_remaining_battery_time());
        EXPECT_EQ(messages[0]->get_remaining_battery_time(), 240);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_RemainingBatteryTime_2BytesAdditionalData) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Remaining battery time
                                /*mesage_code=*/0x04,
                                /*additional_data_length=*/0x00, 0x02,
                                // Support for uint16
                                /*additional_data=*/0x01, 0x0F};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_remaining_battery_time());
        EXPECT_EQ(messages[0]->get_remaining_battery_time(), 271);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_DeviceInfoInvalidMessageCode) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Unknown message code
                                /*mesage_code=*/0x09,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_ModelIdInvalidLength) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Model ID
                                /*mesage_code=*/0x01,
                                // Expected 3 bytes
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_BleAddressUpdateInvalidLength) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // BLE address update
                                /*mesage_code=*/0x02,
                                // Expected 6 bytes
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_BatteryUpdateInvalidLength) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Battery update
                                /*mesage_code=*/0x03,
                                // Expected 3 bytes
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_RemainingBatteryInvalidLength) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Remaining battery
                                /*mesage_code=*/0x04,
                                // Expected 1 or 2 bytes
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_ActiveComponentsInvalidLength) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Active components response
                                /*mesage_code=*/0x06,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_ActiveComponents) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Active components response
                                /*mesage_code=*/0x06,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x03};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_active_components_byte());
        EXPECT_EQ(messages[0]->get_active_components_byte(), 0x03);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_AndroidPlatform) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Platform
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x01, 0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_sdk_version());
        EXPECT_EQ(messages[0]->get_sdk_version(), 28);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_PlatformInvalidLength) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Platform type
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00, 0x00};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_InvalidPlatform) {
  std::vector<uint8_t> bytes = {// Device information
                                /*mesage_group=*/0x03,
                                // Platform type
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00, 0x02,
                                // Only supports Android of type `0x01`
                                /*additional_data=*/0x02, 0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_RingDeviceNoTimeout) {
  std::vector<uint8_t> bytes = {// Device action
                                /*mesage_group=*/0x04,
                                // Ring
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_ring_device_event());
        EXPECT_EQ(messages[0]->get_ring_device_event()->ring_device_byte, 0x01);
        EXPECT_EQ(messages[0]->get_ring_device_event()->timeout_in_seconds, -1);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_RingDeviceTimeout) {
  std::vector<uint8_t> bytes = {// Device action
                                /*mesage_group=*/0x04,
                                // Ring
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x01, 0x3C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_ring_device_event());
        EXPECT_EQ(messages[0]->get_ring_device_event()->ring_device_byte, 0x01);
        EXPECT_EQ(messages[0]->get_ring_device_event()->timeout_in_seconds, 60);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_RingInvalidLength) {
  std::vector<uint8_t> bytes = {// Device action
                                /*mesage_group=*/0x04,
                                // Ring
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x03,
                                /*additional_data=*/0x02, 0x1C, 0x02};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_RingInvalidMessageCode) {
  std::vector<uint8_t> bytes = {// Device action
                                /*mesage_group=*/0x04,
                                // Unknown message code
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x02, 0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_Ack) {
  std::vector<uint8_t> bytes = {// Acknowledgements
                                /*mesage_group=*/0xFF,
                                // ACK
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x04, 0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_acknowledgement());
        EXPECT_EQ(messages[0]->get_acknowledgement()->action_message_code,
                  0x01);
        EXPECT_EQ(messages[0]->get_acknowledgement()->action_message_group,
                  mojom::MessageGroup::kDeviceActionEvent);
        EXPECT_EQ(messages[0]->get_acknowledgement()->acknowledgement,
                  mojom::Acknowledgement::kAck);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_Nak) {
  std::vector<uint8_t> bytes = {// Acknowledgements
                                /*mesage_group=*/0xFF,
                                // NAK
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x03,
                                /*additional_data=*/0x00, 0x04, 0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_acknowledgement());
        EXPECT_EQ(messages[0]->get_acknowledgement()->action_message_code,
                  0x01);
        EXPECT_EQ(messages[0]->get_acknowledgement()->action_message_group,
                  mojom::MessageGroup::kDeviceActionEvent);
        EXPECT_EQ(messages[0]->get_acknowledgement()->acknowledgement,
                  mojom::Acknowledgement::kNotSupportedNak);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_AckInvalidMessageCode) {
  std::vector<uint8_t> bytes = {// Acknowledgements
                                /*mesage_group=*/0xFF,
                                // Unknown message code
                                /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x04, 0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_AckInvalidLength) {
  std::vector<uint8_t> bytes = {// Acknowledgements
                                /*mesage_group=*/0xFF,
                                // ACK
                                /*mesage_code=*/0x01,
                                // Expect size 4 for action message group and
                                // corresponding to the ACK
                                /*additional_data_length=*/0x00, 0x03,
                                /*additional_data=*/0x04, 0x01, 0x01};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_NakInvalidLength) {
  std::vector<uint8_t> bytes = {// Acknowledgements
                                /*mesage_group=*/0xFF,
                                // NACK
                                /*mesage_code=*/0x02,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x00, 0x04};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest, ParseMessageStreamMessage_NotEnoughBytes) {
  std::vector<uint8_t> bytes = {0x01, 0x02, 0x03};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_MultipleMessages_Valid) {
  std::vector<uint8_t> bytes = {// Device Action
                                /*mesage_group=*/0x04,
                                // Ring
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x01,

                                // Device Information
                                /*mesage_group=*/0x03,
                                // Platform Type
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x01, 0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 2);
        EXPECT_TRUE(messages[0]->is_ring_device_event());
        EXPECT_EQ(messages[0]->get_ring_device_event()->ring_device_byte, 0x01);
        EXPECT_EQ(messages[0]->get_ring_device_event()->timeout_in_seconds, -1);
        EXPECT_TRUE(messages[1]->is_sdk_version());
        EXPECT_EQ(messages[1]->get_sdk_version(), 28);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_MultipleMessages_ValidInvalid) {
  std::vector<uint8_t> bytes = {// Device Action
                                /*mesage_group=*/0x04,
                                // Ring
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x01,
                                /*additional_data=*/0x01,

                                // Device Information
                                /*mesage_group=*/0x03,
                                // Platform Type
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x02, 0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_ring_device_event());
        EXPECT_EQ(messages[0]->get_ring_device_event()->ring_device_byte, 0x01);
        EXPECT_EQ(messages[0]->get_ring_device_event()->timeout_in_seconds, -1);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_MultipleMessages_Invalid) {
  std::vector<uint8_t> bytes = {// Device Action
                                /*mesage_group=*/0x04,
                                // Ring
                                /*mesage_code=*/0x01,
                                /*additional_data_length=*/0x00, 0x00,

                                // Device Information
                                /*mesage_group=*/0x03,
                                // Platform type
                                /*mesage_code=*/0x08,
                                /*additional_data_length=*/0x00, 0x02,
                                /*additional_data=*/0x02, 0x1C};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 0);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

// Regression test for b/274788634.
TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_InvalidMessageCodeFollowedByBatteryUpdate) {
  std::vector<uint8_t> bytes = {// Device Information
                                /*mesage_group=*/0x03,
                                // Session Nonce
                                /*mesage_code=*/0x0A,
                                /*additional_data_length=*/0x00, 0x08,
                                /*additional_data=*/0x63, 0x19, 0xEC, 0x34,
                                0x5F, 0xB3, 0xEF, 0x90,

                                // Device Information
                                /*mesage_group=*/0x03,
                                // Battery Update
                                /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00, 0x03,
                                /*additional_data=*/0x57, 0x41, 0x7F};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_battery_update());
        EXPECT_EQ(87,
                  messages[0]->get_battery_update()->left_bud_info->percentage);
        EXPECT_EQ(
            65, messages[0]->get_battery_update()->right_bud_info->percentage);
        EXPECT_EQ(-1, messages[0]->get_battery_update()->case_info->percentage);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

// Regression test for b/274788634.
TEST_F(FastPairDataParserTest,
       ParseMessageStreamMessage_InvalidMessageGroupFollowedByBatteryUpdate) {
  std::vector<uint8_t> bytes = {// SASS
                                /*mesage_group=*/0x07,
                                // Connection Status
                                /*mesage_code=*/0x34,
                                /*additional_data_length=*/0x00, 0x0C,
                                /*additional_data=*/0x4E, 0x61, 0xD9, 0x5B,
                                0x50, 0x57, 0x9C, 0x69, 0x3E, 0x6B, 0x6C, 0x74,

                                // Device Information
                                /*mesage_group=*/0x03,
                                // Battery Update
                                /*mesage_code=*/0x03,
                                /*additional_data_length=*/0x00, 0x03,
                                /*additional_data=*/0x57, 0x41, 0x7F};
  base::RunLoop run_loop;
  auto callback = base::BindLambdaForTesting(
      [&run_loop](std::vector<mojom::MessageStreamMessagePtr> messages) {
        EXPECT_EQ(static_cast<int>(messages.size()), 1);
        EXPECT_TRUE(messages[0]->is_battery_update());
        EXPECT_EQ(87,
                  messages[0]->get_battery_update()->left_bud_info->percentage);
        EXPECT_EQ(
            65, messages[0]->get_battery_update()->right_bud_info->percentage);
        EXPECT_EQ(-1, messages[0]->get_battery_update()->case_info->percentage);
        run_loop.Quit();
      });

  data_parser_->ParseMessageStreamMessages(bytes, std::move(callback));
  run_loop.Run();
}

}  // namespace quick_pair
}  // namespace ash
