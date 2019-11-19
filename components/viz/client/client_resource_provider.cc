// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/client_resource_provider.h"

#include "base/bind.h"
#include "base/bits.h"
#include "base/debug/stack_trace.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace viz {

struct ClientResourceProvider::ImportedResource {
  TransferableResource resource;
  std::unique_ptr<SingleReleaseCallback> release_callback;
  int exported_count = 0;
  bool marked_for_deletion = false;

  gpu::SyncToken returned_sync_token;
  bool returned_lost = false;

#if DCHECK_IS_ON()
  base::debug::StackTrace stack_trace;
#endif

  ImportedResource(ResourceId id,
                   const TransferableResource& resource,
                   std::unique_ptr<SingleReleaseCallback> release_callback)
      : resource(resource),
        release_callback(std::move(release_callback)),
        // If the resource is immediately deleted, it returns the same SyncToken
        // it came with. The client may need to wait on that before deleting the
        // backing or reusing it.
        returned_sync_token(resource.mailbox_holder.sync_token) {
    // Replace the |resource| id with the local id from this
    // ClientResourceProvider.
    this->resource.id = id;
  }
  ~ImportedResource() = default;

  ImportedResource(ImportedResource&&) = default;
  ImportedResource& operator=(ImportedResource&&) = default;
};

ClientResourceProvider::ClientResourceProvider(
    bool verified_sync_tokens_required)
    : verified_sync_tokens_required_(verified_sync_tokens_required) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

ClientResourceProvider::~ClientResourceProvider() {
  // If this fails, there are outstanding resources exported that should be
  // lost and returned by calling ShutdownAndReleaseAllResources(), or there
  // are resources that were imported without being removed by
  // RemoveImportedResource(). In either case, calling
  // ShutdownAndReleaseAllResources() will help, as it will report which
  // resources were imported without being removed as well.
  DCHECK(imported_resources_.empty());
}

gpu::SyncToken ClientResourceProvider::GenerateSyncTokenHelper(
    gpu::gles2::GLES2Interface* gl) {
  DCHECK(gl);
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  DCHECK(sync_token.HasData() ||
         gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  return sync_token;
}

gpu::SyncToken ClientResourceProvider::GenerateSyncTokenHelper(
    gpu::raster::RasterInterface* ri) {
  DCHECK(ri);
  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  DCHECK(sync_token.HasData() ||
         ri->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  return sync_token;
}

void ClientResourceProvider::PrepareSendToParent(
    const std::vector<ResourceId>& export_ids,
    std::vector<TransferableResource>* list,
    ContextProvider* context_provider) {
  auto cb = base::BindOnce(
      [](scoped_refptr<ContextProvider> context_provider,
         std::vector<GLbyte*>* tokens) {
        context_provider->ContextGL()->VerifySyncTokensCHROMIUM(tokens->data(),
                                                                tokens->size());
      },
      base::WrapRefCounted(context_provider));
  PrepareSendToParentInternal(export_ids, list, std::move(cb));
}

void ClientResourceProvider::PrepareSendToParent(
    const std::vector<ResourceId>& export_ids,
    std::vector<TransferableResource>* list,
    RasterContextProvider* context_provider) {
  PrepareSendToParentInternal(
      export_ids, list,
      base::BindOnce(
          [](scoped_refptr<RasterContextProvider> context_provider,
             std::vector<GLbyte*>* tokens) {
            context_provider->RasterInterface()->VerifySyncTokensCHROMIUM(
                tokens->data(), tokens->size());
          },
          base::WrapRefCounted(context_provider)));
}

void ClientResourceProvider::PrepareSendToParentInternal(
    const std::vector<ResourceId>& export_ids,
    std::vector<TransferableResource>* list,
    base::OnceCallback<void(std::vector<GLbyte*>* tokens)> verify_sync_tokens) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This function goes through the array multiple times, store the resources
  // as pointers so we don't have to look up the resource id multiple times.
  // Make sure the maps do not change while these vectors are alive or they
  // will become invalid.
  std::vector<ImportedResource*> imports;
  imports.reserve(export_ids.size());
  for (const ResourceId id : export_ids) {
    auto it = imported_resources_.find(id);
    DCHECK(it != imported_resources_.end());
    imports.push_back(&it->second);
  }

  // Lazily create any mailboxes and verify all unverified sync tokens.
  std::vector<GLbyte*> unverified_sync_tokens;
  if (verified_sync_tokens_required_) {
    for (ImportedResource* imported : imports) {
      if (!imported->resource.is_software &&
          !imported->resource.mailbox_holder.sync_token.verified_flush()) {
        unverified_sync_tokens.push_back(
            imported->resource.mailbox_holder.sync_token.GetData());
      }
    }
  }

  if (!unverified_sync_tokens.empty()) {
    DCHECK(verified_sync_tokens_required_);
    DCHECK(verify_sync_tokens);
    std::move(verify_sync_tokens).Run(&unverified_sync_tokens);
  }

  for (ImportedResource* imported : imports) {
    list->push_back(imported->resource);
    imported->exported_count++;
  }
}

void ClientResourceProvider::ReceiveReturnsFromParent(
    std::vector<ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // |imported_resources_| is a set sorted by id, so if we sort the incoming
  // |resources| list by id also, we can walk through both structures in order,
  // replacing  things to be removed from imported_resources_ with things that
  // we want to keep, making the removal of all items O(N+MlogM) instead of
  // O(N*M). This algorithm is modelled after std::remove_if() but with a
  // parallel walk through |resources| instead of an O(logM) lookup into
  // |resources| for each step.
  std::sort(resources.begin(), resources.end(),
            [](const ReturnedResource& a, const ReturnedResource& b) {
              return a.id < b.id;
            });

  auto returned_it = resources.begin();
  auto returned_end = resources.end();
  auto imported_keep_end_it = imported_resources_.begin();
  auto imported_compare_it = imported_resources_.begin();
  auto imported_end = imported_resources_.end();

  std::vector<base::OnceClosure> release_callbacks;
  release_callbacks.reserve(resources.size());

  while (imported_compare_it != imported_end) {
    if (returned_it == returned_end) {
      // Nothing else to remove from |imported_resources_|.
      if (imported_keep_end_it == imported_compare_it) {
        // We didn't remove anything, so we're done.
        break;
      }
      // If something was removed, we need to shift everything into the empty
      // space that was made.
      *imported_keep_end_it = std::move(*imported_compare_it);
      ++imported_keep_end_it;
      ++imported_compare_it;
      continue;
    }

    const ResourceId returned_resource_id = returned_it->id;
    const ResourceId imported_resource_id = imported_compare_it->first;

    if (returned_resource_id != imported_resource_id) {
      // They're both sorted, and everything being returned should already
      // be in the imported list. So we should be able to walk through the
      // resources list in order, and find each id in both sets when present.
      // That means if it's not matching, we should be above it in the sorted
      // order of the |imported_resources_|, allowing us to get to it by
      // continuing to traverse |imported_resources_|.
      DCHECK_GT(returned_resource_id, imported_resource_id);
      // This means we want to keep the resource at |imported_compare_it|. So go
      // to the next. If we removed anything previously and made empty space,
      // fill it as we move.
      if (imported_keep_end_it != imported_compare_it)
        *imported_keep_end_it = std::move(*imported_compare_it);
      ++imported_keep_end_it;
      ++imported_compare_it;
      continue;
    }

    const ReturnedResource& returned = *returned_it;
    ImportedResource& imported = imported_compare_it->second;

    DCHECK_GE(imported.exported_count, returned.count);
    imported.exported_count -= returned.count;
    imported.returned_lost |= returned.lost;

    if (imported.exported_count) {
      // Can't remove the imported yet so go to the next, looking for the next
      // returned resource.
      ++returned_it;
      // The same ResourceId may appear multiple times (in a row) in the
      // returned set. In that case, we do not want to increment the iterators
      // in |imported_resources_| yet. So we don't increment them here, and let
      // the next iteration of the loop do so.
      continue;
    }

    // Save the sync token only when the exported count is going to 0. Or IOW
    // drop all by the last returned sync token.
    if (returned.sync_token.HasData()) {
      DCHECK(!imported.resource.is_software);
      imported.returned_sync_token = returned.sync_token;
    }

    if (!imported.marked_for_deletion) {
      // Not going to remove the imported yet so go to the next, looking for the
      // next returned resource.
      ++returned_it;
      // If we removed anything previously and made empty space, fill it as we
      // move.
      if (imported_keep_end_it != imported_compare_it)
        *imported_keep_end_it = std::move(*imported_compare_it);
      ++imported_keep_end_it;
      ++imported_compare_it;
      continue;
    }

    // Save the ReleaseCallback to run after iterating through internal data
    // structures, in case it calls back into this class.
    auto run_callback = [](std::unique_ptr<SingleReleaseCallback> cb,
                           const gpu::SyncToken& sync_token, bool lost) {
      cb->Run(sync_token, lost);
      // |cb| is destroyed when leaving scope.
    };
    release_callbacks.push_back(
        base::BindOnce(run_callback, std::move(imported.release_callback),
                       imported.returned_sync_token, imported.returned_lost));
    // We don't want to keep this resource, so we leave |imported_keep_end_it|
    // pointing to it (since it points past the end of what we're keeping). We
    // do move |imported_compare_it| in order to move on to the next resource.
    ++imported_compare_it;
    // The imported iterator is pointing at the next element already, so no need
    // to increment it, and we can move on to looking for the next returned
    // resource.
    ++returned_it;
  }

  // Drop anything that was moved after the |imported_end| iterator in a single
  // O(N) operation.
  if (imported_keep_end_it != imported_compare_it)
    imported_resources_.erase(imported_keep_end_it, imported_resources_.end());

  for (auto& cb : release_callbacks)
    std::move(cb).Run();
}

ResourceId ClientResourceProvider::ImportResource(
    const TransferableResource& resource,
    std::unique_ptr<SingleReleaseCallback> release_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResourceId id = next_id_++;
  auto result = imported_resources_.emplace(
      id, ImportedResource(id, resource, std::move(release_callback)));
  DCHECK(result.second);  // If false, the id was already in the map.
  return id;
}

void ClientResourceProvider::RemoveImportedResource(ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = imported_resources_.find(id);
  DCHECK(it != imported_resources_.end());
  ImportedResource& imported = it->second;
  imported.marked_for_deletion = true;
  if (imported.exported_count == 0) {
    imported.release_callback->Run(imported.returned_sync_token,
                                   imported.returned_lost);
    imported_resources_.erase(it);
  }
}

void ClientResourceProvider::ReleaseAllExportedResources(bool lose) {
  auto release_and_remove =
      [lose](std::pair<ResourceId, ImportedResource>& pair) {
        ImportedResource& imported = pair.second;
        if (!imported.exported_count) {
          // Not exported, not up for consideration to be returned here.
          return false;
        }
        imported.exported_count = 0;
        imported.returned_lost |= lose;
        if (!imported.marked_for_deletion) {
          // Was exported, but not removed by the client, so don't return it
          // yet.
          return false;
        }

        imported.release_callback->Run(imported.returned_sync_token,
                                       imported.returned_lost);
        // Was exported and removed by the client, so return it now.
        return true;
      };

  // This will run |release_and_remove| on each element of |imported_resources_|
  // and drop any resources from the set that it requests.
  base::EraseIf(imported_resources_, release_and_remove);
}

void ClientResourceProvider::ShutdownAndReleaseAllResources() {
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

    imported.release_callback->Run(imported.returned_sync_token,
                                   /*is_lost=*/true);
  }
  imported_resources_.clear();
}

ClientResourceProvider::ScopedSkSurface::ScopedSkSurface(
    GrContext* gr_context,
    sk_sp<SkColorSpace> color_space,
    GLuint texture_id,
    GLenum texture_target,
    const gfx::Size& size,
    ResourceFormat format,
    bool can_use_lcd_text,
    int msaa_sample_count) {
  GrGLTextureInfo texture_info;
  texture_info.fID = texture_id;
  texture_info.fTarget = texture_target;
  texture_info.fFormat = TextureStorageFormat(format);
  GrBackendTexture backend_texture(size.width(), size.height(),
                                   GrMipMapped::kNo, texture_info);
  SkSurfaceProps surface_props = ComputeSurfaceProps(can_use_lcd_text);
  // This type is used only for gpu raster, which implies gpu compositing.
  bool gpu_compositing = true;
  surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      gr_context, backend_texture, kTopLeft_GrSurfaceOrigin, msaa_sample_count,
      ResourceFormatToClosestSkColorType(gpu_compositing, format), color_space,
      &surface_props);
}

ClientResourceProvider::ScopedSkSurface::~ScopedSkSurface() {
  if (surface_)
    surface_->flush();
}

SkSurfaceProps ClientResourceProvider::ScopedSkSurface::ComputeSurfaceProps(
    bool can_use_lcd_text) {
  uint32_t flags = 0;
  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }
  return surface_props;
}

void ClientResourceProvider::ValidateResource(ResourceId id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(id);
  DCHECK(imported_resources_.find(id) != imported_resources_.end());
}

bool ClientResourceProvider::InUseByConsumer(ResourceId id) {
  auto it = imported_resources_.find(id);
  DCHECK(it != imported_resources_.end());
  ImportedResource& imported = it->second;
  return imported.exported_count > 0 || imported.returned_lost;
}

size_t ClientResourceProvider::num_resources_for_testing() const {
  return imported_resources_.size();
}

}  // namespace viz
