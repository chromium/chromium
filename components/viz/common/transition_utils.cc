// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/transition_utils.h"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/render_pass_io.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "third_party/skia/include/core/SkColor.h"

namespace viz {

namespace {

constexpr int kMaxListToProcess = 32;
constexpr int kMaxQuadsPerFrame = 8;

struct StackFrame {
  StackFrame(int list_index,
             SharedQuadStateList::ConstIterator sqs_iter,
             QuadList::ConstIterator quad_iter,
             int indent)
      : list_index(list_index),
        sqs_iter(sqs_iter),
        quad_iter(quad_iter),
        indent(indent) {}

  int list_index;
  SharedQuadStateList::ConstIterator sqs_iter;
  QuadList::ConstIterator quad_iter;
  int indent;
  bool first_pass_visit = true;
};

std::string SkColorToRGBAString(SkColor color) {
  std::ostringstream str;
  str << "rgba(" << SkColorGetR(color) << ", " << SkColorGetG(color) << ", "
      << SkColorGetB(color) << ", " << SkColorGetA(color) << ")";
  return str.str();
}

std::string SkColorToRGBAString(SkColor4f color) {
  return SkColorToRGBAString(color.toSkColor());
}

std::unordered_set<uint64_t> ProcessStack(
    std::ostringstream& str,
    std::vector<StackFrame>& stack,
    const CompositorRenderPassList& list) {
  auto write_indent = [&str](int indent) {
    for (int i = 0; i < indent; ++i)
      str << " ";
  };
  auto write_render_pass = [&str](const CompositorRenderPass* pass) {
    str << "(" << pass << ") render pass id=" << pass->id.GetUnsafeValue()
        << " output_rect=" << pass->output_rect.ToString();
    if (pass->view_transition_element_resource_id.IsValid())
      str << " " << pass->view_transition_element_resource_id.ToString();
    str << "\n";
  };
  auto write_sqs = [&str](const SharedQuadState* sqs) {
    str << "(" << sqs << ") switched to sqs with opacity=" << sqs->opacity
        << ", blend_mode=" << BlendModeToString(sqs->blend_mode)
        << " quad_layer_rect=" << sqs->quad_layer_rect.ToString() << "\n";
  };
  auto write_shared_element_quad = [&str](const SharedElementDrawQuad* quad) {
    str << "(" << quad << ") SharedElementDrawQuad "
        << quad->resource_id.ToString() << "\n";
  };
  auto write_solid_color_quad = [&str](const SolidColorDrawQuad* quad) {
    str << "(" << quad
        << ") SolidColorDrawQuad color=" << SkColorToRGBAString(quad->color)
        << "\n";
  };
  auto write_compositor_render_pass_quad =
      [&str](const CompositorRenderPassDrawQuad* quad) {
        str << "(" << quad << ") CompositorRenderPassDrawQuad\n";
      };
  std::unordered_set<uint64_t> seen_render_pass_ids;
  int quads_per_frame_logged = 0;
  while (!stack.empty()) {
    auto& frame = stack.back();
    auto& pass = list[frame.list_index];

    if (frame.first_pass_visit) {
      frame.first_pass_visit = false;
      write_indent(frame.indent);
      write_render_pass(pass.get());
      seen_render_pass_ids.insert(pass->id.GetUnsafeValue());

      if (const auto* sqs = *frame.sqs_iter) {
        frame.indent += 2;
        write_indent(frame.indent);
        write_sqs(sqs);
      } else {
        stack.pop_back();
        continue;
      }
      if (!*frame.quad_iter) {
        stack.pop_back();
        continue;
      }
      frame.indent += 2;
    } else {
      if (++frame.quad_iter == pass->quad_list.end()) {
        quads_per_frame_logged = 0;
        stack.pop_back();
        continue;
      }
      if (++quads_per_frame_logged > kMaxQuadsPerFrame) {
        write_indent(frame.indent);
        str << "(more quads - orphaned list may not be correct)\n";
        quads_per_frame_logged = 0;
        stack.pop_back();
        continue;
      }
      frame.indent -= 2;
      while ((*frame.quad_iter)->shared_quad_state != *frame.sqs_iter) {
        ++frame.sqs_iter;
        DCHECK(frame.sqs_iter != pass->shared_quad_state_list.end());
        write_indent(frame.indent);
        write_sqs(*frame.sqs_iter);
      }
      frame.indent += 2;
    }

    write_indent(frame.indent);
    switch ((*frame.quad_iter)->material) {
      case DrawQuad::Material::kCompositorRenderPass: {
        auto* quad =
            CompositorRenderPassDrawQuad::MaterialCast((*frame.quad_iter));
        write_compositor_render_pass_quad(quad);

        bool found = false;
        for (auto i = frame.list_index - 1; i >= 0; --i) {
          if (list[i]->id == quad->render_pass_id) {
            found = true;
            stack.emplace_back(i, list[i]->shared_quad_state_list.begin(),
                               list[i]->quad_list.begin(), frame.indent + 2);
            break;
          }
        }
        CHECK(found) << "Couldn't find referenced render pass id";
        break;
      }
      case DrawQuad::Material::kSharedElement: {
        auto* quad = SharedElementDrawQuad::MaterialCast((*frame.quad_iter));
        write_shared_element_quad(quad);
        break;
      }
      case DrawQuad::Material::kSolidColor: {
        auto* quad = SolidColorDrawQuad::MaterialCast((*frame.quad_iter));
        write_solid_color_quad(quad);
        break;
      }
      default:
        str << "DrawQuad, material: "
            << DrawQuadMaterialToString((*frame.quad_iter)->material) << "\n";
        break;
    }
  }
  return seen_render_pass_ids;
}

}  // namespace

std::string TransitionUtils::RenderPassListToString(
    const CompositorRenderPassList& list) {
  std::ostringstream str;

  if (list.size() > kMaxListToProcess || list.empty()) {
    str << "RenderPassList too large or too small (" << list.size()
        << "), max supported list length " << kMaxListToProcess;
    return str.str();
  }

  std::vector<StackFrame> stack;
  stack.emplace_back(list.size() - 1,
                     list.back()->shared_quad_state_list.begin(),
                     list.back()->quad_list.begin(), 0);
  str << "render pass ids in order:\n";
  for (const auto& pass : list)
    str << " " << pass->id.GetUnsafeValue();
  str << "\n";

  str << "rooted render pass tree:\n";
  std::unordered_set<uint64_t> seen_render_pass_ids =
      ProcessStack(str, stack, list);
  if (list.size() != seen_render_pass_ids.size())
    str << "orphaned render pass tree(s):\n";
  while (true) {
    int i;
    for (i = list.size() - 1; i >= 0; --i) {
      if (seen_render_pass_ids.count(list[i]->id.GetUnsafeValue()) == 0)
        break;
    }
    if (i < 0)
      break;

    DCHECK(stack.empty());
    stack.emplace_back(i, list[i]->shared_quad_state_list.begin(),
                       list[i]->quad_list.begin(), 0);
    auto new_seen_pass_ids = ProcessStack(str, stack, list);
    seen_render_pass_ids.insert(new_seen_pass_ids.begin(),
                                new_seen_pass_ids.end());
  }
  return str.str();
}

// static
std::unique_ptr<CompositorRenderPass>
TransitionUtils::CopyPassWithQuadFiltering(
    const CompositorRenderPass& source_pass,
    FilterCallback filter_callback) {
  // This code is similar to CompositorRenderPass::DeepCopy, but does special
  // logic when copying compositor render pass draw quads.
  auto copy_pass = CompositorRenderPass::Create(
      source_pass.shared_quad_state_list.size(), source_pass.quad_list.size());

  copy_pass->SetAll(
      source_pass.id, source_pass.output_rect, source_pass.damage_rect,
      source_pass.transform_to_root_target, source_pass.filters,
      source_pass.backdrop_filters, source_pass.backdrop_filter_bounds,
      source_pass.subtree_capture_id, source_pass.subtree_size,
      source_pass.view_transition_element_resource_id,
      source_pass.has_transparent_background, source_pass.cache_render_pass,
      source_pass.has_damage_from_contributing_content,
      source_pass.generate_mipmap, source_pass.has_per_quad_damage);

  if (source_pass.shared_quad_state_list.empty())
    return copy_pass;

  SharedQuadStateList::ConstIterator sqs_iter =
      source_pass.shared_quad_state_list.begin();
  SharedQuadState* copy_shared_quad_state =
      copy_pass->CreateAndAppendSharedQuadState();
  *copy_shared_quad_state = **sqs_iter;

  for (auto* quad : source_pass.quad_list) {
    while (quad->shared_quad_state != *sqs_iter) {
      ++sqs_iter;
      DCHECK(sqs_iter != source_pass.shared_quad_state_list.end());
      copy_shared_quad_state = copy_pass->CreateAndAppendSharedQuadState();
      *copy_shared_quad_state = **sqs_iter;
    }
    DCHECK(quad->shared_quad_state == *sqs_iter);

    if (filter_callback.Run(*quad, *copy_pass.get()))
      continue;

    if (quad->material == DrawQuad::Material::kCompositorRenderPass) {
      const auto* pass_quad = CompositorRenderPassDrawQuad::MaterialCast(quad);
      copy_pass->CopyFromAndAppendRenderPassDrawQuad(pass_quad,
                                                     pass_quad->render_pass_id);
    } else {
      copy_pass->CopyFromAndAppendDrawQuad(quad);
    }
  }

  return copy_pass;
}

}  // namespace viz
