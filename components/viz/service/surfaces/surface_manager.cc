// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/queue.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_allocation_group.h"
#include "components/viz/service/surfaces/surface_client.h"
#include "components/viz/service/surfaces/surface_manager_delegate.h"

#if DCHECK_IS_ON()
#include <sstream>
#endif

namespace viz {
namespace {

const char kUmaAliveSurfaces[] = "Compositing.SurfaceManager.AliveSurfaces";

const char kUmaTemporaryReferences[] =
    "Compositing.SurfaceManager.TemporaryReferences";

constexpr base::TimeDelta kExpireInterval = base::TimeDelta::FromSeconds(10);

const char kUmaRemovedTemporaryReference[] =
    "Compositing.SurfaceManager.RemovedTemporaryReference";

}  // namespace

SurfaceManager::SurfaceManager(
    SurfaceManagerDelegate* delegate,
    base::Optional<uint32_t> activation_deadline_in_frames)
    : delegate_(delegate),
      activation_deadline_in_frames_(activation_deadline_in_frames),
      root_surface_id_(FrameSinkId(0u, 0u),
                       LocalSurfaceId(1u, base::UnguessableToken::Create())),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  thread_checker_.DetachFromThread();

  // Android WebView doesn't have a task runner and doesn't need the timer.
  if (base::SequencedTaskRunnerHandle::IsSet())
    expire_timer_.emplace();
}

SurfaceManager::~SurfaceManager() {
  // Garbage collect surfaces here to avoid calling back into
  // SurfaceDependencyTracker as members of SurfaceManager are being
  // destroyed.
  temporary_references_.clear();
  temporary_reference_ranges_.clear();
  // Create a copy of the children set as RemoveSurfaceReferenceImpl below will
  // mutate that set.
  base::flat_set<SurfaceId> children(
      GetSurfacesReferencedByParent(root_surface_id_));
  for (const auto& child : children)
    RemoveSurfaceReferenceImpl(SurfaceReference(root_surface_id_, child));

  GarbageCollectSurfaces();

  // All SurfaceClients and their surfaces are supposed to be
  // destroyed before SurfaceManager.
  // TODO(crbug.com/823043): The following two DCHECKs don't hold.
  // DCHECK(surface_map_.empty());
  // DCHECK(surfaces_to_destroy_.empty());
}

#if DCHECK_IS_ON()
std::string SurfaceManager::SurfaceReferencesToString() {
  std::stringstream str;
  SurfaceReferencesToStringImpl(root_surface_id_, "", &str);
  // Temporary references will have an asterisk in front of them.
  for (auto& map_entry : temporary_references_)
    SurfaceReferencesToStringImpl(map_entry.first, "* ", &str);

  return str.str();
}
#endif

void SurfaceManager::SetActivationDeadlineInFramesForTesting(
    base::Optional<uint32_t> activation_deadline_in_frames) {
  activation_deadline_in_frames_ = activation_deadline_in_frames;
}

void SurfaceManager::SetTickClockForTesting(const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

Surface* SurfaceManager::CreateSurface(
    base::WeakPtr<SurfaceClient> surface_client,
    const SurfaceInfo& surface_info) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(surface_info.is_valid());
  DCHECK(surface_client);

  // We should not be asked to create a surface that already exists.
  auto it = surface_map_.find(surface_info.id());
  if (it != surface_map_.end())
    return nullptr;

  SurfaceAllocationGroup* allocation_group =
      GetOrCreateAllocationGroupForSurfaceId(surface_info.id());
  // GetOrCreateAllocationGroupForSurfaceId can fail if two FrameSinkIds use the
  // same embed token.
  if (!allocation_group)
    return nullptr;

  std::unique_ptr<Surface> surface = std::make_unique<Surface>(
      surface_info, this, allocation_group, surface_client);
  surface->SetDependencyDeadline(
      std::make_unique<SurfaceDependencyDeadline>(tick_clock_));
  surface_map_[surface_info.id()] = std::move(surface);
  // We can get into a situation where multiple CompositorFrames arrive for a
  // FrameSink before the client can add any references for the frame. When
  // the second frame with a new size arrives, the first will be destroyed in
  // SurfaceFactory and then if there are no references it will be deleted
  // during surface GC. A temporary reference, removed when a real reference
  // is received, is added to prevent this from happening.
  AddTemporaryReference(surface_info.id());

  return surface_map_[surface_info.id()].get();
}

void SurfaceManager::MarkSurfaceForDestruction(const SurfaceId& surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(surface_map_.count(surface_id));
  for (auto& observer : observer_list_)
    observer.OnSurfaceMarkedForDestruction(surface_id);
  surfaces_to_destroy_.insert(surface_id);
}

void SurfaceManager::InvalidateFrameSinkId(const FrameSinkId& frame_sink_id) {
  auto it = frame_sink_id_to_allocation_groups_.find(frame_sink_id);
  if (it != frame_sink_id_to_allocation_groups_.end()) {
    for (SurfaceAllocationGroup* group : it->second) {
      group->WillNotRegisterNewSurfaces();
    }
  }
  GarbageCollectSurfaces();
}

const SurfaceId& SurfaceManager::GetRootSurfaceId() const {
  return root_surface_id_;
}

std::vector<SurfaceId> SurfaceManager::GetCreatedSurfaceIds() const {
  std::vector<SurfaceId> surface_ids;
  for (auto& map_entry : surface_map_)
    surface_ids.push_back(map_entry.first);
  return surface_ids;
}

void SurfaceManager::AddSurfaceReferences(
    const std::vector<SurfaceReference>& references) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (const auto& reference : references)
    AddSurfaceReferenceImpl(reference);
}

void SurfaceManager::RemoveSurfaceReferences(
    const std::vector<SurfaceReference>& references) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (const auto& reference : references)
    RemoveSurfaceReferenceImpl(reference);
}

void SurfaceManager::GarbageCollectSurfaces() {
  TRACE_EVENT0("viz", "SurfaceManager::GarbageCollectSurfaces");
  if (surfaces_to_destroy_.empty()) {
    // We should still try to garbage collect the allocation groups, because
    // even though no surface has been destroyed recently, the allocation groups
    // might have been unembedded which also marks them for destruction.
    MaybeGarbageCollectAllocationGroups();
    return;
  }

  SurfaceIdSet reachable_surfaces = GetLiveSurfaces();

  // Log the number of reachable surfaces after a garbage collection.
  UMA_HISTOGRAM_CUSTOM_COUNTS(kUmaAliveSurfaces, reachable_surfaces.size(), 1,
                              200, 50);
  // Log the number of temporary references after a garbage collection.
  UMA_HISTOGRAM_CUSTOM_COUNTS(kUmaTemporaryReferences,
                              temporary_references_.size(), 1, 200, 50);

  std::vector<SurfaceId> surfaces_to_delete;

  // Delete all destroyed and unreachable surfaces.
  for (auto iter = surfaces_to_destroy_.begin();
       iter != surfaces_to_destroy_.end();) {
    if (reachable_surfaces.count(*iter) == 0) {
      surfaces_to_delete.push_back(*iter);
      iter = surfaces_to_destroy_.erase(iter);
    } else {
      ++iter;
    }
  }

  // ~Surface() draw callback could modify |surfaces_to_destroy_|.
  for (const SurfaceId& surface_id : surfaces_to_delete)
    DestroySurfaceInternal(surface_id);

  // Run another pass over surfaces_to_delete, all of which have just been
  // deleted, making sure they are not present in |surfaces_to_destroy_|. This
  // is necessary as ~Surface may re-add already-in-destruction surfaces to the
  // set and we need to avoid double-deletion.
  // TODO(ericrk): Removing surfaces both here and above allows for
  // GarbageCollectSurfaces re-entrancy, which is exercised in tests and is
  // hard to prove can't happen in the wild. Evaluate whether we should allow
  // re-entrancy, and if not just remove here.
  for (const SurfaceId& surface_id : surfaces_to_delete)
    surfaces_to_destroy_.erase(surface_id);

  MaybeGarbageCollectAllocationGroups();
}

const base::flat_set<SurfaceId>& SurfaceManager::GetSurfacesReferencedByParent(
    const SurfaceId& surface_id) const {
  auto iter = references_.find(surface_id);
  if (iter == references_.end())
    return empty_surface_id_set_;
  return iter->second;
}

base::flat_set<SurfaceId>
SurfaceManager::GetSurfacesThatReferenceChildForTesting(
    const SurfaceId& surface_id) const {
  base::flat_set<SurfaceId> parents;

  for (auto& parent : references_) {
    if (parent.second.find(surface_id) != parent.second.end())
      parents.insert(parent.first);
  }
  return parents;
}

SurfaceManager::SurfaceIdSet SurfaceManager::GetLiveSurfaces() {
  SurfaceIdSet reachable_surfaces;

  // Walk down from the root and mark each SurfaceId we encounter as
  // reachable.
  base::queue<SurfaceId> surface_queue;
  surface_queue.push(root_surface_id_);

  // All surfaces not marked for destruction are reachable.
  for (auto& map_entry : surface_map_) {
    if (!IsMarkedForDestruction(map_entry.first)) {
      reachable_surfaces.insert(map_entry.first);
      surface_queue.push(map_entry.first);
    }
  }

  // All surfaces with temporary references are also reachable.
  for (auto& map_entry : temporary_references_) {
    const SurfaceId& surface_id = map_entry.first;
    if (reachable_surfaces.insert(surface_id).second) {
      surface_queue.push(surface_id);
    }
  }

  while (!surface_queue.empty()) {
    const auto& children = GetSurfacesReferencedByParent(surface_queue.front());
    for (const SurfaceId& child_id : children) {
      // Check for cycles when inserting into |reachable_surfaces|.
      if (reachable_surfaces.insert(child_id).second)
        surface_queue.push(child_id);
    }
    surface_queue.pop();
  }

  return reachable_surfaces;
}

void SurfaceManager::AddSurfaceReferenceImpl(
    const SurfaceReference& reference) {
  const SurfaceId& parent_id = reference.parent_id();
  const SurfaceId& child_id = reference.child_id();

  if (parent_id.frame_sink_id() == child_id.frame_sink_id()) {
    DLOG(ERROR) << "Cannot add self reference from " << parent_id << " to "
                << child_id;
    return;
  }

  // We trust that |parent_id| either exists or is about to exist, since is not
  // sent over IPC. We don't trust |child_id|, since it is sent over IPC.
  if (surface_map_.count(child_id) == 0) {
    DLOG(ERROR) << "No surface in map for " << child_id.ToString();
    return;
  }

  references_[parent_id].insert(child_id);

  for (auto& observer : observer_list_)
    observer.OnAddedSurfaceReference(parent_id, child_id);

  if (HasTemporaryReference(child_id))
    RemoveTemporaryReferenceImpl(child_id, RemovedReason::EMBEDDED);
}

void SurfaceManager::RemoveSurfaceReferenceImpl(
    const SurfaceReference& reference) {
  const SurfaceId& parent_id = reference.parent_id();
  const SurfaceId& child_id = reference.child_id();

  auto iter_parent = references_.find(parent_id);
  if (iter_parent == references_.end())
    return;

  auto child_iter = iter_parent->second.find(child_id);
  if (child_iter == iter_parent->second.end())
    return;

  for (auto& observer : observer_list_)
    observer.OnRemovedSurfaceReference(parent_id, child_id);

  iter_parent->second.erase(child_iter);
  if (iter_parent->second.empty())
    references_.erase(iter_parent);
}

bool SurfaceManager::HasTemporaryReference(const SurfaceId& surface_id) const {
  return temporary_references_.count(surface_id) != 0;
}

void SurfaceManager::AddTemporaryReference(const SurfaceId& surface_id) {
  DCHECK(!HasTemporaryReference(surface_id));

  // Add an entry to |temporary_references_|. Also add a range tracking entry so
  // we know the order that surfaces were created for the FrameSinkId.
  temporary_references_.emplace(surface_id, TemporaryReferenceData());
  temporary_reference_ranges_[surface_id.frame_sink_id()].push_back(
      surface_id.local_surface_id());

  // Start timer to expire temporary references if it's not running.
  if (expire_timer_ && !expire_timer_->IsRunning()) {
    expire_timer_->Start(FROM_HERE, kExpireInterval, this,
                         &SurfaceManager::ExpireOldTemporaryReferences);
  }
}

void SurfaceManager::RemoveTemporaryReferenceImpl(const SurfaceId& surface_id,
                                                  RemovedReason reason) {
  const FrameSinkId& frame_sink_id = surface_id.frame_sink_id();
  std::vector<LocalSurfaceId>& frame_sink_temp_refs =
      temporary_reference_ranges_[frame_sink_id];

  // Find the iterator to the range tracking entry for |surface_id|. Use that
  // iterator to find the right end iterator for the temporary references we
  // want to remove.
  auto end_iter =
      std::find_if(frame_sink_temp_refs.begin(), frame_sink_temp_refs.end(),
                   [&surface_id](const LocalSurfaceId& id) {
                     return id.IsNewerThan(surface_id.local_surface_id());
                   });
  auto begin_iter = frame_sink_temp_refs.begin();

  // Remove temporary references and range tracking information.
  for (auto iter = begin_iter; iter != end_iter; ++iter) {
    temporary_references_.erase(SurfaceId(frame_sink_id, *iter));

    // If removing more than the temporary reference to |surface_id| then the
    // reason for removing others is because they are being skipped.
    const bool was_skipped = (*iter != surface_id.local_surface_id());
    UMA_HISTOGRAM_ENUMERATION(kUmaRemovedTemporaryReference,
                              was_skipped ? RemovedReason::SKIPPED : reason,
                              RemovedReason::COUNT);
  }
  frame_sink_temp_refs.erase(begin_iter, end_iter);

  // If last temporary reference is removed for |frame_sink_id| then cleanup
  // range tracking map entry.
  if (frame_sink_temp_refs.empty())
    temporary_reference_ranges_.erase(frame_sink_id);

  // Stop the timer if there are no temporary references that could expire.
  if (temporary_references_.empty()) {
    if (expire_timer_ && expire_timer_->IsRunning())
      expire_timer_->Stop();
  }
}

Surface* SurfaceManager::GetLatestInFlightSurface(
    const SurfaceRange& surface_range) {
  SurfaceAllocationGroup* end_allocation_group =
      GetAllocationGroupForSurfaceId(surface_range.end());
  if (end_allocation_group) {
    Surface* result =
        end_allocation_group->FindLatestActiveSurfaceInRange(surface_range);
    if (result)
      return result;
  }
  if (!surface_range.start() ||
      surface_range.start()->local_surface_id().embed_token() ==
          surface_range.end().local_surface_id().embed_token()) {
    return nullptr;
  }
  SurfaceAllocationGroup* start_allocation_group =
      GetAllocationGroupForSurfaceId(*surface_range.start());
  if (!start_allocation_group)
    return nullptr;
  return start_allocation_group->FindLatestActiveSurfaceInRange(surface_range);
}

void SurfaceManager::ExpireOldTemporaryReferences() {
  if (temporary_references_.empty())
    return;

  std::vector<SurfaceId> temporary_references_to_delete;
  for (auto& map_entry : temporary_references_) {
    const SurfaceId& surface_id = map_entry.first;
    TemporaryReferenceData& data = map_entry.second;
    if (data.marked_as_old) {
      // The temporary reference has existed for more than 10 seconds, a surface
      // reference should have replaced it by now. To avoid permanently leaking
      // memory delete the temporary reference.
      base::StringPiece frame_sink_debug_label;
      if (delegate_) {
        frame_sink_debug_label =
            delegate_->GetFrameSinkDebugLabel(surface_id.frame_sink_id());
      }
      DLOG(ERROR) << "Old/orphaned temporary reference to "
                  << surface_id.ToString(frame_sink_debug_label);
      temporary_references_to_delete.push_back(surface_id);
    } else if (IsMarkedForDestruction(surface_id)) {
      // Never mark live surfaces as old, they can't be garbage collected.
      data.marked_as_old = true;
    }
  }

  for (auto& surface_id : temporary_references_to_delete)
    RemoveTemporaryReferenceImpl(surface_id, RemovedReason::EXPIRED);
}

Surface* SurfaceManager::GetSurfaceForId(const SurfaceId& surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return nullptr;
  return it->second.get();
}

bool SurfaceManager::SurfaceModified(const SurfaceId& surface_id,
                                     const BeginFrameAck& ack) {
  CHECK(thread_checker_.CalledOnValidThread());
  bool changed = false;
  for (auto& observer : observer_list_)
    changed |= observer.OnSurfaceDamaged(surface_id, ack);
  return changed;
}

void SurfaceManager::FirstSurfaceActivation(const SurfaceInfo& surface_info) {
  CHECK(thread_checker_.CalledOnValidThread());

  for (auto& observer : observer_list_)
    observer.OnFirstSurfaceActivation(surface_info);
}

void SurfaceManager::SurfaceActivated(
    Surface* surface,
    base::Optional<base::TimeDelta> duration) {
  // Trigger a display frame if necessary.
  const CompositorFrame& frame = surface->GetActiveFrame();
  if (!SurfaceModified(surface->surface_id(), frame.metadata.begin_frame_ack)) {
    TRACE_EVENT_INSTANT0("viz", "Damage not visible.",
                         TRACE_EVENT_SCOPE_THREAD);
    surface->SendAckToClient();
  }

  for (auto& observer : observer_list_)
    observer.OnSurfaceActivated(surface->surface_id(), duration);
}

void SurfaceManager::SurfaceDestroyed(Surface* surface) {
  for (auto& observer : observer_list_)
    observer.OnSurfaceDestroyed(surface->surface_id());
}

void SurfaceManager::SurfaceDamageExpected(const SurfaceId& surface_id,
                                           const BeginFrameArgs& args) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observer_list_)
    observer.OnSurfaceDamageExpected(surface_id, args);
}

void SurfaceManager::DestroySurfaceInternal(const SurfaceId& surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  // Make sure that the surface is removed from the map before being actually
  // destroyed. An ack could be sent during the destruction of a surface which
  // could trigger a synchronous frame submission to a half-destroyed surface
  // and that's not desirable.
  std::unique_ptr<Surface> doomed = std::move(it->second);
  surface_map_.erase(it);
  references_.erase(surface_id);
}

#if DCHECK_IS_ON()
void SurfaceManager::SurfaceReferencesToStringImpl(const SurfaceId& surface_id,
                                                   std::string indent,
                                                   std::stringstream* str) {
  *str << indent;
  // Print the current line for |surface_id|.
  Surface* surface = GetSurfaceForId(surface_id);
  if (surface) {
    *str << surface->surface_id().ToString();

    *str << (IsMarkedForDestruction(surface_id) ? " destroyed" : " live");

    if (surface->HasPendingFrame()) {
      // This provides the surface size from the root render pass.
      const CompositorFrame& frame = surface->GetPendingFrame();
      *str << " pending " << frame.size_in_pixels().ToString();
    }

    if (surface->HasActiveFrame()) {
      // This provides the surface size from the root render pass.
      const CompositorFrame& frame = surface->GetActiveFrame();
      *str << " active " << frame.size_in_pixels().ToString();
    }
  } else {
    *str << surface_id;
  }
  *str << "\n";

  // If the current surface has references to children, sort children and print
  // references for each child.
  for (const SurfaceId& child_id : GetSurfacesReferencedByParent(surface_id))
    SurfaceReferencesToStringImpl(child_id, indent + "  ", str);
}
#endif  // DCHECK_IS_ON()

bool SurfaceManager::IsMarkedForDestruction(const SurfaceId& surface_id) {
  return surfaces_to_destroy_.count(surface_id) != 0;
}

void SurfaceManager::SurfaceWillBeDrawn(Surface* surface) {
  for (auto& observer : observer_list_)
    observer.OnSurfaceWillBeDrawn(surface);
}

void SurfaceManager::DropTemporaryReference(const SurfaceId& surface_id) {
  RemoveTemporaryReferenceImpl(surface_id, RemovedReason::DROPPED);
}

SurfaceAllocationGroup* SurfaceManager::GetOrCreateAllocationGroupForSurfaceId(
    const SurfaceId& surface_id) {
  std::unique_ptr<SurfaceAllocationGroup>& allocation_group =
      embed_token_to_allocation_group_[surface_id.local_surface_id()
                                           .embed_token()];
  if (allocation_group && allocation_group->submitter_frame_sink_id() !=
                              surface_id.frame_sink_id()) {
    DLOG(ERROR) << "Cannot reuse embed token across frame sinks";
    return nullptr;
  }
  if (!allocation_group) {
    allocation_group = std::make_unique<SurfaceAllocationGroup>(
        this, surface_id.frame_sink_id(),
        surface_id.local_surface_id().embed_token());
    frame_sink_id_to_allocation_groups_[surface_id.frame_sink_id()].push_back(
        allocation_group.get());
  }
  return allocation_group.get();
}

SurfaceAllocationGroup* SurfaceManager::GetAllocationGroupForSurfaceId(
    const SurfaceId& surface_id) {
  auto it = embed_token_to_allocation_group_.find(
      surface_id.local_surface_id().embed_token());
  if (it == embed_token_to_allocation_group_.end())
    return nullptr;
  DCHECK(it->second);
  if (it->second->submitter_frame_sink_id() != surface_id.frame_sink_id()) {
    DLOG(ERROR) << "Cannot reuse embed token across frame sinks";
    return nullptr;
  }
  return it->second.get();
}

void SurfaceManager::SetAllocationGroupsNeedGarbageCollection() {
  allocation_groups_need_garbage_collection_ = true;
}

void SurfaceManager::MaybeGarbageCollectAllocationGroups() {
  if (!allocation_groups_need_garbage_collection_)
    return;

  bool did_destroy = false;
  for (auto it = embed_token_to_allocation_group_.begin();
       it != embed_token_to_allocation_group_.end(); ++it) {
    if (!it->second->IsReadyToDestroy())
      continue;
    // Before destroying the allocation group, remove it from
    // |frame_sink_id_to_allocation_groups_|.
    auto list_it = frame_sink_id_to_allocation_groups_.find(
        it->second->submitter_frame_sink_id());
    DCHECK(list_it != frame_sink_id_to_allocation_groups_.end());
    base::Erase(list_it->second, it->second.get());
    if (list_it->second.empty())
      frame_sink_id_to_allocation_groups_.erase(list_it);
    // Destroy the allocation group. Removing it from the map is done in a
    // separate pass to avoid invalidating the iterator.
    it->second.reset();
    did_destroy = true;
  }

  // Remove the destroyed allocation groups from the map.
  if (did_destroy) {
    base::EraseIf(embed_token_to_allocation_group_,
                  [](auto& entry) { return !entry.second; });
  }

  allocation_groups_need_garbage_collection_ = false;
}

bool SurfaceManager::HasBlockedEmbedder(
    const FrameSinkId& frame_sink_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = frame_sink_id_to_allocation_groups_.find(frame_sink_id);
  if (it == frame_sink_id_to_allocation_groups_.end())
    return false;
  for (SurfaceAllocationGroup* group : it->second) {
    if (group->HasBlockedEmbedder())
      return true;
  }
  return false;
}

}  // namespace viz
