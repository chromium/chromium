// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/test/ios/test_utils.h"

#import <Foundation/Foundation.h>

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// A protocol to test OCMock with.
@protocol FakeProtocol <NSObject>

// An arbitrary method whose parameter is a NSString*.
- (void)methodWithNSString:(NSString*)string;

// An arbitrary method whose parameter is a std::string*.
- (void)methodWithSTDStringPointer:(std::string*)string;

// OCMock can only mock/stub methods whose parameters are pointers or scalars
// (int, char, float...). A method such as
// `- (void)methodWithString:(std::string)string;`
// could not be mocked.

@end

class TestOCMockHelper : public PlatformTest {
 public:
  TestOCMockHelper() : PlatformTest() {
    fake_ = OCMProtocolMock(@protocol(FakeProtocol));
  }

  ~TestOCMockHelper() override { EXPECT_OCMOCK_VERIFY(fake_); }

 protected:
  id<FakeProtocol> fake_;
};

// Checks that andCompareObjectAtIndex accepts the expected argument.
TEST_F(TestOCMockHelper, TestAndCompareObjectAtIndex) {
  std::string string = "foo";

  // Set up the expectation.
  OCMExpect(
      [fake_ methodWithSTDStringPointer:ios::OCM::AnyPointer<std::string>()])
      .andCompareObjectAtIndex(std::string("foo"), 0);
  //  .andCompareObjectAtIndex("foo", 0) would fail because `"foo"` inferred
  //  type is `const char[4]` and not `std::string`, hence not equal.

  // Call the mocked method.
  [fake_ methodWithSTDStringPointer:&string];
}

// Checks that andAssignStructParameterToVariable assigns the received value to
// a variable.
TEST_F(TestOCMockHelper, TestAndAssignStructParameterToVariable) {
  std::string string = "foo";
  __block std::string* received_parameter_pointer;

  // Set up the expectation.
  OCMExpect(
      [fake_ methodWithSTDStringPointer:ios::OCM::AnyPointer<std::string>()])
      .andAssignStructParameterToVariable(received_parameter_pointer, 0);

  // Call the mocked method.
  [fake_ methodWithSTDStringPointer:&string];

  // Checks that `received_parameter_pointer` was assigned to the address of
  // `string`.
  EXPECT_EQ(received_parameter_pointer, &string);
}

// Checks that andAssignStructParameterAtAddressToVariable assigns the received
// value to a variable.
TEST_F(TestOCMockHelper, TestAndAssignStructParameterAtAddressToVariable) {
  std::string string = "foo";
  __block std::string received_parameter;

  // Set up the expectation.
  OCMExpect(
      [fake_ methodWithSTDStringPointer:ios::OCM::AnyPointer<std::string>()])
      .andAssignStructParameterAtAddressToVariable(received_parameter, 0);

  // Call the mocked method.
  [fake_ methodWithSTDStringPointer:&string];

  // Checked that received_parameter was set to "foo".
  EXPECT_EQ(received_parameter, "foo");
}

// Checks that AssignValueToVariable assigns the received value to a variable.
TEST_F(TestOCMockHelper, TestAssignValueToVariable) {
  NSMutableString* ns_string = [[NSMutableString alloc] initWithString:@"foo"];
  __block NSMutableString* received_parameter;

  // Set up the expectation.
  OCMExpect(
      [fake_ methodWithNSString:AssignValueToVariable(received_parameter)]);

  // Call the mocked method.
  [fake_ methodWithNSString:ns_string];

  // Checks that ns_string and received_parameter are the same object.
  EXPECT_EQ(received_parameter, ns_string);
}

// Checks that CopyValueToVariable copies the received value to a variable.
TEST_F(TestOCMockHelper, TestCopyValueToVariable) {
  NSMutableString* ns_string = [[NSMutableString alloc] initWithString:@"foo"];
  __block NSMutableString* received_parameter;

  // Set up the expectation.
  OCMExpect([fake_ methodWithNSString:CopyValueToVariable(received_parameter)]);

  // Call the mocked method.
  [fake_ methodWithNSString:ns_string];

  // Checks that ns_string and received_parameter are different but equal
  // object.
  EXPECT_NSEQ(received_parameter, ns_string);
  EXPECT_NE(received_parameter, ns_string);
}

// Checks that andCallBlockWithParameterAtIndex assigns the received value to a
// variable.
TEST_F(TestOCMockHelper, TestAndCallBlockWithParameterAtIndex) {
  std::string string = "foo";
  __block BOOL block_called = NO;

  // Set up the expectation.
  OCMExpect(
      [fake_ methodWithSTDStringPointer:ios::OCM::AnyPointer<std::string>()])
      .andCallBlockWithParameterAtIndex(
          std::string, 0, ^(std::string* received_parameter_pointer) {
            // Expects received_parameter_pointer to be the address of string.
            EXPECT_EQ(*received_parameter_pointer, "foo");
            block_called = YES;
          });

  // Call the mocked method.
  [fake_ methodWithSTDStringPointer:&string];

  EXPECT_TRUE(block_called);
}
