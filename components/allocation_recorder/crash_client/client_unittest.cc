// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_client/client.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/debug/allocation_trace.h"
#include "components/allocation_recorder/internal/internal.h"
#include "components/crash/core/common/crash_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/annotation_list.h"

using allocation_recorder::internal::kAnnotationName;
using allocation_recorder::internal::kAnnotationType;
using base::debug::tracer::AllocationTraceRecorder;
using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;

namespace allocation_recorder::crash_client {

class AllocationStackRecorderCrashClientTest : public ::testing::Test {
 protected:
  AllocationTraceRecorder& GetRecorder() const { return *recorder_; }

  // Verify that RegisterRecorderWithCrashpad correctly registered the address
  // of the recorder with the Crashpad annotations framework.
  static AssertionResult RecorderIsRegistered(
      const AllocationTraceRecorder& recorder);

  // Verify that we have either no annotation inserted at all, or if we have an
  // annotation inserted, check that it's not enabled.
  static AssertionResult NoRecorderIsRegistered();

  static AssertionResult IsExpectedAnnotation(
      const AllocationTraceRecorder* const recorder);

 private:
  // Find all annotations which match the type and name of the
  // allocation_recorder's annotation.
  static std::vector<const crashpad::Annotation*>
  FindAllocationRecorderAnnotations(
      const crashpad::AnnotationList& annotation_list);

  const std::unique_ptr<AllocationTraceRecorder> recorder_ =
      std::make_unique<AllocationTraceRecorder>();
};

AssertionResult AllocationStackRecorderCrashClientTest::RecorderIsRegistered(
    const AllocationTraceRecorder& recorder) {
  // On Android, RegisterRecorderWithCrashpad additionally inserts the recorder
  // into the list of allowed memory addresses. Unfortunately, the list which
  // the recorder is being added to is a private implementation detail and not
  // accessible. Hence, no verification for this part.

  return IsExpectedAnnotation(&recorder);
}

AssertionResult
AllocationStackRecorderCrashClientTest::NoRecorderIsRegistered() {
  return IsExpectedAnnotation(nullptr);
}

std::vector<const crashpad::Annotation*>
AllocationStackRecorderCrashClientTest::FindAllocationRecorderAnnotations(
    const crashpad::AnnotationList& annotation_list) {
  std::vector<const crashpad::Annotation*> annotations;

  for (auto* annotation : annotation_list) {
    if (annotation && (annotation->type() == kAnnotationType) &&
        (annotation->name() &&
         (0 == strcmp(annotation->name(), kAnnotationName)))) {
      annotations.push_back(annotation);
    }
  }

  return annotations;
}

AssertionResult AllocationStackRecorderCrashClientTest::IsExpectedAnnotation(
    const AllocationTraceRecorder* const recorder) {
  auto* const annotation_list = crashpad::AnnotationList::Get();
  if (annotation_list == nullptr) {
    // If a recorder is registered, there _must_ also be an annotation list
    // registered. If no recorder is registered on the other side, having no
    // annotation list is accepted outcome, but we have nothing left to check in
    // this case.
    return recorder ? AssertionFailure() << "No annotation list found."
                    : AssertionSuccess();
  }

  // Depending on the sequence of the tests the annotation may already have been
  // inserted by a previous test. Therefore, we first find _all_ annotations
  // which have the type and name of a allocation_record's annotation and check
  // the correct number of annotations in a later step.
  const std::vector<const crashpad::Annotation*> annotations =
      FindAllocationRecorderAnnotations(*annotation_list);

  const size_t number_of_annotations = annotations.size();
  if (number_of_annotations == 0ul) {
    return recorder ? (AssertionFailure()
                       << "No annotation of allocation recorder found.")
                    : AssertionSuccess();
  } else if (number_of_annotations > 1ul) {
    return AssertionFailure()
           << "Multiple annotations of allocation recorder "
              "found, but at most one expected. annotations-found="
           << number_of_annotations;
  }

  const crashpad::Annotation* const annotation = annotations.front();

  if (annotation->value() == nullptr) {
    return AssertionFailure() << "The annotation's value is nullptr.";
  }

  if (recorder) {
    EXPECT_TRUE(annotation->is_set());
    EXPECT_EQ(annotation->size(), sizeof(uintptr_t));
    EXPECT_EQ(
        *reinterpret_cast<AllocationTraceRecorder* const*>(annotation->value()),
        recorder);
  } else {
    EXPECT_FALSE(annotation->is_set());
    EXPECT_EQ(annotation->size(), 0ul);
    EXPECT_EQ(
        *reinterpret_cast<AllocationTraceRecorder* const*>(annotation->value()),
        nullptr);
  }

  return AssertionSuccess();
}

// Since the client cannot be torn down completely, we would introduce some
// dependencies between tests if we introduced dedicated tests. Therefore, one
// test verifies the required behaviour.
TEST_F(AllocationStackRecorderCrashClientTest, Verify) {
  auto& recorder = GetRecorder();

  ASSERT_TRUE(NoRecorderIsRegistered());

  RegisterRecorderWithCrashpad(recorder);

  ASSERT_TRUE(RecorderIsRegistered(recorder));

  UnregisterRecorderWithCrashpad();

  ASSERT_TRUE(NoRecorderIsRegistered());
}

TEST_F(AllocationStackRecorderCrashClientTest,
       VerifyReInitializationAfterShutDown) {
  auto& recorder = GetRecorder();
  const std::unique_ptr<AllocationTraceRecorder> another_recorder =
      std::make_unique<AllocationTraceRecorder>();

  ASSERT_TRUE(NoRecorderIsRegistered());

  RegisterRecorderWithCrashpad(recorder);
  ASSERT_TRUE(RecorderIsRegistered(recorder));
  UnregisterRecorderWithCrashpad();

  ASSERT_TRUE(NoRecorderIsRegistered());

  RegisterRecorderWithCrashpad(*another_recorder);
  ASSERT_TRUE(RecorderIsRegistered(*another_recorder));
  UnregisterRecorderWithCrashpad();

  ASSERT_TRUE(NoRecorderIsRegistered());

  RegisterRecorderWithCrashpad(recorder);
  ASSERT_TRUE(RecorderIsRegistered(recorder));
  UnregisterRecorderWithCrashpad();

  ASSERT_TRUE(NoRecorderIsRegistered());
}

#if GTEST_HAS_DEATH_TEST
using AllocationStackRecorderCrashClientDeathTest =
    AllocationStackRecorderCrashClientTest;

TEST_F(AllocationStackRecorderCrashClientDeathTest,
       VerifyDuplicateInitializationWithSameRecorder) {
  auto& recorder = GetRecorder();

  RegisterRecorderWithCrashpad(recorder);

  EXPECT_DEATH(RegisterRecorderWithCrashpad(recorder), "");

  UnregisterRecorderWithCrashpad();
}

TEST_F(AllocationStackRecorderCrashClientDeathTest,
       VerifyDuplicateInitializationWithDifferentRecorder) {
  auto& recorder = GetRecorder();
  const std::unique_ptr<AllocationTraceRecorder> another_recorder =
      std::make_unique<AllocationTraceRecorder>();

  RegisterRecorderWithCrashpad(recorder);

  EXPECT_DEATH(RegisterRecorderWithCrashpad(*another_recorder), "");

  UnregisterRecorderWithCrashpad();
}

TEST_F(AllocationStackRecorderCrashClientDeathTest,
       VerifyShutDownWithoutInitialization) {
  EXPECT_DEATH(UnregisterRecorderWithCrashpad(), "");
}

TEST_F(AllocationStackRecorderCrashClientDeathTest, VerifyDuplicateShutDown) {
  auto& recorder = GetRecorder();

  RegisterRecorderWithCrashpad(recorder);

  UnregisterRecorderWithCrashpad();

  EXPECT_DEATH(UnregisterRecorderWithCrashpad(), "");
}
#endif
}  // namespace allocation_recorder::crash_client
