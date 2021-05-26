// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/cxx17_backports.h"
#include "chrome/test/logging/win/mof_data_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

// A test fixture for Mof parser tests.
class MofDataParserTest : public ::testing::Test {
 protected:
  EVENT_TRACE* MakeEventWithDataOfSize(size_t size);
  template<typename T> EVENT_TRACE* MakeEventWithBlittedValue(T value) {
    EVENT_TRACE* event = MakeEventWithDataOfSize(sizeof(value));
    *reinterpret_cast<T*>(event->MofData) = value;
    return event;
  }
  EVENT_TRACE* MakeEventWithDWORD(DWORD value);
  EVENT_TRACE* MakeEventWithPointerArray(const void* const* pointers,
                                         DWORD size);
  EVENT_TRACE* MakeEventWithString(const char* a_string, size_t length);

  std::vector<uint8_t> buffer_;
};

EVENT_TRACE* MofDataParserTest::MakeEventWithDataOfSize(size_t size) {
  buffer_.assign(sizeof(EVENT_TRACE) + size, 0);
  EVENT_TRACE* event = reinterpret_cast<EVENT_TRACE*>(&buffer_[0]);
  event->MofLength = size;
  event->MofData = &buffer_[sizeof(EVENT_TRACE)];
  return event;
}

EVENT_TRACE* MofDataParserTest::MakeEventWithDWORD(DWORD value) {
  return MakeEventWithBlittedValue(value);
}

EVENT_TRACE* MofDataParserTest::MakeEventWithPointerArray(
    const void* const* pointers,
    DWORD size) {
  EVENT_TRACE* event =
      MakeEventWithDataOfSize(sizeof(DWORD) + sizeof(*pointers) * size);
  *reinterpret_cast<DWORD*>(event->MofData) = size;
  ::memcpy(reinterpret_cast<DWORD*>(event->MofData) + 1, pointers,
           sizeof(*pointers) * size);
  return event;
}

// |length| is the number of bytes to put in (i.e., include the terminator if
// you want one).
EVENT_TRACE* MofDataParserTest::MakeEventWithString(const char* a_string,
                                                    size_t length) {
  EVENT_TRACE* event = MakeEventWithDataOfSize(length);
  ::memcpy(event->MofData, a_string, length);
  return event;
}

// Tests reading a primitive value.  ReadDWORD, ReadInt, and ReadPointer share
// the same implementation, so this test covers all three.
TEST_F(MofDataParserTest, ReadPrimitive) {

  // Read a valid DWORD.
  EVENT_TRACE* event = MakeEventWithDWORD(5);
  {
    DWORD value = 0;
    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_TRUE(parser.ReadDWORD(&value));
    EXPECT_EQ(5UL, value);
    EXPECT_TRUE(parser.empty());
  }

  // Try again if there's insufficient data.
  --(event->MofLength);
  {
    DWORD value = 0;
    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_FALSE(parser.ReadDWORD(&value));
    EXPECT_EQ(0UL, value);
  }
}

// Tests reading an array of pointer-sized values.  These arrays are encoded by
// writing a DWORD item count followed by the items.
TEST_F(MofDataParserTest, ReadPointerArray) {
  const void* const pointers[] = { this, &buffer_ };
  const DWORD array_size = base::size(pointers);

  // Read a valid array of two pointers.
  EVENT_TRACE* event = MakeEventWithPointerArray(&pointers[0], array_size);
  {
    DWORD size = 0;
    const intptr_t* values = NULL;

    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_TRUE(parser.ReadDWORD(&size));
    EXPECT_EQ(array_size, size);
    EXPECT_TRUE(parser.ReadPointerArray(size, &values));
    EXPECT_EQ(0, ::memcmp(&pointers[0], values, sizeof(*values) * size));
    EXPECT_TRUE(parser.empty());
  }

  // Try again if there's insufficient data.
  --(event->MofLength);
  {
    DWORD size = 0;
    const intptr_t* values = NULL;

    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_TRUE(parser.ReadDWORD(&size));
    EXPECT_EQ(array_size, size);
    EXPECT_FALSE(parser.ReadPointerArray(size, &values));
    EXPECT_FALSE(parser.empty());
  }
}

// Tests reading a structure.
TEST_F(MofDataParserTest, ReadStructure) {
  struct Spam {
    int blorf;
    char spiffy;
  };
  const Spam canned_meat = { 47, 'Y' };

  // Read a pointer to a structure.
  EVENT_TRACE* event = MakeEventWithBlittedValue(canned_meat);
  {
    const Spam* value = NULL;
    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_TRUE(parser.ReadStructure(&value));
    EXPECT_EQ(canned_meat.blorf, value->blorf);
    EXPECT_EQ(canned_meat.spiffy, value->spiffy);
    EXPECT_TRUE(parser.empty());
  }

  // Try again if there's insufficient data.
  --(event->MofLength);
  {
    const Spam* value = NULL;
    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_FALSE(parser.ReadStructure(&value));
    EXPECT_FALSE(parser.empty());
  }
}

// Tests reading null-terminated string.
TEST_F(MofDataParserTest, ReadString) {
  const char a_string_nl[] = "sometimes i get lost in my own thoughts.\n";
  const char a_string[] = "sometimes i get lost in my own thoughts.";

  // Read a string with a trailing newline.
  EVENT_TRACE* event =
      MakeEventWithString(a_string_nl, base::size(a_string_nl));
  {
    base::StringPiece value;
    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_TRUE(parser.ReadString(&value));
    EXPECT_EQ(base::StringPiece(&a_string_nl[0], base::size(a_string_nl) - 2),
              value);
    EXPECT_TRUE(parser.empty());
  }

  // Read a string without a trailing newline.
  event = MakeEventWithString(a_string, base::size(a_string));
  {
    base::StringPiece value;
    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_TRUE(parser.ReadString(&value));
    EXPECT_EQ(base::StringPiece(&a_string[0], base::size(a_string) - 1), value);
    EXPECT_TRUE(parser.empty());
  }

  // Try a string that isn't terminated.
  event = MakeEventWithString(a_string, base::size(a_string) - 1);
  {
    base::StringPiece value;
    logging_win::MofDataParser parser(event);
    EXPECT_FALSE(parser.empty());
    EXPECT_FALSE(parser.ReadString(&value));
    EXPECT_FALSE(parser.empty());
  }
}
