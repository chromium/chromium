// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"

#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::Ref;

std::vector<FormFieldData> MakeFields(size_t num_fields) {
  std::vector<FormFieldData> fields;
  fields.reserve(num_fields);
  for (size_t i = 0; i < num_fields; ++i) {
    fields.emplace_back();
    fields.back().set_renderer_id(FieldRendererId(i));
  }
  return fields;
}

// Tests that iterating over the elements works.
TEST(AutofillScannerTest, Advance) {
  std::vector<FormFieldData> fields = MakeFields(4);
  AutofillScanner scanner(fields, [](const FormFieldData&) { return true; });
  EXPECT_FALSE(scanner.IsEnd());
  EXPECT_THAT(scanner.Predecessor(), IsNull());
  EXPECT_THAT(scanner.Cursor(), Ref(fields[0]));
  scanner.Advance();
  EXPECT_FALSE(scanner.IsEnd());
  EXPECT_THAT(scanner.Predecessor(), Pointee(Ref(fields[0])));
  EXPECT_THAT(scanner.Cursor(), Ref(fields[1]));
  scanner.Advance();
  EXPECT_FALSE(scanner.IsEnd());
  EXPECT_THAT(scanner.Predecessor(), Pointee(Ref(fields[1])));
  EXPECT_THAT(scanner.Cursor(), Ref(fields[2]));
  scanner.Advance();
  EXPECT_FALSE(scanner.IsEnd());
  EXPECT_THAT(scanner.Predecessor(), Pointee(Ref(fields[2])));
  EXPECT_THAT(scanner.Cursor(), Ref(fields[3]));
  scanner.Advance();
  EXPECT_TRUE(scanner.IsEnd());
}

// Tests that filtering all but some elements in the middle works.
TEST(AutofillScannerTest, AdvanceWithFilter_SkipAll) {
  std::vector<FormFieldData> fields = MakeFields(8);
  AutofillScanner scanner(fields,
                          [](const FormFieldData& field) { return false; });
  EXPECT_TRUE(scanner.IsEnd());
}

// Tests that filtering all but some elements in the beginning works.
TEST(AutofillScannerTest, AdvanceWithFilter_SkipFirst) {
  std::vector<FormFieldData> fields = MakeFields(4);
  AutofillScanner scanner(fields, [](const FormFieldData& field) {
    return *field.renderer_id() % 2 == 1;
  });
  EXPECT_FALSE(scanner.IsEnd());
  EXPECT_THAT(scanner.Predecessor(), IsNull());
  EXPECT_THAT(scanner.Cursor(), Ref(fields[1]));
  scanner.Advance();
  EXPECT_THAT(scanner.Predecessor(), Pointee(Ref(fields[1])));
  EXPECT_THAT(scanner.Cursor(), Ref(fields[3]));
  scanner.Advance();
  EXPECT_TRUE(scanner.IsEnd());
}

// Tests that filtering all but some elements in the end works.
TEST(AutofillScannerTest, AdvanceWithFilter_SkipLast) {
  std::vector<FormFieldData> fields = MakeFields(4);
  AutofillScanner scanner(fields, [](const FormFieldData& field) {
    return *field.renderer_id() % 2 == 0;
  });
  EXPECT_FALSE(scanner.IsEnd());
  EXPECT_THAT(scanner.Predecessor(), IsNull());
  EXPECT_THAT(scanner.Cursor(), Ref(fields[0]));
  scanner.Advance();
  EXPECT_THAT(scanner.Predecessor(), Pointee(Ref(fields[0])));
  EXPECT_THAT(scanner.Cursor(), Ref(fields[2]));
  scanner.Advance();
  EXPECT_TRUE(scanner.IsEnd());
}

// Tests that filtering all but some elements in the middle works.
TEST(AutofillScannerTest, AdvanceWithFilter_SkipFirstAndLast) {
  std::vector<FormFieldData> fields = MakeFields(8);
  AutofillScanner scanner(fields, [](const FormFieldData& field) {
    return 3 <= *field.renderer_id() && *field.renderer_id() <= 5;
  });
  EXPECT_FALSE(scanner.IsEnd());
  EXPECT_THAT(scanner.Predecessor(), IsNull());
  EXPECT_THAT(scanner.Cursor(), Ref(fields[3]));
  scanner.Advance();
  EXPECT_THAT(scanner.Predecessor(), Pointee(Ref(fields[3])));
  EXPECT_THAT(scanner.Cursor(), Ref(fields[4]));
  scanner.Advance();
  EXPECT_THAT(scanner.Predecessor(), Pointee(Ref(fields[4])));
  EXPECT_THAT(scanner.Cursor(), Ref(fields[5]));
  scanner.Advance();
  EXPECT_TRUE(scanner.IsEnd());
}

// Tests restoring the position.
TEST(AutofillScannerTest, Restore) {
  std::vector<FormFieldData> fields = MakeFields(3);
  AutofillScanner scanner(fields, [](const FormFieldData&) { return true; });
  scanner.Advance();
  EXPECT_THAT(scanner.Cursor(), Ref(fields[1]));
  AutofillScanner::Position position = scanner.GetPosition();
  scanner.Advance();
  scanner.Advance();
  scanner.Restore(position);
  EXPECT_THAT(scanner.Cursor(), Ref(fields[1]));
}

// Tests that the offset skips filtered fields.
TEST(AutofillScannerTest, GetOffset) {
  std::vector<FormFieldData> fields = MakeFields(8);
  AutofillScanner scanner(fields, [](const FormFieldData& field) {
    return *field.renderer_id() % 2 == 1;
  });
  scanner.Advance();
  scanner.Advance();
  scanner.Advance();
  EXPECT_EQ(scanner.GetOffset(), 3u);
}

}  // namespace
}  // namespace autofill
