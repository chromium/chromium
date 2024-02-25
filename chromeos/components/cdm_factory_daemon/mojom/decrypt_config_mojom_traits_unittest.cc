// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/mojom/decrypt_config_mojom_traits.h"

#include "media/base/encryption_pattern.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(DecryptConfigStructTraitsTest, ConvertEncryptionPattern) {
  auto input = media::EncryptionPattern(22, 42);
  std::vector<uint8_t> data =
      chromeos::cdm::mojom::EncryptionPattern::Serialize(&input);

  media::EncryptionPattern output;
  EXPECT_TRUE(chromeos::cdm::mojom::EncryptionPattern::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(input.crypt_byte_block(), output.crypt_byte_block());
  EXPECT_EQ(input.skip_byte_block(), output.skip_byte_block());
}

TEST(DecryptConfigStructTraitsTest, ConvertSubsampleEntry) {
  auto input = media::SubsampleEntry(22, 42);
  std::vector<uint8_t> data =
      chromeos::cdm::mojom::SubsampleEntry::Serialize(&input);

  media::SubsampleEntry output;
  EXPECT_TRUE(chromeos::cdm::mojom::SubsampleEntry::Deserialize(std::move(data),
                                                                &output));
  EXPECT_EQ(input.clear_bytes, output.clear_bytes);
  EXPECT_EQ(input.cypher_bytes, output.cypher_bytes);
}

TEST(DecryptConfigStructTraitsTest, ConvertDecryptConfig) {
  std::unique_ptr<media::DecryptConfig> input =
      std::make_unique<media::DecryptConfig>(
          media::EncryptionScheme::kCbcs, "FAKEKEY",
          std::string(media::DecryptConfig::kDecryptionKeySize, '1'),
          std::vector<media::SubsampleEntry>({media::SubsampleEntry(1, 3)}),
          std::make_optional<media::EncryptionPattern>(22, 42));

  std::vector<uint8_t> data =
      chromeos::cdm::mojom::DecryptConfig::Serialize(&input);

  std::unique_ptr<media::DecryptConfig> output;
  EXPECT_TRUE(chromeos::cdm::mojom::DecryptConfig::Deserialize(std::move(data),
                                                               &output));
  EXPECT_EQ(input->encryption_scheme(), output->encryption_scheme());
  EXPECT_EQ(input->key_id(), output->key_id());
  EXPECT_EQ(input->iv(), output->iv());
  EXPECT_EQ(input->subsamples().size(), output->subsamples().size());
  EXPECT_EQ(input->subsamples()[0].clear_bytes,
            output->subsamples()[0].clear_bytes);
  EXPECT_EQ(input->subsamples()[0].cypher_bytes,
            output->subsamples()[0].cypher_bytes);
  EXPECT_EQ(input->encryption_pattern(), output->encryption_pattern());
}

}  // namespace chromeos