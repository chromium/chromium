// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/client_resource_provider.h"

#include <algorithm>
#include <utility>

#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace viz {

namespace {

void CustomUmaHistogramMemoryKB(const char* name, int sample) {
  // Based on UmaHistogramMemoryKB, but with a starting bucket for under 1 MB.
  // This gives granularity for smaller resources that are held around. Vs 0 KB
  // representing an estimation error.
  base::UmaHistogramCustomCounts(name, sample, 0, 500000, 50);
}

void ReportResourceSourceUsage(TransferableResource::ResourceSource source,
                               size_t usage) {
  const size_t usage_in_kb = usage / 1024u;
  switch (source) {
    case TransferableResource::ResourceSource::kUnknown:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.Unknown", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kAR:
      CustomUmaHistogramMemoryKB("Memory.Renderer.EvictedLockedResources.AR",
                                 usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kCanvas:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.Canvas", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kDrawingBuffer:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.DrawingBuffer", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kExoBuffer:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.ExoBuffer", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kHeadsUpDisplay:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.HeadsUpDisplay", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kImageLayerBridge:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.ImageLayerBridge",
          usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kPPBGraphics3D:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.PPBGraphics3D", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kPepperGraphics2D:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.PepperGraphics2D",
          usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kViewTransition:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.ViewTransition", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kStaleContent:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.StaleContent", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kTest:
      CustomUmaHistogramMemoryKB("Memory.Renderer.EvictedLockedResources.Test",
                                 usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kTileRasterTask:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.TileRasterTask", usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kUI:
      CustomUmaHistogramMemoryKB("Memory.Renderer.EvictedLockedResources.UI",
                                 usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kVideo:
      CustomUmaHistogramMemoryKB("Memory.Renderer.EvictedLockedResources.Video",
                                 usage_in_kb);
      break;
    case TransferableResource::ResourceSource::kWebGPUSwapBuffer:
      CustomUmaHistogramMemoryKB(
          "Memory.Renderer.EvictedLockedResources.WebGPUSwapBuffer",
          usage_in_kb);
      break;
  }
}

class ScopedBatchResourcesReleaseImpl
    : public ClientResourceProvider::ScopedBatchResourcesRelease {
 public:
  using ScopedBatchResourcesRelease::ScopedBatchResourcesRelease;
  explicit ScopedBatchResourcesReleaseImpl(
      base::OnceClosure batch_release_callback);
};

ScopedBatchResourcesReleaseImpl::ScopedBatchResourcesReleaseImpl(
    base::OnceClosure batch_release_callback)
    : ScopedBatchResourcesRelease(std::move(batch_release_callback)) {}

}  // namespace

struct ClientResourceProvider::ImportedResource {
  TransferableResource resource;
  ReleaseCallback impl_release_callback;
  ReleaseCallback main_thread_release_callback;
  int exported_count = 0;
  bool marked_for_deletion = false;

  gpu::SyncToken returned_sync_token;
  bool returned_lost = false;

  ResourceEvictedCallback evicted_callback;

#if DCHECK_IS_ON()
  base::debug::StackTrace stack_trace;
#endif

  ImportedResource(ResourceId id,
                   const TransferableResource& resource,
                   ReleaseCallback impl_release_callback,
                   ReleaseCallback main_thread_release_callback,
                   ResourceEvictedCallback evicted_callback)
      : resource(resource),
        impl_release_callback(std::move(impl_release_callback)),
        main_thread_release_callback(std::move(main_thread_release_callback)),
        // If the resource is immediately deleted, it returns the same SyncToken
        // it came with. The client may need to wait on that before deleting the
        // backing or reusing it.
        returned_sync_token(resource.sync_token()),
        evicted_callback(std::move(evicted_callback)) {
    // We should never have no ReleaseCallback.
    CHECK(this->impl_release_callback || this->main_thread_release_callback);
    // Replace the |resource| id with the local id from this
    // ClientResourceProvider.
    this->resource.id = id;
  }
  ~ImportedResource() = default;

  ImportedResource(ImportedResource&&) = default;
  ImportedResource& operator=(ImportedResource&&) = default;

  void RunReleaseCallbacks() {
    if (impl_release_callback) {
      std::move(impl_release_callback).Run(returned_sync_token, returned_lost);
    }
    // We intend for `main_thread_release_callback` to be run on
    // `main_tast_runner_`. This is done in
    // `ClientResourceProvider::BatchMainReleaseCallbacks`, in response to
    // resources being returned. However the client can also remove resources
    // independently. Since we currently do not know when these removals would
    // start/stop, we cannot batch them. Instead maintain previous behaviour
    // of just calling these directly.
    if (main_thread_release_callback) {
      std::move(main_thread_release_callback)
          .Run(returned_sync_token, returned_lost);
    }
  }
};

ClientResourceProvider::ScopedBatchResourcesRelease::
    ScopedBatchResourcesRelease(
        ClientResourceProvider::ScopedBatchResourcesRelease&& other) = default;

ClientResourceProvider::ScopedBatchResourcesRelease::
    ~ScopedBatchResourcesRelease() {
  if (batch_release_callback_) {
    std::move(batch_release_callback_).Run();
  }
}

ClientResourceProvider::ScopedBatchResourcesRelease::
    ScopedBatchResourcesRelease(
        base::OnceCallback<void()> batch_release_callback)
    : batch_release_callback_(std::move(batch_release_callback)) {}

ClientResourceProvider::ClientResourceProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

ClientResourceProvider::ClientResourceProvider(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    scoped_refptr<base::SequencedTaskRunner> impl_task_runner,
    ResourceFlushCallback resource_flush_callback)
    : main_task_runner_(main_task_runner),
      impl_task_runner_(impl_task_runner),
      resource_flush_callback_(std::move(resource_flush_callback)),
      threaded_release_callbacks_supported_(
          main_task_runner_ && impl_task_runner_ &&
          main_task_runner_ != impl_task_runner_ && resource_flush_callback_) {}

ClientResourceProvider::~ClientResourceProvider() {
  // If this fails, there are outstanding resources exported that should be
  // lost and returned by calling ShutdownAndReleaseAllResources(), or there
  // are resources that were imported without being removed by
  // RemoveImportedResource(). In either case, calling
  // ShutdownAndReleaseAllResources() will help, as it will report which
  // resources were imported without being removed as well.
  DCHECK(imported_resources_.empty());

  // It is possible that we were deleted while a `ScopedBatchResourcesRelease`
  // was still being held. This ensures the callbacks are ran.
  BatchResourceRelease();
}

void ClientResourceProvider::PrepareSendToParent(
    const std::vector<ResourceId>& export_ids,
    std::vector<TransferableResource>* list,
    gpu::SharedImageInterface* shared_image_interface) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This function goes through the array multiple times, store the resources
  // as pointers so we don't have to look up the resource id multiple times.
  // Make sure the maps do not change while these vectors are alive or they
  // will become invalid.
  std::vector<ImportedResource*> imports;
  imports.reserve(export_ids.size());
  for (const ResourceId id : export_ids) {
    auto it = imported_resources_.find(id);
    CHECK(it != imported_resources_.end());
    imports.push_back(&it->second);
  }

  if (shared_image_interface) {
    // Note for this lamdba -> gpu::SyncToken& is required as implicit auto
    // decays removing the reference
    static auto get_sync_token =
        [](ImportedResource* imported_resource) -> gpu::SyncToken& {
      CHECK(imported_resource);
      return imported_resource->resource.mutable_sync_token();
    };

    shared_image_interface->VerifySyncTokens(imports, get_sync_token);
  } else {
    static auto check_is_verified =
        [](const ImportedResource* imported_resource) {
          CHECK(imported_resource);
          return !imported_resource->resource.sync_token().verified_flush();
        };

    DCHECK(std::none_of(imports.begin(), imports.end(), check_is_verified));
  }

  list->reserve(list->size() + imports.size());
  for (ImportedResource* imported : imports) {
    list->push_back(imported->resource);
    imported->exported_count++;
  }
}

void ClientResourceProvider::ReceiveReturnsFromParent(
    std::vector<ReturnedResource> resources) {
  TRACE_EVENT0("viz", __PRETTY_FUNCTION__);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<base::OnceClosure> impl_release_callbacks;
  impl_release_callbacks.reserve(resources.size());
  std::vector<base::OnceClosure> main_impl_release_callbacks;
  main_impl_release_callbacks.reserve(resources.size());

  for (const auto& returned : resources) {
    auto imported_it = imported_resources_.find(returned.id);
    if (imported_it == imported_resources_.end()) {
      // TODO(zmo): In theory, everything being returned should already be in
      // the imported list. We should figure out why this happens.
      continue;
    }
    auto& imported = imported_it->second;

    DCHECK_GE(imported.exported_count, returned.count);
    imported.exported_count -= returned.count;
    imported.returned_lost |= returned.lost;

    if (imported.exported_count) {
      // Can't remove the imported yet so go to the next, looking for the next
      // returned resource.
      continue;
    }

    // Save the sync token only when the exported count is going to 0. Or IOW
    // drop all by the last returned sync token.
    if (returned.sync_token.HasData()) {
      imported.returned_sync_token = returned.sync_token;
    }

    if (!imported.marked_for_deletion) {
      // Not going to remove the imported yet so go to the next, looking for the
      // next returned resource.
      continue;
    }

    if (imported.main_thread_release_callback) {
      main_impl_release_callbacks.push_back(
          base::BindOnce(std::move(imported.main_thread_release_callback),
                         imported.returned_sync_token, imported.returned_lost));
    }

    if (imported.impl_release_callback) {
      impl_release_callbacks.push_back(
          base::BindOnce(std::move(imported.impl_release_callback),
                         imported.returned_sync_token, imported.returned_lost));
    }

    imported_resources_.erase(imported_it);
  }

  for (auto& cb : impl_release_callbacks) {
    std::move(cb).Run();
  }

  if (!main_impl_release_callbacks.empty()) {
    BatchMainReleaseCallbacks(std::move(main_impl_release_callbacks));
  }
}

ResourceId ClientResourceProvider::ImportResource(
    const TransferableResource& resource,
    ReleaseCallback impl_release_callback,
    ReleaseCallback main_thread_release_callback,
    ResourceEvictedCallback evicted_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Clients are not allowed to import any empty resource.
  CHECK(!resource.is_empty());
  ResourceId id = id_generator_.GenerateNextId();
  auto result = imported_resources_.emplace(
      id, ImportedResource(id, resource, std::move(impl_release_callback),
                           std::move(main_thread_release_callback),
                           std::move(evicted_callback)));
  CHECK(result.second);  // If false, the id was already in the map.
  return id;
}

void ClientResourceProvider::RemoveImportedResource(ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = imported_resources_.find(id);
  CHECK(it != imported_resources_.end());
  ImportedResource& imported = it->second;
  imported.marked_for_deletion = true;
  // We clear the callback here, as we will hold onto `imported` until it has
  // been returned. Which could occur after the lifetime of the importer.
  imported.evicted_callback = ResourceEvictedCallback();
  if (imported.exported_count == 0) {
    TakeOrRunResourceReleases(batch_release_callbacks_, imported);
    imported_resources_.erase(it);
  }
}

void ClientResourceProvider::ReleaseAllExportedResources(bool lose) {
  const bool batch =
      base::FeatureList::IsEnabled(features::kBatchResourceRelease);
  if (batch) {
    batch_main_release_callbacks_.reserve(imported_resources_.size());
  }

  for (auto it = imported_resources_.begin();
       it != imported_resources_.end();) {
    ImportedResource& imported = it->second;
    if (!imported.exported_count) {
      // Not exported, not up for consideration to be returned here.
      it++;
      continue;
    }
    imported.exported_count = 0;
    imported.returned_lost |= lose;
    if (!imported.marked_for_deletion) {
      // Was exported, but not removed by the client, so don't return it
      // yet.
      it++;
      continue;
    }

    TakeOrRunResourceReleases(batch, imported);
    // Was exported and removed by the client, so erase it.
    it = imported_resources_.erase(it);
  }

  BatchResourceRelease();
}

void ClientResourceProvider::ShutdownAndReleaseAllResources() {
  const bool batch =
      base::FeatureList::IsEnabled(features::kBatchResourceRelease);
  if (batch) {
    batch_main_release_callbacks_.reserve(imported_resources_.size());
  }

  for (auto& pair : imported_resources_) {
    ImportedResource& imported = pair.second;

#if DCHECK_IS_ON()
    // If this is false, then the resource has not been removed via
    // RemoveImportedResource(), and all resources should be removed before
    // we resort to marking resources as lost during shutdown.
    DCHECK(imported.marked_for_deletion)
        << "id: " << pair.first << " from:\n"
        << imported.stack_trace.ToString() << "===";
    DCHECK(imported.exported_count) << "id: " << pair.first << " from:\n"
                                    << imported.stack_trace.ToString() << "===";
#endif

    imported.returned_lost = true;
    TakeOrRunResourceReleases(batch, imported);
  }
  imported_resources_.clear();
  BatchResourceRelease();
}

void ClientResourceProvider::ValidateResource(ResourceId id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(id);
  DCHECK(imported_resources_.find(id) != imported_resources_.end());
}

bool ClientResourceProvider::InUseByConsumer(ResourceId id) {
  auto it = imported_resources_.find(id);
  CHECK(it != imported_resources_.end());
  ImportedResource& imported = it->second;
  return imported.exported_count > 0 || imported.returned_lost;
}

void ClientResourceProvider::SetEvicted(bool evicted) {
  if (evicted_ == evicted) {
    return;
  }
  evicted_ = evicted;
  HandleEviction();
}

void ClientResourceProvider::SetVisible(bool visible) {
  if (visible_ == visible) {
    return;
  }
  visible_ = visible;
  HandleEviction();
}

ClientResourceProvider::ScopedBatchResourcesRelease
ClientResourceProvider::CreateScopedBatchResourcesRelease() {
  // Typically `batch_release_callbacks_` will remain `true` until the callback
  // `BatchResourceRelease` is called.
  //
  // However other internal batching can lead to this being `false` as bot
  // `ReleaseAllExportedResources` and `ShutdownAndReleaseAllResources`.
  batch_release_callbacks_ =
      base::FeatureList::IsEnabled(features::kBatchResourceRelease);
  return ScopedBatchResourcesReleaseImpl(
      base::BindOnce(&ClientResourceProvider::BatchResourceRelease,
                     weak_factory_.GetWeakPtr()));
}

void ClientResourceProvider::HandleEviction() {
  // The eviction and visibility change messages are racy. The Renderer
  // Main-thread can be slow enough that we are still considered visible when
  // the eviction signal is received. We do not count the locked resources
  // solely when evicted. Instead we await the visibility change, as the Main
  // thread may be in the process of unlocking resources, and the visibility
  // change itself will attempt to free more resources.
  if (!evicted_ || visible_) {
    return;
  }
  int locked = 0;
  size_t total_mem = 0u;
  std::unordered_map<TransferableResource::ResourceSource, size_t>
      mem_per_source;
  std::vector<ResourceId> ids_to_unlock;
  for (auto& [id, imported] : imported_resources_) {
    if (!imported.marked_for_deletion) {
      ++locked;
      auto resource_source = imported.resource.resource_source;
      size_t resource_mem = imported.resource.GetFormat().EstimatedSizeInBytes(
          imported.resource.GetSize());
      total_mem += resource_mem;
      mem_per_source[resource_source] += resource_mem;

      if (!base::FeatureList::IsEnabled(features::kEvictionUnlocksResources) ||
          !imported.evicted_callback) {
        continue;
      }
      // We can't call imported.evicted_callback here, nor can we save them and
      // them in a for-loop after. This is because callbacks may trigger
      // ClientResourceProvider::RemoveImportedResource().
      ids_to_unlock.push_back(id);
    }
  }

  for (const auto id : ids_to_unlock) {
    auto imported = imported_resources_.find(id);
    if (imported == imported_resources_.end()) {
      continue;
    }
    std::move(imported->second.evicted_callback).Run();
  }

  // Only report when there are locked resources. Evictions where all resources
  // can be released are not interesting
  if (!locked) {
    return;
  }
  size_t total_mem_in_kb = total_mem / 1024u;
  CustomUmaHistogramMemoryKB("Memory.Renderer.EvictedLockedResources.Total",
                             total_mem_in_kb);
  for (auto& [source, size] : mem_per_source) {
    if (size) {
      ReportResourceSourceUsage(source, size);
    }
  }
}

void ClientResourceProvider::BatchMainReleaseCallbacks(
    std::vector<base::OnceClosure> release_callbacks) {
  if (threaded_release_callbacks_supported_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::vector<base::OnceClosure> release_callbacks,
               scoped_refptr<base::SequencedTaskRunner> impl_task_runner,
               base::OnceClosure completed_callback) {
              for (auto& cb : release_callbacks) {
                std::move(cb).Run();
              }
              std::move(completed_callback).Run();
            },
            std::move(release_callbacks), impl_task_runner_,
            base::BindPostTask(impl_task_runner_, resource_flush_callback_)));
  } else {
    for (auto& cb : release_callbacks) {
      std::move(cb).Run();
    }
  }
}

void ClientResourceProvider::BatchResourceRelease() {
  batch_release_callbacks_ = false;
  if (!batch_main_release_callbacks_.empty()) {
    BatchMainReleaseCallbacks(std::move(batch_main_release_callbacks_));
  }
  batch_main_release_callbacks_.clear();
}

void ClientResourceProvider::TakeOrRunResourceReleases(
    bool batch,
    ImportedResource& imported) {
  if (batch) {
    if (imported.impl_release_callback) {
      std::move(imported.impl_release_callback)
          .Run(imported.returned_sync_token, imported.returned_lost);
    }
    if (imported.main_thread_release_callback) {
      batch_main_release_callbacks_.push_back(
          base::BindOnce(std::move(imported.main_thread_release_callback),
                         imported.returned_sync_token, imported.returned_lost));
    }
  } else {
    imported.RunReleaseCallbacks();
  }
}

size_t ClientResourceProvider::num_resources_for_testing() const {
  return imported_resources_.size();
}

}  // namespace viz
