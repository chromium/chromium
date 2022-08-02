// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_saved_frame.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/transition_utils.h"
#include "components/viz/service/surfaces/surface.h"
#include "services/viz/public/mojom/compositing/compositor_render_pass_id.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"

namespace viz {

namespace {
constexpr gfx::Size kDefaultTextureSizeForTesting = gfx::Size(20, 20);

constexpr auto kResultFormat = CopyOutputRequest::ResultFormat::RGBA;
constexpr auto kResultDestination =
    CopyOutputRequest::ResultDestination::kNativeTextures;

SurfaceSavedFrame::RenderPassDrawData GetRootRenderPassDrawData(
    Surface* surface) {
  const auto& frame = surface->GetActiveFrame();
  DCHECK(!frame.render_pass_list.empty());
  const auto& root_render_pass = frame.render_pass_list.back();
  return {*root_render_pass, 1.f};
}

// Returns the index of |render_pass_id| in |shared_elements| if the id
// corresponds to an element in the given list. Otherwise returns the size of
// |shared_elements| vector.
size_t GetSharedPassIndex(
    const std::vector<CompositorFrameTransitionDirective::SharedElement>&
        shared_elements,
    CompositorRenderPassId render_pass_id) {
  size_t shared_element_index = 0;
  for (; shared_element_index < shared_elements.size();
       ++shared_element_index) {
    if (shared_elements[shared_element_index].render_pass_id ==
        render_pass_id) {
      break;
    }
  }
  return shared_element_index;
}

}  // namespace

SurfaceSavedFrame::SurfaceSavedFrame(
    CompositorFrameTransitionDirective directive,
    TransitionDirectiveCompleteCallback directive_finished_callback)
    : directive_(std::move(directive)),
      directive_finished_callback_(std::move(directive_finished_callback)) {
  // We should only be constructing a saved frame from a save directive.
  DCHECK_EQ(directive_.type(), CompositorFrameTransitionDirective::Type::kSave);
}

SurfaceSavedFrame::~SurfaceSavedFrame() {
  if (directive_finished_callback_)
    std::move(directive_finished_callback_).Run(directive_.sequence_id());
}

base::flat_set<SharedElementResourceId> SurfaceSavedFrame::GetEmptyResourceIds()
    const {
  base::flat_set<SharedElementResourceId> result;
  for (auto& shared_element : directive_.shared_elements())
    if (shared_element.render_pass_id.is_null())
      result.insert(shared_element.shared_element_resource_id);
  return result;
}

bool SurfaceSavedFrame::IsValid() const {
  bool result = valid_result_count_ == ExpectedResultCount();
  // If this saved frame is valid, then we should have a frame result.
  DCHECK(!result || frame_result_);
  return result;
}

void SurfaceSavedFrame::RequestCopyOfOutput(Surface* surface) {
  DCHECK(surface->HasActiveFrame());

  if (directive_.is_renderer_driven_animation()) {
    // TODO(khushalsagar) : This should be the only mode once renderer based SET
    // lands.
    copy_root_render_pass_ = false;
    CopyUsingOriginalFrame(surface);
  } else {
    CopyUsingCleanFrame(surface);
  }

  DCHECK_EQ(copy_request_count_, ExpectedResultCount());
}

void SurfaceSavedFrame::CopyUsingCleanFrame(Surface* surface) {
  const auto& root_draw_data = GetRootRenderPassDrawData(surface);
  // Bind kRoot and root geometry information to the callback.
  auto root_request = std::make_unique<CopyOutputRequest>(
      kResultFormat, kResultDestination,
      base::BindOnce(&SurfaceSavedFrame::NotifyCopyOfOutputComplete,
                     weak_factory_.GetWeakPtr(), ResultType::kRoot, 0,
                     root_draw_data));
  root_request->set_result_task_runner(base::ThreadTaskRunnerHandle::Get());
  copy_request_count_ = 0;

  // Only one copy request implies only the root element needs to be copied so
  // the frame must not have any shared element render passes.
  if (ExpectedResultCount() == 1) {
    surface->RequestCopyOfOutputOnRootRenderPass(std::move(root_request));
    copy_request_count_++;
    return;
  }

  // If the directive includes shared elements then we need to create a new
  // CompositorFrame with render passes that remove these elements. The strategy
  // is as follows :
  //
  // 1) For each render pass create a deep copy with identical content that will
  //    draw (directly or indirectly) to the onscreen buffer.
  //
  // 2) If a render pass is a shared element or is "tainted" (includes content
  //    from a shared element), create a new "clean" render pass with the
  //    following modifications :
  //
  //    - RenderPassDrawQuads which are 1:1 with a shared element are removed.
  //
  //    - RenderPassDrawQuads which are tainted are replaced with the equivalent
  //      clean render pass.
  //
  //    The new clean render passes are only used to issue copy requests and
  //    never drawn to the onscreen buffer.
  const auto& active_frame = surface->GetActiveFrame();
  CompositorRenderPassId max_id = CompositorRenderPassId(0);
  base::flat_map<CompositorRenderPassId, CompositorRenderPassId>
      tainted_to_clean_pass_ids;
  PrepareForCopy(active_frame.render_pass_list, max_id,
                 tainted_to_clean_pass_ids);

  TransitionUtils::FilterCallback filter_callback = base::BindRepeating(
      &SurfaceSavedFrame::FilterSharedElementAndTaintedQuads,
      base::Unretained(this), base::Unretained(&tainted_to_clean_pass_ids));

  CompositorFrame clean_frame;
  clean_frame.metadata = active_frame.metadata.Clone();
  clean_frame.resource_list = active_frame.resource_list;

  for (auto& render_pass : active_frame.render_pass_list) {
    const auto original_render_pass_id = render_pass->id;
    CompositorRenderPass* pass_for_clean_copy = nullptr;

    auto it = tainted_to_clean_pass_ids.find(original_render_pass_id);
    if (it != tainted_to_clean_pass_ids.end()) {
      auto clean_pass = TransitionUtils::CopyPassWithQuadFiltering(
          *render_pass, filter_callback);
      it->second = max_id = clean_pass->id =
          TransitionUtils::NextRenderPassId(max_id);
      pass_for_clean_copy = clean_pass.get();
      clean_frame.render_pass_list.push_back(std::move(clean_pass));
    }

    // Deep copy of the original pass propagating copy requests.
    auto copy_requests = std::move(render_pass->copy_requests);
    auto duplicate_pass = render_pass->DeepCopy();
    duplicate_pass->copy_requests = std::move(copy_requests);
    if (!pass_for_clean_copy)
      pass_for_clean_copy = duplicate_pass.get();
    clean_frame.render_pass_list.push_back(std::move(duplicate_pass));

    if (auto request = CreateCopyRequestIfNeeded(
            *render_pass, active_frame.render_pass_list)) {
      pass_for_clean_copy->copy_requests.push_back(std::move(request));
      copy_request_count_++;
    }

    bool is_root_pass =
        original_render_pass_id == active_frame.render_pass_list.back()->id;
    if (is_root_pass) {
      DCHECK(root_request);
      pass_for_clean_copy->copy_requests.push_back(std::move(root_request));
      copy_request_count_++;
    }
  }

  clean_surface_.emplace(surface, std::move(clean_frame));
}

void SurfaceSavedFrame::CopyUsingOriginalFrame(Surface* surface) {
  const auto& active_frame = surface->GetActiveFrame();
  for (const auto& render_pass : active_frame.render_pass_list) {
    if (auto request = CreateCopyRequestIfNeeded(
            *render_pass, active_frame.render_pass_list)) {
      surface->RequestCopyOfOutputOnActiveFrameRenderPassId(std::move(request),
                                                            render_pass->id);
      copy_request_count_++;
    }
  }
}

std::unique_ptr<CopyOutputRequest> SurfaceSavedFrame::CreateCopyRequestIfNeeded(
    const CompositorRenderPass& render_pass,
    const CompositorRenderPassList& render_pass_list) const {
  auto shared_pass_index =
      GetSharedPassIndex(directive_.shared_elements(), render_pass.id);
  if (shared_pass_index >= directive_.shared_elements().size())
    return nullptr;

  RenderPassDrawData draw_data(
      render_pass, TransitionUtils::ComputeAccumulatedOpacity(render_pass_list,
                                                              render_pass.id));
  auto request = std::make_unique<CopyOutputRequest>(
      kResultFormat, kResultDestination,
      base::BindOnce(&SurfaceSavedFrame::NotifyCopyOfOutputComplete,
                     weak_factory_.GetWeakPtr(), ResultType::kShared,
                     shared_pass_index, draw_data));
  request->set_result_task_runner(base::ThreadTaskRunnerHandle::Get());
  return request;
}

void SurfaceSavedFrame::ReleaseSurface() {
  clean_surface_.reset();
}

void SurfaceSavedFrame::PrepareForCopy(
    const CompositorRenderPassList& render_passes,
    CompositorRenderPassId& max_id,
    base::flat_map<CompositorRenderPassId, CompositorRenderPassId>&
        tainted_to_clean_pass_ids) const {
  for (const auto& pass : render_passes) {
    max_id = std::max(max_id, pass->id);

    for (auto it = pass->quad_list.BackToFrontBegin();
         it != pass->quad_list.BackToFrontEnd(); ++it) {
      const DrawQuad* quad = *it;
      if (quad->material != DrawQuad::Material::kCompositorRenderPass)
        continue;

      auto quad_pass_id =
          CompositorRenderPassDrawQuad::MaterialCast(quad)->render_pass_id;
      if (IsSharedElementRenderPass(quad_pass_id) ||
          tainted_to_clean_pass_ids.contains(quad_pass_id)) {
        tainted_to_clean_pass_ids[pass->id] = CompositorRenderPassId(0);
        break;
      }
    }
  }
}

bool SurfaceSavedFrame::FilterSharedElementAndTaintedQuads(
    const base::flat_map<CompositorRenderPassId, CompositorRenderPassId>*
        tainted_to_clean_pass_ids,
    const DrawQuad& quad,
    CompositorRenderPass& copy_pass) const {
  if (quad.material != DrawQuad::Material::kCompositorRenderPass)
    return false;
  const auto& pass_quad = *CompositorRenderPassDrawQuad::MaterialCast(&quad);

  // Skip drawing shared elements embedded inside render passes.
  if (IsSharedElementRenderPass(pass_quad.render_pass_id))
    return true;

  // Replace tainted quads with equivalent clean render passes.
  auto it = tainted_to_clean_pass_ids->find(pass_quad.render_pass_id);
  if (it != tainted_to_clean_pass_ids->end()) {
    DCHECK_NE(it->second, CompositorRenderPassId(0));
    copy_pass.CopyFromAndAppendRenderPassDrawQuad(&pass_quad, it->second);
    return true;
  }

  return false;
}

bool SurfaceSavedFrame::IsSharedElementRenderPass(
    CompositorRenderPassId pass_id) const {
  const auto& shared_elements = directive_.shared_elements();
  return GetSharedPassIndex(shared_elements, pass_id) < shared_elements.size();
}

size_t SurfaceSavedFrame::ExpectedResultCount() const {
  base::flat_set<CompositorRenderPassId> ids;
  for (auto& shared_element : directive_.shared_elements())
    if (!shared_element.render_pass_id.is_null())
      ids.insert(shared_element.render_pass_id);
  // Add 1 if we need to copy root render pass.
  return ids.size() + copy_root_render_pass_;
}

void SurfaceSavedFrame::NotifyCopyOfOutputComplete(
    ResultType type,
    size_t shared_index,
    const RenderPassDrawData& data,
    std::unique_ptr<CopyOutputResult> output_copy) {
  DCHECK_GT(copy_request_count_, 0u);
  // Even if we early out, we update the count since we are no longer waiting
  // for this result.
  if (--copy_request_count_ == 0) {
    std::move(directive_finished_callback_).Run(directive_.sequence_id());
    clean_surface_.reset();
  }

  // Return if the result is empty.
  if (output_copy->IsEmpty()) {
    LOG(ERROR) << "SurfaceSavedFrame copy output result for shared index "
               << shared_index << " is empty.";
    return;
  }

  ++valid_result_count_;
  if (!frame_result_) {
    frame_result_.emplace();
    // Resize to the number of shared elements, even if some will be nullopts.
    frame_result_->shared_results.resize(directive_.shared_elements().size());
  }

  OutputCopyResult* slot = nullptr;
  if (type == ResultType::kRoot) {
    slot = &frame_result_->root_result;
  } else {
    DCHECK(type == ResultType::kShared);
    CHECK_LT(shared_index, frame_result_->shared_results.size());
    DCHECK(!frame_result_->shared_results[shared_index]);
    slot = &frame_result_->shared_results[shared_index].emplace();
  }

  DCHECK(slot);
  DCHECK_EQ(output_copy->size(), data.size);
  DCHECK_EQ(output_copy->format(), CopyOutputResult::Format::RGBA);
  if (output_copy->destination() ==
      CopyOutputResult::Destination::kSystemMemory) {
    slot->bitmap = output_copy->ScopedAccessSkBitmap().GetOutScopedBitmap();
    slot->is_software = true;
  } else {
    auto output_copy_texture = *output_copy->GetTextureResult();
    slot->mailbox = output_copy_texture.planes[0].mailbox;
    slot->sync_token = output_copy_texture.planes[0].sync_token;
    slot->color_space = output_copy_texture.color_space;

    CopyOutputResult::ReleaseCallbacks release_callbacks =
        output_copy->TakeTextureOwnership();
    // CopyOutputResults carrying RGBA format contain a single texture, there
    // should be only one release callback:
    DCHECK_EQ(1u, release_callbacks.size());
    slot->release_callback = std::move(release_callbacks[0]);
    slot->is_software = false;
  }
  slot->draw_data = data;
}

absl::optional<SurfaceSavedFrame::FrameResult> SurfaceSavedFrame::TakeResult() {
  return std::exchange(frame_result_, absl::nullopt);
}

void SurfaceSavedFrame::CompleteSavedFrameForTesting() {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kDefaultTextureSizeForTesting.width(),
                                 kDefaultTextureSizeForTesting.height()));

  frame_result_.emplace();
  frame_result_->root_result.bitmap = std::move(bitmap);
  frame_result_->root_result.draw_data.size = kDefaultTextureSizeForTesting;
  frame_result_->root_result.draw_data.target_transform.MakeIdentity();
  frame_result_->root_result.draw_data.opacity = 1.f;
  frame_result_->root_result.is_software = true;

  frame_result_->shared_results.resize(directive_.shared_elements().size());
  copy_request_count_ = 0;
  valid_result_count_ = ExpectedResultCount();
  weak_factory_.InvalidateWeakPtrs();
  DCHECK(IsValid());
}

SurfaceSavedFrame::RenderPassDrawData::RenderPassDrawData() = default;
SurfaceSavedFrame::RenderPassDrawData::RenderPassDrawData(
    const CompositorRenderPass& render_pass,
    float opacity)
    : opacity(opacity) {
  // During copy request, the origin for |render_pass|'s output_rect is mapped
  // to the origin of the texture in the result. We account for that here.
  size = render_pass.output_rect.size();

  target_transform = render_pass.transform_to_root_target;
  target_transform.Translate(render_pass.output_rect.x(),
                             render_pass.output_rect.y());
}

SurfaceSavedFrame::OutputCopyResult::OutputCopyResult() = default;
SurfaceSavedFrame::OutputCopyResult::OutputCopyResult(
    OutputCopyResult&& other) {
  *this = std::move(other);
}

SurfaceSavedFrame::OutputCopyResult::~OutputCopyResult() {
  if (release_callback)
    std::move(release_callback).Run(sync_token, /*is_lost=*/false);
}

SurfaceSavedFrame::OutputCopyResult&
SurfaceSavedFrame::OutputCopyResult::operator=(OutputCopyResult&& other) {
  mailbox = std::move(other.mailbox);
  other.mailbox = gpu::Mailbox();

  sync_token = std::move(other.sync_token);
  other.sync_token = gpu::SyncToken();

  color_space = std::move(other.color_space);
  other.color_space = gfx::ColorSpace();

  bitmap = std::move(other.bitmap);
  other.bitmap = SkBitmap();

  draw_data = std::move(other.draw_data);

  release_callback = std::move(other.release_callback);

  is_software = other.is_software;
  other.is_software = false;

  return *this;
}

SurfaceSavedFrame::FrameResult::FrameResult() = default;

SurfaceSavedFrame::FrameResult::FrameResult(FrameResult&& other) = default;

SurfaceSavedFrame::FrameResult::~FrameResult() = default;

SurfaceSavedFrame::FrameResult& SurfaceSavedFrame::FrameResult::operator=(
    FrameResult&& other) = default;

SurfaceSavedFrame::ScopedCleanSurface::ScopedCleanSurface(
    Surface* surface,
    CompositorFrame clean_frame)
    : surface_(surface) {
  surface_->SetInterpolatedFrame(std::move(clean_frame));
}

SurfaceSavedFrame::ScopedCleanSurface::~ScopedCleanSurface() {
  surface_->ResetInterpolatedFrame();
}

}  // namespace viz
