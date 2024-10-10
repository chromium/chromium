// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/surfaces/surface_observer.h"
#include "components/viz/service/surfaces/surface_reference.h"

#if DCHECK_IS_ON()
#include <iosfwd>
#include <string>
#endif

namespace base {
class TickClock;
}  // namespace base

namespace viz {

class Surface;
class SurfaceAllocationGroup;
class SurfaceClient;
class SurfaceManagerDelegate;
class SurfaceRange;

class VIZ_SERVICE_EXPORT SurfaceManager {
 public:
  SurfaceManager(SurfaceManagerDelegate* delegate,
                 std::optional<uint32_t> activation_deadline_in_frames,
                 size_t max_uncommitted_frames);

  SurfaceManager(const SurfaceManager&) = delete;
  SurfaceManager& operator=(const SurfaceManager&) = delete;

  ~SurfaceManager();

#if DCHECK_IS_ON()
  // Returns a string representation of all reachable surface references.
  std::string SurfaceReferencesToString();
#endif

  // Sets an alternative system default frame activation deadline for unit
  // tests. std::nullopt indicates no deadline (in other words, an unlimited
  // deadline).
  void SetActivationDeadlineInFramesForTesting(
      std::optional<uint32_t> deadline);

  std::optional<uint32_t> activation_deadline_in_frames() const {
    return activation_deadline_in_frames_;
  }

  // Sets an alternative base::TickClock to pass into surfaces for surface
  // synchronization deadlines. This allows unit tests to mock the wall clock.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Returns the base::TickClock used to set surface synchronization deadlines.
  const base::TickClock* tick_clock() { return tick_clock_; }

  // Creates a Surface for the given SurfaceClient. The surface will be
  // destroyed when MarkSurfaceForDestruction is called, all of its destruction
  // dependencies are satisfied, and it is not reachable from the root surface.
  // A temporary reference will be added to the new Surface.
  Surface* CreateSurface(base::WeakPtr<SurfaceClient> surface_client,
                         const SurfaceInfo& surface_info,
                         const SurfaceId& pending_copy_surface_id);

  // Marks |surface_id| for destruction. The surface will get destroyed when
  // it's not reachable from the root or any other surface that is not marked
  // for destruction.
  void MarkSurfaceForDestruction(const SurfaceId& surface_id);

  // Returns a Surface corresponding to the provided |surface_id|.
  Surface* GetSurfaceForId(const SurfaceId& surface_id) const;

  void AddObserver(SurfaceObserver* obs) { observer_list_.AddObserver(obs); }

  void RemoveObserver(SurfaceObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

  // Called when a Surface is modified, e.g. when a CompositorFrame is
  // activated, its producer confirms that no CompositorFrame will be submitted
  // in response to a BeginFrame, or a CopyOutputRequest is issued.
  //
  // |ack.sequence_number| is only valid if called in response to a BeginFrame.
  bool SurfaceModified(const SurfaceId& surface_id,
                       const BeginFrameAck& ack,
                       SurfaceObserver::HandleInteraction handle_interaction);

  // Called when a surface has an active frame for the first time.
  void FirstSurfaceActivation(const SurfaceInfo& surface_info);

  // Called when there is new frame in uncommitted queue of the surface.
  void OnSurfaceHasNewUncommittedFrame(Surface* surface);

  // Called when a CompositorFrame within |surface| has activated.
  void SurfaceActivated(Surface* surface);

  // Called when |surface| is being destroyed.
  void SurfaceDestroyed(Surface* surface);

  // Called when a Surface's CompositorFrame producer has received a BeginFrame
  // and, thus, is expected to produce damage soon.
  void SurfaceDamageExpected(const SurfaceId& surface_id,
                             const BeginFrameArgs& args);

  // Invalidate a frame_sink_id that might still have associated sequences,
  // possibly because a renderer process has crashed.
  void InvalidateFrameSinkId(const FrameSinkId& frame_sink_id);

  // Returns the top level root SurfaceId. Surfaces that are not reachable
  // from the top level root may be garbage collected. It will not be a valid
  // SurfaceId and will never correspond to a surface.
  const SurfaceId& GetRootSurfaceId() const;

  // Returns SurfaceIds of currently alive Surfaces. This may include ids of
  // Surfaces that are about to be destroyed.
  std::vector<SurfaceId> GetCreatedSurfaceIds() const;

  // Adds all surface references in |references|. This will remove any temporary
  // references for child surface in a surface reference.
  void AddSurfaceReferences(const std::vector<SurfaceReference>& references);

  // Removes all surface references in |references| then runs garbage
  // collection to delete unreachable surfaces.
  void RemoveSurfaceReferences(const std::vector<SurfaceReference>& references);

  // Garbage collects all destroyed surfaces that aren't live.
  void GarbageCollectSurfaces();

  // Returns all surfaces referenced by parent |surface_id|. Will return an
  // empty set if |surface_id| is unknown or has no references.
  const base::flat_set<SurfaceId>& GetSurfacesReferencedByParent(
      const SurfaceId& surface_id) const;

  // Returns all surfaces that have a reference to child |surface_id|. Will
  // return an empty set if |surface_id| is unknown or has no references to it.
  base::flat_set<SurfaceId> GetSurfacesThatReferenceChildForTesting(
      const SurfaceId& surface_id) const;

  // Gets the earliest timestamp when the surface with ID `surface_id` gets
  // embedded through `AddSurfaceReferences()`, if it's already embedded.
  // Returns an empty base::TimeTicks() if the surface hasn't been embedded yet.
  base::TimeTicks GetSurfaceReferencedTimestamp(
      const SurfaceId& surface_id) const;

  // Returns the primary surface if it exists. Otherwise, this will return the
  // most recent surface in |surface_range|. If no surface exists, this will
  // return nullptr.
  Surface* GetLatestInFlightSurface(const SurfaceRange& surface_range);

  // Called by SurfaceAggregator notifying us that it will use |surface| in the
  // next display frame. We will notify SurfaceObservers accordingly.
  void SurfaceWillBeDrawn(Surface* surface);

  // Removes temporary reference to |surface_id| and older surfaces.
  void DropTemporaryReference(const SurfaceId& surface_id);

  // Returns the corresponding SurfaceAllocationGroup for |surface_id|. A
  // SurfaceAllocationGroup will be created for |surface_id| if one doesn't
  // exist yet. If there is already a SurfaceAllocationGroup that matches the
  // embed token of |surface_id| but its submitter doesn't match |surface_id|'s
  // FrameSinkId, nullptr will be returned. In any other case, the returned
  // value will always be a valid SurfaceAllocationGroup.
  SurfaceAllocationGroup* GetOrCreateAllocationGroupForSurfaceId(
      const SurfaceId& surface_id);

  // Similar to GetOrCreateAllocationGroupForSurfaceId, but will not attempt to
  // create the allocation group if it does not already exist.
  SurfaceAllocationGroup* GetAllocationGroupForSurfaceId(
      const SurfaceId& surface_id);

  // Called by allocation groups when they're ready to destroy and need garbage
  // collection.
  void SetAllocationGroupsNeedGarbageCollection();

  // Returns whether there is any surface blocked on a surface from
  // |frame_sink_id|.
  bool HasBlockedEmbedder(const FrameSinkId& frame_sink_id) const;

  // Indicates that the set of frame sinks being aggregated for display has
  // changed since the previous aggregation.
  void AggregatedFrameSinksChanged();

  using CommitPredicate =
      base::FunctionRef<bool(const SurfaceId&, const BeginFrameId&)>;
  // Commits all surfaces in range and their referenced surfaces. For each
  // surface processed calls `predicate` for each uncommitted frame from oldest
  // to newest. If predicate returns true, surface is committed. If not the
  // surface processing stops and we go to the next surface.
  void CommitFramesInRangeRecursively(const SurfaceRange& range,
                                      const CommitPredicate& predicate);

 private:
  friend class CompositorFrameSinkSupportTestBase;
  friend class FrameSinkManagerTest;
  friend class HitTestAggregatorTest;
  friend class SurfaceSynchronizationTest;
  friend class SurfaceReferencesTest;
  friend class SurfaceSynchronizationTest;

  using SurfaceIdSet = std::unordered_set<SurfaceId, SurfaceIdHash>;

  // The reason for removing a temporary reference.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RemovedReason {
    EMBEDDED = 0,  // The surface was embedded.
    DROPPED = 1,   // The surface won't be embedded so it was dropped.
    SKIPPED = 2,   // A newer surface was embedded and the surface was skipped.
    EXPIRED = 4,   // The surface was never embedded and expired.
    COPIED = 5,    // The surface was copied.
    COUNT
  };

  struct TemporaryReferenceData {
    // Used to track old surface references, will be marked as true on first
    // timer tick and will be true on second timer tick.
    bool marked_as_old = false;
  };

  // Returns set of surfaces that cannot be garbage-collected.
  SurfaceIdSet GetLiveSurfaces();

  // Adds a reference from |parent_id| to |child_id| without dealing with
  // temporary references.
  void AddSurfaceReferenceImpl(const SurfaceReference& reference);

  // Removes a reference from a |parent_id| to |child_id|.
  void RemoveSurfaceReferenceImpl(const SurfaceReference& reference);

  // Returns whether |surface_id| has a temporary reference or not.
  bool HasTemporaryReference(const SurfaceId& surface_id) const;

  // Adds a temporary reference to |surface_id|. The reference will not have an
  // owner initially.
  void AddTemporaryReference(const SurfaceId& surface_id);

  // Removes temporary reference to |surface_id| and older surfaces. The
  // |reason| for removing will be recorded with UMA.
  void RemoveTemporaryReferenceImpl(const SurfaceId& surface_id,
                                    RemovedReason reason);

  // Marks and then expires old temporary references. This function is run
  // periodically by a timer.
  void ExpireOldTemporaryReferences();

  // Removes the surface from the surface map and destroys it.
  void DestroySurfaceInternal(const SurfaceId& surface_id);

#if DCHECK_IS_ON()
  // Recursively prints surface references starting at |surface_id| to |str|.
  void SurfaceReferencesToStringImpl(const SurfaceId& surface_id,
                                     std::string indent,
                                     std::stringstream* str);
#endif

  // Returns true if |surface_id| is in the garbage collector's queue.
  bool IsMarkedForDestruction(const SurfaceId& surface_id);

  // Garbage-collects the allocation groups if they have signalled that they are
  // ready for destruction.
  void MaybeGarbageCollectAllocationGroups();

  // This returns true if early-acks for frame activation during interaction is
  // enabled and if the number of frames since ack and the last interactive
  // frame is below the cooldown threshold. This is only true for the Surfaces
  // which are not currently being interacted with.
  bool ShouldAckNonInteractiveFrame(const CompositorFrameMetadata& ack) const;

  // Can be nullptr.
  const raw_ptr<SurfaceManagerDelegate> delegate_;

  std::optional<uint32_t> activation_deadline_in_frames_;

  base::flat_map<base::UnguessableToken,
                 std::unique_ptr<SurfaceAllocationGroup>>
      embed_token_to_allocation_group_;
  base::flat_map<
      FrameSinkId,
      std::vector<raw_ptr<SurfaceAllocationGroup, VectorExperimental>>>
      frame_sink_id_to_allocation_groups_;
  base::flat_map<SurfaceId, std::unique_ptr<Surface>> surface_map_;
  base::ObserverList<SurfaceObserver>::Unchecked observer_list_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::flat_map<SurfaceId, base::TimeTicks> surfaces_to_destroy_;

  // Root SurfaceId that references display root surfaces. There is no Surface
  // with this id, it's for bookkeeping purposes only.
  const SurfaceId root_surface_id_;

  // Always empty set that is returned when there is no entry in |references_|
  // for a SurfaceId.
  const base::flat_set<SurfaceId> empty_surface_id_set_;

  // Used for setting deadlines for surface synchronization.
  raw_ptr<const base::TickClock> tick_clock_;

  // Keeps track of surface references for a surface. The graph of references is
  // stored in parent to child direction. i.e the map stores all direct children
  // of the surface specified by |SurfaceId|.
  std::unordered_map<SurfaceId, base::flat_set<SurfaceId>, SurfaceIdHash>
      references_;

  // A map of surfaces that have temporary references.
  std::unordered_map<SurfaceId, TemporaryReferenceData, SurfaceIdHash>
      temporary_references_;

  // A map of pair(the timestamp of the first time a surface gets referenced,
  // the number of references that surface has).
  std::unordered_map<SurfaceId,
                     std::pair<base::TimeTicks, uint32_t>,
                     SurfaceIdHash>
      surface_referenced_timestamps_;

  // Range tracking information for temporary references. Each map entry is an
  // is an ordered list of SurfaceIds that have temporary references with the
  // same FrameSinkId. A SurfaceId can be reconstructed with:
  //   SurfaceId surface_id(key, value[index]);
  // The LocalSurfaceIds are stored in the order the surfaces are created in. If
  // a reference is added to a later SurfaceId then all temporary references up
  // to that point will be removed. This is to handle clients getting out of
  // sync, for example the embedded client producing new SurfaceIds faster than
  // the embedding client can use them.
  std::unordered_map<FrameSinkId, std::vector<LocalSurfaceId>, FrameSinkIdHash>
      temporary_reference_ranges_;

  std::optional<BeginFrameId> last_interactive_frame_;

  // Timer to remove old temporary references that aren't removed after an
  // interval of time. The timer will started/stopped so it only runs if there
  // are temporary references. Also the timer isn't used with Android WebView.
  std::optional<base::RepeatingTimer> expire_timer_;

  bool allocation_groups_need_garbage_collection_ = false;

  // Maximum length of uncommitted queue, zero means all frames are committed
  // automatically.
  const size_t max_uncommitted_frames_;

  std::optional<uint64_t>
      cooldown_frames_for_ack_on_activation_during_interaction_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_MANAGER_H_
