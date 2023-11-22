// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/octree_color_quantizer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "chromeos/ash/services/recording/gif_encoding_types.h"
#include "chromeos/ash/services/recording/rgb_video_frame.h"

namespace recording {

namespace {

// The masks used to extract the bits the correspond to each level, where level
// is the index into this array.
// Level 0 corresponds to the most significant bit in an R, G, or B color
// component, whereas level 7 corresponds to the least significant bit.
constexpr uint8_t kLevelMasks[kNumBitsPerColorChannel] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

// Forms and returns an index by combining 3 bits, one from each color component
// R, G, and B respectively. The position of these bits are determined by the
// given `level`. See comment above.
ColorIndex GetColorIndexAtLevel(const RgbColor& color, int level) {
  ColorIndex index = 0;
  const auto mask = kLevelMasks[level];

  if (color.r & mask) {
    //         RGB.
    index |= 0b100;
  }
  if (color.g & mask) {
    //         RGB.
    index |= 0b010;
  }
  if (color.b & mask) {
    //         RGB.
    index |= 0b001;
  }

  return index;
}

// Invokes the given functor `f` on each pixel's `RgbColor` of the given
// `rgb_video_frame`.
template <class Functor>
void ForEachPixelColor(RgbVideoFrame& rgb_video_frame, Functor f) {
  auto* pixel = &rgb_video_frame.pixel_color(0, 0);
  const auto* const end = &pixel[rgb_video_frame.num_pixels()];
  for (; pixel < end; ++pixel) {
    f(*pixel);
  }
}

template <class Functor>
void ForEachPixelColor(const RgbVideoFrame& rgb_video_frame, Functor f) {
  ForEachPixelColor(const_cast<RgbVideoFrame&>(rgb_video_frame), f);
}

}  // namespace

// -----------------------------------------------------------------------------
// OctreeColorQuantizer:

OctreeColorQuantizer::OctreeColorQuantizer() = default;

OctreeColorQuantizer::OctreeColorQuantizer(
    const RgbVideoFrame& rgb_video_frame) {
  // Insert all the colors in the given `rgb_video_frame` into this tree. If a
  // color is referenced by multiple pixels, the same corresponding leaf node
  // will be reached, and its `red_`, `green_`, `blue_` and `ref_count_` will be
  // incremented.
  NodesPerLevel nodes_per_level;
  ForEachPixelColor(rgb_video_frame, [&](const RgbColor& color) {
    InsertColor(color, nodes_per_level);
  });

  // Reduce the leaf nodes (i.e. number of unique colors) to a maximum of
  // `kMaxNumberOfColorsInPalette` (256) colors.
  Reduce(nodes_per_level);
}

OctreeColorQuantizer::OctreeColorQuantizer(OctreeColorQuantizer&&) = default;

OctreeColorQuantizer& OctreeColorQuantizer::operator=(OctreeColorQuantizer&&) =
    default;

OctreeColorQuantizer::~OctreeColorQuantizer() = default;

void OctreeColorQuantizer::ExtractColorPalette(ColorTable& out_color_palette) {
  out_color_palette.clear();

  size_t color_palette_index = 0;
  Node* curr = leaf_nodes_head_;
  while (curr != nullptr) {
    out_color_palette.push_back(curr->GetColor());
    curr->palette_index_ = color_palette_index;
    curr = curr->next_;
    ++color_palette_index;
  }
}

void OctreeColorQuantizer::ExtractPixelColorIndices(
    const RgbVideoFrame& rgb_video_frame,
    ColorIndices& out_pixel_color_indices) const {
  size_t pixel_index = 0;
  ForEachPixelColor(rgb_video_frame, [&](const RgbColor& color) {
    const auto color_index = FindColorIndex(color);
    out_pixel_color_indices[pixel_index] = color_index;
    ++pixel_index;
  });
}

// -----------------------------------------------------------------------------
// OctreeColorQuantizer::Node:

OctreeColorQuantizer::Node::Node() = default;

OctreeColorQuantizer::Node::Node(Node&&) = default;

OctreeColorQuantizer::Node& OctreeColorQuantizer::Node::operator=(Node&&) =
    default;

OctreeColorQuantizer::Node::~Node() = default;

RgbColor OctreeColorQuantizer::Node::GetColor() const {
  DCHECK_GT(ref_count_, 0u);

  return RgbColor(red_ / ref_count_, green_ / ref_count_, blue_ / ref_count_);
}

size_t OctreeColorQuantizer::Node::ChildrenRefCount() const {
  size_t count = 0;
  for (const auto& child : child_nodes_) {
    if (child) {
      count += child->ref_count_;
    }
  }
  return count;
}

// -----------------------------------------------------------------------------
// OctreeColorQuantizer:

void OctreeColorQuantizer::InsertColor(const RgbColor& color,
                                       NodesPerLevel& nodes_per_level) {
  InsertColorInternal(&root_, color, /*level=*/0, nodes_per_level);
}

void OctreeColorQuantizer::InsertColorInternal(Node* node,
                                               const RgbColor& color,
                                               int level,
                                               NodesPerLevel& nodes_per_level) {
  if (level >= kNumBitsPerColorChannel) {
    // This is a leaf node. Accumulate all the color components and increment
    // the color count.
    node->red_ += color.r;
    node->green_ += color.g;
    node->blue_ += color.b;

    // If this is the very first time this leaf node is referenced, add it to
    // the leaf nodes linked list.
    if (++(node->ref_count_) == 1u) {
      InsertLeafNode(node);
    }

    return;
  }

  const auto index = GetColorIndexAtLevel(color, level);
  DCHECK_LE(index, 7u);

  auto& child_node = node->child_nodes_[index];

  if (!child_node) {
    // This is the first time a node at this index is accessed. Create a node
    // and track it as one of the nodes at the current `level`.
    child_node = std::make_unique<Node>();
    nodes_per_level[level].push_back(child_node.get());
  }

  InsertColorInternal(child_node.get(), color, level + 1, nodes_per_level);
}

void OctreeColorQuantizer::Reduce(NodesPerLevel& nodes_per_level) {
  // The nodes at the last level (level 7 = `kNumBitsPerColorChannel - 1`) are
  // all leaf nodes (i.e. they have no children). There's not point in starting
  // the reduction there. So we start from the level above (level 6).
  for (int level = kNumBitsPerColorChannel - 2; level >= 0; --level) {
    auto& cur_level_nodes = nodes_per_level[level];

    // Sort the nodes in this level, such that the ones with the least
    // referenced child nodes come first, so that they can be reduced first.
    // This keeps the important colors (the ones that were referenced the most
    // by many pixels in the video frame) are less likely to be merged with
    // other nodes.
    std::sort(cur_level_nodes.begin(), cur_level_nodes.end(),
              [](const Node* const a, const Node* const b) {
                return a->ChildrenRefCount() < b->ChildrenRefCount();
              });

    for (Node* node : cur_level_nodes) {
      for (auto& child_node : node->child_nodes_) {
        if (!child_node) {
          continue;
        }

        // Whether this `node` was a leaf before we merge a leaf child node with
        // it.
        const bool node_was_leaf = node->is_leaf();

        if (child_node->is_leaf()) {
          node->red_ += child_node->red_;
          node->green_ += child_node->green_;
          node->blue_ += child_node->blue_;
          node->ref_count_ += child_node->ref_count_;

          // `child_node` was merged into `node`. We can now get rid of it.
          RemoveLeafNode(child_node.get());
          child_node.reset();

          // If this is the first time `node` becomes a leaf node, we need to
          // insert it into the linked list.
          if (!node_was_leaf) {
            InsertLeafNode(node);
          }
        }
      }

      // After reducing the children of the current `node`, let's see if we are
      // at or below tha maximum number of colors.
      if (leaf_nodes_count_ <= kMaxNumberOfColorsInPalette) {
        return;
      }
    }
  }
}

size_t OctreeColorQuantizer::FindColorIndex(const RgbColor& color) const {
  return FindColorIndexInternal(&root_, /*level=*/0, color);
}

size_t OctreeColorQuantizer::FindColorIndexInternal(
    const Node* node,
    int level,
    const RgbColor& color) const {
  if (node->is_leaf()) {
    return node->palette_index_;
  }

  const auto index = GetColorIndexAtLevel(color, level);

  // Search forward starting at `index` then search backward.
  for (uint8_t i = index; i < kNumBitsPerColorChannel; ++i) {
    if (const auto& child = node->child_nodes_[i]) {
      return FindColorIndexInternal(child.get(), level + 1, color);
    }
  }

  for (uint8_t i = index; i > 0; --i) {
    if (const auto& child = node->child_nodes_[i - 1]) {
      return FindColorIndexInternal(child.get(), level + 1, color);
    }
  }

  NOTREACHED_NORETURN();
}

void OctreeColorQuantizer::InsertLeafNode(Node* node) {
  ++leaf_nodes_count_;

  // Always insert the node at the front. The order of leaf nodes doesn't
  // matter.
  if (leaf_nodes_head_) {
    leaf_nodes_head_->prev_ = node;
  }
  node->next_ = leaf_nodes_head_;
  node->prev_ = nullptr;
  leaf_nodes_head_ = node;
}

void OctreeColorQuantizer::RemoveLeafNode(Node* node) {
  --leaf_nodes_count_;

  if (node->prev_) {
    node->prev_->next_ = node->next_;
  }
  if (node->next_) {
    node->next_->prev_ = node->prev_;
  }

  if (node == leaf_nodes_head_) {
    leaf_nodes_head_ = node->next_;
  }

  node->prev_ = nullptr;
  node->next_ = nullptr;
}

}  // namespace recording
