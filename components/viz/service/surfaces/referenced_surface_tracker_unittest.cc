// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/referenced_surface_tracker.h"

#include <memory>

#include "base/containers/flat_set.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_reference.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::IsEmpty;

namespace viz {
namespace {

constexpr FrameSinkId kParentFrameSink(2, 1);
constexpr FrameSinkId kChildFrameSink1(65563, 1);
constexpr FrameSinkId kChildFrameSink2(65564, 1);

base::flat_set<SurfaceId> MakeReferenceSet(
    std::initializer_list<SurfaceId> surface_ids) {
  return base::flat_set<SurfaceId>(surface_ids);
}

SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id, uint32_t parent_id) {
  return SurfaceId(frame_sink_id,
                   LocalSurfaceId(parent_id, base::UnguessableToken::Create()));
}

}  // namespace

class ReferencedSurfaceTrackerTest : public testing::Test {
 public:
  ReferencedSurfaceTrackerTest() {}

  ReferencedSurfaceTrackerTest(const ReferencedSurfaceTrackerTest&) = delete;
  ReferencedSurfaceTrackerTest& operator=(const ReferencedSurfaceTrackerTest&) =
      delete;

  ~ReferencedSurfaceTrackerTest() override {}

  const std::vector<SurfaceReference>& references_to_remove() const {
    return references_to_remove_;
  }

  const std::vector<SurfaceReference>& references_to_add() const {
    return references_to_add_;
  }

  void UpdateReferences(
      const SurfaceId& surface_id,
      const base::flat_set<SurfaceId>& old_referenced_surfaces,
      const base::flat_set<SurfaceId>& new_referenced_surfaces) {
    references_to_add_.clear();
    references_to_remove_.clear();
    GetSurfaceReferenceDifference(surface_id, old_referenced_surfaces,
                                  new_referenced_surfaces, &references_to_add_,
                                  &references_to_remove_);
  }

 private:
  std::vector<SurfaceReference> references_to_add_;
  std::vector<SurfaceReference> references_to_remove_;
};

TEST_F(ReferencedSurfaceTrackerTest, AddSurfaceReference) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceReference reference(parent_id, child_id1);

  // Check that reference to |child_id1| is added.
  UpdateReferences(parent_id, MakeReferenceSet({}),
                   MakeReferenceSet({child_id1}));
  EXPECT_THAT(references_to_add(), UnorderedElementsAre(reference));
  EXPECT_THAT(references_to_remove(), IsEmpty());
}

TEST_F(ReferencedSurfaceTrackerTest, NoChangeToReferences) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceReference reference(parent_id, child_id1);

  // Check that no references are added or removed.
  auto referenced_surfaces = MakeReferenceSet({child_id1});
  UpdateReferences(parent_id, referenced_surfaces, referenced_surfaces);
  EXPECT_THAT(references_to_remove(), IsEmpty());
  EXPECT_THAT(references_to_add(), IsEmpty());
}

TEST_F(ReferencedSurfaceTrackerTest, RemoveSurfaceReference) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceReference reference(parent_id, child_id1);

  // Check that reference to |child_id1| is removed.
  UpdateReferences(parent_id, MakeReferenceSet({child_id1}),
                   MakeReferenceSet({}));
  EXPECT_THAT(references_to_add(), IsEmpty());
  EXPECT_THAT(references_to_remove(), UnorderedElementsAre(reference));
}

TEST_F(ReferencedSurfaceTrackerTest, RemoveOneOfTwoSurfaceReferences) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1_first = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id1_second = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceReference reference_first(parent_id, child_id1_first);
  const SurfaceReference reference_second(parent_id, child_id1_second);

  // Check that reference to |child_id1_first| is removed and reference to
  // |child_id1_second| is added.
  UpdateReferences(parent_id, MakeReferenceSet({child_id1_first}),
                   MakeReferenceSet({child_id1_second}));
  EXPECT_THAT(references_to_remove(), UnorderedElementsAre(reference_first));
  EXPECT_THAT(references_to_add(), UnorderedElementsAre(reference_second));
}

TEST_F(ReferencedSurfaceTrackerTest, AddTwoThenRemoveOneSurfaceReferences) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 2);
  const SurfaceReference reference1(parent_id, child_id1);
  const SurfaceReference reference2(parent_id, child_id2);

  // Check that first frame adds both surface references.
  const auto initial_referenced = MakeReferenceSet({child_id1, child_id2});
  UpdateReferences(parent_id, MakeReferenceSet({}), initial_referenced);
  EXPECT_THAT(references_to_remove(), IsEmpty());
  EXPECT_THAT(references_to_add(),
              UnorderedElementsAre(reference1, reference2));

  // Check that reference to |child_id1| is removed but not to |child_id2|.
  UpdateReferences(parent_id, initial_referenced,
                   MakeReferenceSet({child_id2}));
  EXPECT_THAT(references_to_remove(), UnorderedElementsAre(reference1));
  EXPECT_THAT(references_to_add(), IsEmpty());
}

}  // namespace viz
