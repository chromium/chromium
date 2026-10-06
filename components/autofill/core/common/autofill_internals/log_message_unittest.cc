// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_internals/log_message.h"

#include <optional>

#include "base/json/json_writer.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::Optional;

TEST(LogMessage, Serialization) {
  LogBuffer buffer;
  buffer << LogMessage::kParsedForms;
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(
          R"({"attributes":{"class":"log-message","message":"ParsedForms"},)"
          R"("children":[{"type":"text","value":"Parsed forms:"}],)"
          R"("type":"element","value":"div"})")));
}

}  // namespace
}  // namespace autofill
