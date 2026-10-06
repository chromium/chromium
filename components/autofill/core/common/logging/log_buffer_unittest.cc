// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/logging/log_buffer.h"

#include <optional>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::Optional;

TEST(LogBuffer, JSONSerializeString) {
  LogBuffer buffer;
  buffer << "<foo><!--\"";
  // JSON takes care of serializing the <, we don't want &lt; as that would then
  // be escaped twice.
  EXPECT_THAT(base::WriteJson(*buffer.RetrieveResult()),
              Optional(std::string(
                  R"({"type":"text","value":"\u003Cfoo>\u003C!--\""})")));
}

TEST(LogBuffer, JSONSerializeString16) {
  LogBuffer buffer;
  buffer << u"<foo><!--\"";
  // JSON takes care of serializing the <, we don't want &lt; as that would then
  // be escaped twice.
  EXPECT_THAT(base::WriteJson(*buffer.RetrieveResult()),
              Optional(std::string(
                  R"({"type":"text","value":"\u003Cfoo>\u003C!--\""})")));
}

TEST(LogBuffer, SupportNumbers) {
  LogBuffer buffer;
  buffer << 42;
  EXPECT_THAT(base::WriteJson(*buffer.RetrieveResult()),
              Optional(std::string(R"({"type":"text","value":"42"})")));
}

TEST(LogBuffer, SanitizeURLs) {
  LogBuffer buffer;
  buffer << GURL("https://user:pw@www.example.com:80/foo?bar=1#foo");
  // Verify that the url gets scrubbed.
  EXPECT_THAT(base::WriteJson(*buffer.RetrieveResult()),
              Optional(std::string(
                  R"({"type":"text","value":"https://www.example.com:80/"})")));
}

TEST(LogBuffer, Empty) {
  LogBuffer buffer;
  EXPECT_FALSE(buffer.RetrieveResult());
}

TEST(LogBuffer, UnclosedTag) {
  LogBuffer buffer;
  buffer << Tag{"foo"};
  EXPECT_THAT(base::WriteJson(*buffer.RetrieveResult()),
              Optional(std::string(R"({"type":"element","value":"foo"})")));
}

TEST(LogBuffer, ClosedTag) {
  LogBuffer buffer;
  buffer << Tag{"foo"} << CTag{};
  EXPECT_THAT(base::WriteJson(*buffer.RetrieveResult()),
              Optional(std::string(R"({"type":"element","value":"foo"})")));
}

TEST(LogBuffer, NestedTag) {
  LogBuffer buffer;
  buffer << Tag{"foo"} << Tag{"bar"} << CTag{} << CTag{};
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(R"({"children":[{"type":"element","value":"bar"}],)"
                           R"("type":"element","value":"foo"})")));
}

TEST(LogBuffer, NestedTagClosingTooOften) {
  LogBuffer buffer;
  buffer << Tag{"foo"} << Tag{"bar"} << CTag{} << CTag{} << CTag{};
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(R"({"children":[{"type":"element","value":"bar"}],)"
                           R"("type":"element","value":"foo"})")));
}

TEST(LogBuffer, NestedTagClosingNotAtAll) {
  LogBuffer buffer;
  buffer << Tag{"foo"} << Tag{"bar"};
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(R"({"children":[{"type":"element","value":"bar"}],)"
                           R"("type":"element","value":"foo"})")));
}

TEST(LogBuffer, NestedTagWithAttributes) {
  LogBuffer buffer;
  buffer << Tag{"foo"} << Tag{"bar"} << Attrib{"b1", "1"} << Attrib{"b2", "2"}
         << CTag{} << Attrib{"f1", "1"};
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(
          R"({"attributes":{"f1":"1"},"children":[)"
          R"({"attributes":{"b1":"1","b2":"2"},"type":"element","value":"bar"})"
          R"(],"type":"element","value":"foo"})")));
}

TEST(LogBuffer, DivWithBr) {
  LogBuffer buffer;
  buffer << Tag{"div"} << "foo" << Br{} << "bar" << CTag{};
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(
          R"({"children":[{"type":"text","value":"foo"},)"
          R"({"type":"element","value":"br"},{"type":"text","value":"bar"}],)"
          R"("type":"element","value":"div"})")));
}

TEST(LogBuffer, CoalesceStrings) {
  LogBuffer buffer;
  buffer << Tag{"div"} << "foo"
         << "bar";
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(R"({"children":[{"type":"text","value":"foobar"}],)"
                           R"("type":"element","value":"div"})")));
}

struct SampleObject {
  int x;
  std::string y;
};

LogBuffer& operator<<(LogBuffer& buf, const SampleObject& value) {
  buf << Tag{"table"};
  buf << Tag{"tr"} << Tag{"td"} << "x" << CTag{"td"} << Tag{"td"} << value.x
      << CTag{"td"} << CTag{"tr"};
  buf << Tag{"tr"} << Tag{"td"} << "y" << CTag{"td"} << Tag{"td"} << value.y
      << CTag{"td"} << CTag{"tr"};
  buf << CTag{"table"};
  return buf;
}

TEST(LogBuffer, CanStreamCustomObjects) {
  LogBuffer buffer;
  SampleObject o{42, "foobar<!--"};
  buffer << o;
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(
          R"({"children":[)"                                      // table
          /**/ R"({"children":[)"                                 // tr
          /****/ R"({"children":[{"type":"text","value":"x"}],)"  // td
          /******/ R"("type":"element","value":"td"},)"
          /****/ R"({"children":[{"type":"text","value":"42"}],)"  // td
          /******/ R"("type":"element","value":"td"}],)"
          /****/ R"("type":"element","value":"tr"},)"  // continuation of tr
          /**/ R"({"children":[)"                      // tr
          /****/ R"({"children":[{"type":"text","value":"y"}],)"
          /******/ R"("type":"element","value":"td"},)"
          /****/ R"({"children":[{"type":"text","value":"foobar\u003C!--"}],)"
          /******/ R"("type":"element","value":"td"}],)"
          /**/ R"("type":"element","value":"tr"}],"type":"element",)"
          /****/ R"("value":"table"})")));
}

TEST(LogBuffer, LogTableRowBuffer) {
  LogBuffer expected;
  expected << Tag{"table"};
  expected << Tag{"tr"} << Attrib{"class", "awesome"} << Tag{"td"} << "Foo"
           << CTag{"td"} << Tag{"td"} << "Bar" << CTag{"td"} << CTag{"tr"};
  expected << CTag{"table"};
  LogBuffer actual;
  actual << Tag{"table"};
  actual << Tr{} << Attrib{"class", "awesome"} << "Foo"
         << "Bar";
  actual << CTag{"table"};
  EXPECT_EQ(*expected.RetrieveResult(), *actual.RetrieveResult());
}

TEST(LogBuffer, CreateFragment) {
  LogBuffer buffer;
  buffer << "foo" << Br{} << "bar";
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(
          R"({"children":[{"type":"text","value":"foo"},)"
          R"({"type":"element","value":"br"},{"type":"text","value":"bar"}],)"
          R"("type":"fragment"})")));
}

TEST(LogBuffer, AppendFragmentByInlining) {
  LogBuffer tmp_buffer;
  tmp_buffer << "foo" << Br{} << "bar";
  LogBuffer buffer;
  buffer << std::move(tmp_buffer);
  EXPECT_THAT(
      base::WriteJson(*buffer.RetrieveResult()),
      Optional(std::string(
          R"({"children":[{"type":"text","value":"foo"},)"
          R"({"type":"element","value":"br"},{"type":"text","value":"bar"}],)"
          R"("type":"fragment"})")));
}

TEST(LogBuffer, AppendSingleElementBuffer) {
  LogBuffer tmp_buffer;
  tmp_buffer << "foo";
  LogBuffer buffer;
  buffer << std::move(tmp_buffer);
  EXPECT_THAT(base::WriteJson(*buffer.RetrieveResult()),
              Optional(std::string(R"({"type":"text","value":"foo"})")));
}

TEST(LogBuffer, Highlight) {
  LogBuffer expected;
  expected << "foo" << Tag{"b"} << "bar" << CTag{"b"} << "baz";
  LogBuffer actual;
  actual << HighlightValue("foobarbaz", "bar");
  EXPECT_EQ(*expected.RetrieveResult(), *actual.RetrieveResult());
}

TEST(LogBuffer, HighlightAtStart) {
  LogBuffer expected;
  expected << Tag{"b"} << "foo" << CTag{"b"} << "barbaz";
  LogBuffer actual;
  actual << HighlightValue("foobarbaz", "foo");
  EXPECT_EQ(*expected.RetrieveResult(), *actual.RetrieveResult());
}

TEST(LogBuffer, HighlightAtEnd) {
  LogBuffer expected;
  expected << "foobar" << Tag{"b"} << "baz" << CTag{"b"};
  LogBuffer actual;
  actual << HighlightValue("foobarbaz", "baz");
  EXPECT_EQ(*expected.RetrieveResult(), *actual.RetrieveResult());
}

TEST(LogBuffer, HighlightEmpty) {
  LogBuffer expected;
  expected << "foobarbaz";
  LogBuffer actual;
  actual << HighlightValue("foobarbaz", "");
  EXPECT_EQ(*expected.RetrieveResult(), *actual.RetrieveResult());
}

TEST(LogBuffer, HighlightNotFound) {
  LogBuffer expected;
  expected << "foobarbaz";
  LogBuffer actual;
  actual << HighlightValue("foobarbaz", "notfound");
  EXPECT_EQ(*expected.RetrieveResult(), *actual.RetrieveResult());
}

TEST(LogBuffer, HighlightEmptyString) {
  LogBuffer expected;
  expected << "";
  LogBuffer actual;
  actual << HighlightValue("", "");
  EXPECT_EQ(expected.RetrieveResult(), actual.RetrieveResult());
}

}  // namespace
}  // namespace autofill
