// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/mojom/cdm_key_information_mojom_traits.h"

#include "media/base/cdm_key_information.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(CdmKeyInformationStructTraitsTest, ConvertCdmKeyInformation) {
  auto input = std::make_unique<media::CdmKeyInformation>(
      "key_id", media::CdmKeyInformation::KeyStatus::USABLE, 23);
  std::vector<uint8_t> data =
      chromeos::cdm::mojom::CdmKeyInformation::Serialize(&input);

  std::unique_ptr<media::CdmKeyInformation> output;
  EXPECT_TRUE(chromeos::cdm::mojom::CdmKeyInformation::Deserialize(
      std::move(data), &output));
  EXPECT_EQ(input->key_id, output->key_id);
  EXPECT_EQ(input->status, output->status);
  EXPECT_EQ(input->system_code, output->system_code);
}

}  // namespace chromeos