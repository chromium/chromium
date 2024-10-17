// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_switches.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/user_annotations/user_annotations_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_annotations {

namespace {

using ::base::test::EqualsProto;
using ::testing::UnorderedElementsAre;

struct Entry {
  size_t entry_id;
  std::string key;
  std::string value;
};

TEST(UserAnnotationsSwitchesTest, OverrideFormsAnnotations) {
  const std::vector<Entry>& response_upserted_entries = {
      {0, "label", "whatever"},
      {0, "nolabel", "value"},
  };
  optimization_guide::proto::FormsAnnotationsResponse response;
  for (const auto& entry : response_upserted_entries) {
    optimization_guide::proto::UserAnnotationsEntry* new_entry =
        response.add_upserted_entries();
    new_entry->set_entry_id(entry.entry_id);
    new_entry->set_key(entry.key);
    new_entry->set_value(entry.value);
  }

  std::string encoded_annotations;
  response.SerializeToString(&encoded_annotations);
  encoded_annotations = base::Base64Encode(encoded_annotations);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFormsAnnotationsOverride, encoded_annotations);

  auto output = switches::ParseFormsAnnotationsFromCommandLine();

  EXPECT_TRUE(output.has_value());
  EXPECT_THAT(output.value(), EqualsProto(response));
}

TEST(UserAnnotationsSwitchesTest, OverrideFormsAnnotationsBadFormat) {
  std::string encoded_annotations = "Not a proto";
  encoded_annotations = base::Base64Encode(encoded_annotations);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kFormsAnnotationsOverride, encoded_annotations);

  auto output = switches::ParseFormsAnnotationsFromCommandLine();

  EXPECT_FALSE(output.has_value());
}

}  // namespace
}  // namespace user_annotations
