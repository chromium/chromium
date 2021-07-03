// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_saved_frame.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
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

SurfaceSavedFrame::RenderPassDrawData GetRootRenderPassDrawData(
    Surface* surface) {
  const auto& frame = surface->GetActiveFrame();
  DCHECK(!frame.render_pass_list.empty());
  const auto& root_render_pass = frame.render_pass_list.back();
  return {*root_render_pass, 1.f};
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

bool SurfaceSavedFrame::IsValid() const {
  bool result = valid_result_count_ == ExpectedResultCount();
  // If this saved frame is valid, then we should have a frame result.
  DCHECK(!result || frame_result_);
  return result;
}

void SurfaceSavedFrame::RequestCopyOfOutput(Surface* surface) {
  DCHECK(surface->HasActiveFrame());

  // RGBA_TEXTURE will become RGBA_BITMAP with SoftwareCompositing path.
  // TODO(kylechar): Add RGBA_NATIVE that returns either RGBA_TEXTURE or
  // RGBA_BITMAP depending on what is native.
  constexpr auto result_format = CopyOutputRequest::ResultFormat::RGBA_TEXTURE;

  const auto& root_draw_data = GetRootRenderPassDrawData(surface);
  // Bind kRoot and root geometry information to the callback.
  auto root_request = std::make_unique<CopyOutputRequest>(
      result_format,
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

  DCHECK(tainted_to_clean_pass_ids.contains(
      (active_frame.render_pass_list.back()->id)))
      << "The root pass must be tainted";

  TransitionUtils::FilterCallback filter_callback = base::BindRepeating(
      &SurfaceSavedFrame::FilterSharedElementAndTaintedQuads,
      base::Unretained(this), base::Unretained(&tainted_to_clean_pass_ids));

  CompositorFrame interpolated_frame;
  interpolated_frame.metadata = active_frame.metadata.Clone();
  interpolated_frame.resource_list = active_frame.resource_list;

  for (auto& render_pass : active_frame.render_pass_list) {
    const auto original_render_pass_id = render_pass->id;
    CompositorRenderPass* pass_for_clean_copy = nullptr;

    auto it = tainted_to_clean_pass_ids.find(original_render_pass_id);
    if (it != tainted_to_clean_pass_ids.end()) {
      auto clean_pass = TransitionUtils::CopyPassWithRenderPassFiltering(
          *render_pass, filter_callback);
      it->second = max_id = clean_pass->id =
          TransitionUtils::NextRenderPassId(max_id);
      pass_for_clean_copy = clean_pass.get();
      interpolated_frame.render_pass_list.push_back(std::move(clean_pass));
    }

    // Deep copy of the original pass propagating copy requests.
    auto copy_requests = std::move(render_pass->copy_requests);
    auto duplicate_pass = render_pass->DeepCopy();
    duplicate_pass->copy_requests = std::move(copy_requests);
    if (!pass_for_clean_copy)
      pass_for_clean_copy = duplicate_pass.get();
    interpolated_frame.render_pass_list.push_back(std::move(duplicate_pass));

    const auto& shared_pass_ids = directive_.shared_render_pass_ids();
    auto shared_pass_it =
        std::find(shared_pass_ids.begin(), shared_pass_ids.end(),
                  original_render_pass_id);
    if (shared_pass_it != shared_pass_ids.end()) {
      RenderPassDrawData draw_data(
          *render_pass,
          TransitionUtils::ComputeAccumulatedOpacity(
              active_frame.render_pass_list, original_render_pass_id));
      int index = std::distance(shared_pass_ids.begin(), shared_pass_it);
      auto request = std::make_unique<CopyOutputRequest>(
          result_format,
          base::BindOnce(&SurfaceSavedFrame::NotifyCopyOfOutputComplete,
                         weak_factory_.GetWeakPtr(), ResultType::kShared, index,
                         draw_data));
      request->set_result_task_runner(base::ThreadTaskRunnerHandle::Get());
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

  surface_.emplace(surface, std::move(interpolated_frame));

  DCHECK_EQ(copy_request_count_, ExpectedResultCount());
}

void SurfaceSavedFrame::ReleaseSurface() {
  surface_.reset();
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
    const CompositorRenderPassDrawQuad& pass_quad,
    CompositorRenderPass& copy_pass) const {
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
  const auto& shared_pass_ids = directive_.shared_render_pass_ids();
  return std::find(shared_pass_ids.begin(), shared_pass_ids.end(), pass_id) !=
         shared_pass_ids.end();
}

size_t SurfaceSavedFrame::ExpectedResultCount() const {
  // Start with 1 for the root render pass.
  size_t count = 1;
  for (auto& pass_id : directive_.shared_render_pass_ids())
    count += !pass_id.is_null();
  return count;
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
    surface_.reset();
  }

  // Return if the result is empty.
  // TODO(vmpstr): We should log / trace this.
  if (output_copy->IsEmpty())
    return;

  ++valid_result_count_;
  if (!frame_result_) {
    frame_result_.emplace();
    // Resize to the number of shared elements, even if some will be nullopts.
    frame_result_->shared_results.resize(
        directive_.shared_render_pass_ids().size());
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
  if (output_copy->format() == CopyOutputResult::Format::RGBA_BITMAP) {
    slot->bitmap = output_copy->ScopedAccessSkBitmap().GetOutScopedBitmap();
    slot->is_software = true;
  } else {
    auto output_copy_texture = *output_copy->GetTextureResult();
    slot->mailbox = output_copy_texture.mailbox;
    slot->sync_token = output_copy_texture.sync_token;
    slot->release_callback = output_copy->TakeTextureOwnership();
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

  frame_result_->shared_results.resize(
      directive_.shared_render_pass_ids().size());
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

SurfaceSavedFrame::ScopedInterpolatedSurface::ScopedInterpolatedSurface(
    Surface* surface,
    CompositorFrame frame)
    : surface_(surface) {
  surface_->SetInterpolatedFrame(std::move(frame));
}

SurfaceSavedFrame::ScopedInterpolatedSurface::~ScopedInterpolatedSurface() {
  surface_->ResetInterpolatedFrame();
}

}  // namespace viz
