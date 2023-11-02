// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/message_builder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(MessageBuilderTest, NewLine) {
  MessageBuilder builder;
  builder.NewLine();
  EXPECT_EQ(L"\n", builder.content());

  builder.NewLine().NewLine();
  EXPECT_EQ(L"\n\n\n", builder.content());
}

TEST(MessageBuilderTest, Add) {
  MessageBuilder builder;
  builder.Add(L"abc").Add(L" ").Add("xyz").Add(" ").Add(10);
  EXPECT_EQ(L"abc xyz 10", builder.content());

  builder.Add(" ").Add(true).Add(" ").Add(false);
  EXPECT_EQ(L"abc xyz 10 1 0", builder.content());
}

TEST(MessageBuilderTest, AddLine) {
  MessageBuilder builder;
  builder.AddLine(L"abc", L" ", 10).AddLine("xyz", L" ", false);
  std::wstring expected = L"abc 10\nxyz 0\n";
  EXPECT_EQ(expected, builder.content());

  builder.AddLine(" test ", true).AddLine(false);
  expected += L" test 1\n0\n";
  EXPECT_EQ(expected, builder.content());
}

TEST(MessageBuilderTest, ScopedIndentation) {
  MessageBuilder builder;
  builder.Add(L"*", L"*").NewLine();
  std::wstring expected = L"**\n";
  EXPECT_EQ(expected, builder.content());

  {
    MessageBuilder::ScopedIndent scoped_indent(&builder);
    builder.Add(L"*", L"*").AddLine(L"*");
    expected += L"\t***\n";
    EXPECT_EQ(expected, builder.content());

    {
      auto indent = builder.Indent();
      builder.Add(L"*", L"*", L"*").NewLine();
      expected += L"\t\t***\n";
      EXPECT_EQ(expected, builder.content());
      builder.AddLine(L"*", L"*");
      expected += L"\t\t**\n";
      EXPECT_EQ(expected, builder.content());

      MessageBuilder::ScopedIndent scoped_indent_2(&builder);
      builder.Add(L"*", L"*").NewLine();
      expected += L"\t\t\t**\n";
      EXPECT_EQ(expected, builder.content());
    }

    builder.Add(L"*", L"*").NewLine();
    expected += L"\t**\n";
    EXPECT_EQ(expected, builder.content());
  }

  builder.Add(L"*");
  expected += L"*";
  EXPECT_EQ(expected, builder.content());

  {
    MessageBuilder::ScopedIndent scoped_indent(&builder);
    expected += L"\n";  // Added due to change in indentation level.
    EXPECT_EQ(expected, builder.content());

    builder.Add(L"*", L"*");
    expected += L"\t**";
    EXPECT_EQ(expected, builder.content());
  }

  expected += L"\n";  // Added due to change in indentation level.
  EXPECT_EQ(expected, builder.content());

  builder.AddLine(L"*", L"*");
  expected += L"**\n";
  EXPECT_EQ(expected, builder.content());

  {
    auto scoped_indent = builder.Indent();
    builder.AddLine(L"*", L"*");
    expected += L"\t**\n";
    EXPECT_EQ(expected, builder.content());
  }

  builder.Add(L"*", L"*").Add(L"*", L"*", L"*").NewLine();
  expected += L"*****\n";
  EXPECT_EQ(expected, builder.content());
}

TEST(MessageBuilderTest, AddHeaderLine) {
  MessageBuilder builder;
  builder.AddHeaderLine(L"Header1").AddHeaderLine(L"Header2");
  std::wstring expected = L"Header1:\nHeader2:\n";
  EXPECT_EQ(expected, builder.content());

  MessageBuilder::ScopedIndent scoped_indent(&builder);
  builder.AddHeaderLine(L"Header3");
  expected += L"\tHeader3:\n";
  EXPECT_EQ(expected, builder.content());

  MessageBuilder::ScopedIndent scoped_indent_2(&builder);
  builder.AddHeaderLine(L"Header4").AddHeaderLine(L"Header5");
  expected += L"\t\tHeader4:\n\t\tHeader5:\n";
  EXPECT_EQ(expected, builder.content());
}

TEST(MessageBuilderTest, AddFieldValueLine) {
  MessageBuilder builder;

  builder.AddFieldValueLine(L"Field1", "abc")
      .AddFieldValueLine(L"Field2", L"xyz");
  std::wstring expected = L"Field1: abc\nField2: xyz\n";
  EXPECT_EQ(expected, builder.content());

  MessageBuilder::ScopedIndent scoped_indent(&builder);
  builder.AddFieldValueLine(L"Field3", 10);
  expected += L"\tField3: 10\n";
  EXPECT_EQ(expected, builder.content());

  MessageBuilder::ScopedIndent scoped_indent_2(&builder);
  builder.AddFieldValueLine(L"Field4", true)
      .AddFieldValueLine(L"Field5", false);
  expected += L"\t\tField4: 1\n\t\tField5: 0\n";
  EXPECT_EQ(expected, builder.content());
}

}  // namespace chrome_cleaner
