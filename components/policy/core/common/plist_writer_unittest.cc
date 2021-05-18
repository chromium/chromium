// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/plist_writer.h"

#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kPlistHeaderXML[] = "<?xml version=\"1.0\"?>";
const char kPlistHeaderVersion[] = "<plist>";
const char kPlistFooter[] = "</plist>\n";

#define PLIST_NEWLINE "\n"

}  // namespace

class PlistWriterTest : public testing::Test {
 public:
  PlistWriterTest();
  ~PlistWriterTest() override;

  void SetUp() override;

  std::string header_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlistWriterTest);
};

PlistWriterTest::PlistWriterTest() {}

PlistWriterTest::~PlistWriterTest() {}

void PlistWriterTest::SetUp() {
  header_ = base::StrCat(
      {kPlistHeaderXML, PLIST_NEWLINE, kPlistHeaderVersion, PLIST_NEWLINE});
}

TEST_F(PlistWriterTest, BasicTypes) {
  std::string output_plist;

  // Test empty dict.
  EXPECT_TRUE(PlistWrite(base::DictionaryValue(), &output_plist));
  EXPECT_EQ(base::StrCat({header_, " <dict/>", PLIST_NEWLINE, kPlistFooter}),
            output_plist);

  // Test empty list.
  EXPECT_TRUE(PlistWrite(base::ListValue(), &output_plist));
  EXPECT_EQ(base::StrCat({header_, " <array/>", PLIST_NEWLINE, kPlistFooter}),
            output_plist);

  // Test integer values.
  EXPECT_TRUE(PlistWrite(base::Value(42), &output_plist));
  EXPECT_EQ(base::StrCat({header_, " <integer>42</integer>", PLIST_NEWLINE,
                          kPlistFooter}),
            output_plist);

  // Test boolean values.
  EXPECT_TRUE(PlistWrite(base::Value(true), &output_plist));
  EXPECT_EQ(base::StrCat({header_, " <true/>", PLIST_NEWLINE, kPlistFooter}),
            output_plist);

  // Test string values.
  EXPECT_TRUE(PlistWrite(base::Value("foo"), &output_plist));
  EXPECT_EQ(base::StrCat({header_, " <string>foo</string>", PLIST_NEWLINE,
                          kPlistFooter}),
            output_plist);
}

TEST_F(PlistWriterTest, NestedTypes) {
  std::string output_plist;

  // Writer unittests like empty list/dict nesting,
  // list list nesting, etc.
  base::DictionaryValue root_dict;
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  std::unique_ptr<base::DictionaryValue> inner_dict(
      new base::DictionaryValue());
  inner_dict->SetInteger("inner int", 10);
  list->Append(std::move(inner_dict));
  list->Append(std::make_unique<base::ListValue>());
  list->AppendBoolean(false);
  root_dict.Set("list", std::move(list));

  EXPECT_TRUE(PlistWrite(root_dict, &output_plist));
  EXPECT_EQ(base::StrCat({header_,       " <dict>",
                          PLIST_NEWLINE, "  <key>list</key>",
                          PLIST_NEWLINE, "  <array>",
                          PLIST_NEWLINE, "   <dict>",
                          PLIST_NEWLINE, "    <key>inner int</key>",
                          PLIST_NEWLINE, "    <integer>10</integer>",
                          PLIST_NEWLINE, "   </dict>",
                          PLIST_NEWLINE, "   <array/>",
                          PLIST_NEWLINE, "   <false/>",
                          PLIST_NEWLINE, "  </array>",
                          PLIST_NEWLINE, " </dict>",
                          PLIST_NEWLINE, kPlistFooter}),
            output_plist);
}

TEST_F(PlistWriterTest, KeysWithPeriods) {
  std::string output_plist;

  base::DictionaryValue period_dict;
  period_dict.SetKey("a.b", base::Value(3));
  period_dict.SetKey("c", base::Value(2));
  std::unique_ptr<base::DictionaryValue> period_dict2(
      new base::DictionaryValue());
  period_dict2->SetKey("g.h.i.j", base::Value(1));
  period_dict.SetKey("d.e.f",
                     base::Value::FromUniquePtrValue(std::move(period_dict2)));
  EXPECT_TRUE(PlistWrite(period_dict, &output_plist));
  EXPECT_EQ(base::StrCat({header_,       " <dict>",
                          PLIST_NEWLINE, "  <key>a.b</key>",
                          PLIST_NEWLINE, "  <integer>3</integer>",
                          PLIST_NEWLINE, "  <key>c</key>",
                          PLIST_NEWLINE, "  <integer>2</integer>",
                          PLIST_NEWLINE, "  <key>d.e.f</key>",
                          PLIST_NEWLINE, "  <dict>",
                          PLIST_NEWLINE, "   <key>g.h.i.j</key>",
                          PLIST_NEWLINE, "   <integer>1</integer>",
                          PLIST_NEWLINE, "  </dict>",
                          PLIST_NEWLINE, " </dict>",
                          PLIST_NEWLINE, kPlistFooter}),
            output_plist);
}

}  // namespace policy
