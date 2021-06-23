// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <sstream>

#include "base/macros.h"
#include "components/network_hints/renderer/dns_prefetch_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

// Single threaded tests of DnsQueue functionality.

namespace network_hints {

class DnsQueueTest : public testing::Test {
};

// Define a helper class that does Push'es and Pop's of numbers.
// This makes it easy to test a LOT of reads, and keep the expected Pop
// value in sync with the Push value.
class DnsQueueSequentialTester {
 public:
  DnsQueueSequentialTester(DnsQueue& buffer,
                           int32_t read_counter = 0,
                           int32_t write_counter = 0);

  // Return of false means buffer was full, or would not take entry.
  bool Push(void);  // Push the string value of next number.

  // Return of false means buffer returned wrong value.
  bool Pop(void);  // Validate string value of next read.

 private:
  DnsQueue* buffer_;
  int32_t read_counter_;   // expected value of next read string.
  int32_t write_counter_;  // Numerical value to write next string.
  DISALLOW_COPY_AND_ASSIGN(DnsQueueSequentialTester);
};

DnsQueueSequentialTester::DnsQueueSequentialTester(DnsQueue& buffer,
                                                   int32_t read_counter,
                                                   int32_t write_counter)
    : buffer_(&buffer),
      read_counter_(read_counter),
      write_counter_(write_counter) {}

bool DnsQueueSequentialTester::Push(void) {
  std::ostringstream value;
  value << write_counter_;

  // Exercise both write methods intermittently.
  DnsQueue::PushResult result = (write_counter_ % 2) ?
       buffer_->Push(value.str().c_str(), value.str().size()) :
       buffer_->Push(value.str());
  if (DnsQueue::SUCCESSFUL_PUSH == result)
    write_counter_++;
  return DnsQueue::OVERFLOW_PUSH != result;
}

bool DnsQueueSequentialTester::Pop(void) {
  std::string string;
  if (buffer_->Pop(&string)) {
    std::ostringstream expected_value;
    expected_value << read_counter_++;
    EXPECT_STREQ(expected_value.str().c_str(), string.c_str())
        << "Pop did not match write for value " << read_counter_;
    return true;
  }
  return false;
}


TEST(DnsQueueTest, BufferUseCheck) {
  // Use a small buffer so we can see that we can't write a string as soon as it
  // gets longer than one less than the buffer size.  The extra empty character
  // is used to keep read and write pointers from overlapping when buffer is
  // full.  This shows the buffer size can constrain writes (and we're not
  // scribbling all over memory).
  const int buffer_size = 3;  // Just room for 2 digts plus '\0' plus blank.
  std::string string;
  DnsQueue buffer(buffer_size);
  DnsQueueSequentialTester tester(buffer);

  EXPECT_FALSE(tester.Pop()) << "Pop from empty buffer succeeded";

  int i;
  for (i = 0; i < 102; i++) {
    if (!tester.Push())
      break;  // String was too large.
    EXPECT_TRUE(tester.Pop()) << "Unable to read back data " << i;
    EXPECT_FALSE(buffer.Pop(&string))
                << "read from empty buffer not flagged";
  }

  EXPECT_GE(i, 100) << "Can't write 2 digit strings in 4 character buffer";
  EXPECT_LT(i, 101) << "We wrote 3 digit strings into a 4 character buffer";
}

TEST(DnsQueueTest, SubstringUseCheck) {
  // Verify that only substring is written/read.
  const int buffer_size = 100;
  const char big_string[] = "123456789";
  std::string string;
  DnsQueue buffer(buffer_size);

  EXPECT_FALSE(buffer.Pop(&string)) << "Initial buffer not empty";

  EXPECT_EQ(DnsQueue::SUCCESSFUL_PUSH, buffer.Push(big_string, 3))
      << "Can't write string";
  EXPECT_EQ(DnsQueue::SUCCESSFUL_PUSH, buffer.Push(big_string, 0))
      << "Can't write null string";
  EXPECT_EQ(DnsQueue::SUCCESSFUL_PUSH, buffer.Push(big_string, 5))
      << "Can't write string";

  EXPECT_TRUE(buffer.Pop(&string)) << "Filled buffer marked as empty";
  EXPECT_STREQ(string.c_str(), "123") << "Can't read actual data";
  EXPECT_TRUE(buffer.Pop(&string)) << "Filled buffer marked as empty";
  EXPECT_STREQ(string.c_str(), "") << "Can't read null string";
  EXPECT_TRUE(buffer.Pop(&string)) << "Filled buffer marked as empty";
  EXPECT_STREQ(string.c_str(), "12345") << "Can't read actual data";

  EXPECT_FALSE(buffer.Pop(&string))
              << "read from empty buffer not flagged";
}

TEST(DnsQueueTest, SizeCheck) {
  // Verify that size is correctly accounted for in buffer.
  const int buffer_size = 100;
  std::string input_string = "Hello";
  std::string string;
  DnsQueue buffer(buffer_size);

  EXPECT_EQ(0U, buffer.Size());
  EXPECT_FALSE(buffer.Pop(&string));
  EXPECT_EQ(DnsQueue::SUCCESSFUL_PUSH, buffer.Push(input_string));
  EXPECT_EQ(1U, buffer.Size());
  EXPECT_EQ(DnsQueue::SUCCESSFUL_PUSH, buffer.Push("Hi There"));
  EXPECT_EQ(2U, buffer.Size());
  EXPECT_TRUE(buffer.Pop(&string));
  EXPECT_EQ(1U, buffer.Size());
  EXPECT_TRUE(buffer.Pop(&string));
  EXPECT_EQ(0U, buffer.Size());
  EXPECT_EQ(DnsQueue::SUCCESSFUL_PUSH, buffer.Push(input_string));
  EXPECT_EQ(1U, buffer.Size());

  // Check to see that the first string, if repeated, is discarded.
  EXPECT_EQ(DnsQueue::REDUNDANT_PUSH, buffer.Push(input_string));
  EXPECT_EQ(1U, buffer.Size());
}

TEST(DnsQueueTest, FillThenEmptyCheck) {
  // Use a big buffer so we'll get a bunch of writes in.
  // This tests to be sure the buffer holds many strings.
  // We also make sure they all come out intact.
  const size_t buffer_size = 1000;
  size_t byte_usage_counter = 1;  // Separation character between pointer.
  DnsQueue buffer(buffer_size);
  DnsQueueSequentialTester tester(buffer);

  size_t write_success;
  for (write_success = 0; write_success < buffer_size; write_success++) {
    if (!tester.Push())
      break;
    EXPECT_EQ(buffer.Size(), write_success + 1);
    if (write_success > 99)
      byte_usage_counter += 4;  // 3 digit plus '\0'.
    else if (write_success > 9)
      byte_usage_counter += 3;  // 2 digits plus '\0'.
    else
      byte_usage_counter += 2;  // Digit plus '\0'.
  }
  EXPECT_LE(byte_usage_counter, buffer_size)
      << "Written data exceeded buffer size";
  EXPECT_GE(byte_usage_counter, buffer_size - 4)
      << "Buffer does not appear to have filled";

  EXPECT_GE(write_success, 10U) << "Couldn't even write 10 one digit strings "
      "in " << buffer_size << " byte buffer";


  while (1) {
    if (!tester.Pop())
      break;
    write_success--;
  }
  EXPECT_EQ(write_success, 0U) << "Push and Pop count were different";

  EXPECT_FALSE(tester.Pop()) << "Read from empty buffer succeeded";
}

TEST(DnsQueueTest, ClearCheck) {
  // Use a big buffer so we'll get a bunch of writes in.
  const size_t buffer_size = 1000;
  DnsQueue buffer(buffer_size);
  std::string string("ABC");
  DnsQueueSequentialTester tester(buffer);

  size_t write_success;
  for (write_success = 0; write_success < buffer_size; write_success++) {
    if (!tester.Push())
      break;
    EXPECT_EQ(buffer.Size(), write_success + 1);
  }

  buffer.Clear();
  EXPECT_EQ(buffer.Size(), 0U);

  size_t write_success2;
  for (write_success2 = 0; write_success2 < buffer_size; write_success2++) {
    if (!tester.Push())
      break;
    EXPECT_EQ(buffer.Size(), write_success2 + 1);
  }

  for (; write_success2 > 0; write_success2--) {
    EXPECT_EQ(buffer.Size(), write_success2);
    EXPECT_TRUE(buffer.Pop(&string));
  }

  EXPECT_EQ(buffer.Size(), 0U);
  buffer.Clear();
  EXPECT_EQ(buffer.Size(), 0U);
}

TEST(DnsQueueTest, WrapOnVariousSubstrings) {
  // Use a prime number for the allocated buffer size so that we tend
  // to exercise all possible edge conditions (in circular text buffer).
  // Once we're over 10 writes, all our strings are 2 digits long,
  // with a '\0' terminator added making 3 characters per write.
  // Since 3 is relatively prime to 23, we'll soon wrap (about
  // every 6 writes).  Hence after 18 writes, we'll have tested all
  // edge conditions.  We'll first do this where we empty the buffer
  // after each write, and then again where there are some strings
  // still in the buffer after each write.
  const int prime_number = 23;
  // Circular buffer needs an extra extra space to distinguish full from empty.
  const int buffer_size = prime_number - 1;
  DnsQueue buffer(buffer_size);
  DnsQueueSequentialTester tester(buffer);

  // First test empties between each write. Second loop
  // has writes for each pop.  Third has three pushes per pop.
  // Third has two items pending during each write.
  for (int j = 0; j < 3; j++) {
    // Each group does 30 tests, which is more than 10+18
    // which was needed to get into the thorough testing zone
    // mentioned above.
    for (int i = 0; i < 30; i++) {
      EXPECT_TRUE(tester.Push()) << "write failed with only " << j
                                    << " blocks in buffer";
      EXPECT_TRUE(tester.Pop()) << "Unable to read back data ";
    }
    EXPECT_TRUE(tester.Push());
  }

  // Read back the accumulated 3 extra blocks.
  EXPECT_TRUE(tester.Pop());
  EXPECT_TRUE(tester.Pop());
  EXPECT_TRUE(tester.Pop());
  EXPECT_FALSE(tester.Pop());
}

}  // namespace network_hints
