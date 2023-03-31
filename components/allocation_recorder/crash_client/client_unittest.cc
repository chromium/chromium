// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/allocation_recorder/crash_client/client.h"

#include "base/debug/allocation_trace.h"
#include "components/allocation_recorder/internal/internal.h"
#include "components/crash/core/common/crash_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/annotation_list.h"

using allocation_recorder::internal::kAnnotationName;
using allocation_recorder::internal::kAnnotationType;
using base::debug::tracer::AllocationTraceRecorder;

namespace allocation_recorder::crash_client {

namespace {
// Unfortunately, crashpad::AnnotationList::Iterator doesn't comply to
// requirements for std iterator. More specifically
// - it doesn't include various typedefs of iterator type etc.
// - the interface is missing some functions required by InputIterator, see
//   https://en.cppreference.com/w/cpp/named_req/InputIterator for details.
//
// Hence, we provide a small helper function to compute the distance. Since
// Iterator provides an interface similar to an input iterator, we assume that
// end is reachable from begin using operator++().
//
// TODO(https://crbug.com/1284275) With C++20s new iterator concept, we should
// see if we can make crashpad::AnnotationList::Iterator standard compliant and
// make this function obsolete. Also to discuss, can we go for something for
// powerful than InputIterator?
size_t distance(crashpad::AnnotationList::Iterator const begin,
                crashpad::AnnotationList::Iterator const end) {
  size_t dist = 0;

  for (auto current = begin; current != end; ++current) {
    ++dist;
  }

  return dist;
}
}  // namespace

class AllocationStackRecorderCrashClientTest : public ::testing::Test {};

TEST_F(AllocationStackRecorderCrashClientTest, VerifyInitialize) {
  // execute test
  auto& recorder = Initialize();

  // check that Initialize correctly registered the address of the recorder with
  // the crashpad annotations framework.
  auto* const annotation_list = crashpad::AnnotationList::Get();
  ASSERT_NE(annotation_list, nullptr);
  ASSERT_EQ(distance(annotation_list->begin(), annotation_list->end()), 1ul);

  auto* const annotation = *(annotation_list->begin());
  ASSERT_NE(annotation, nullptr);

  ASSERT_EQ(annotation->is_set(), true);
  ASSERT_EQ(annotation->size(), sizeof(&recorder));
  EXPECT_EQ(annotation->type(), kAnnotationType);

  ASSERT_NE(annotation->value(), nullptr);
  ASSERT_EQ(
      *reinterpret_cast<AllocationTraceRecorder* const*>(annotation->value()),
      &recorder);

  // Assert here to prevent construction of std::string from nullptr.
  ASSERT_NE(annotation->name(), nullptr);
  EXPECT_EQ(annotation->name(), std::string{kAnnotationName});

  // On Android, Initialize additionally inserts the recorder into the list of
  // allowed memory addresses. Unfortunately, the list which the recorder is
  // being added to is a private implementation detail and not accessible.
  // Hence, no verification for this part.
}

}  // namespace allocation_recorder::crash_client
