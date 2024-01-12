// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_OCTREE_COLOR_QUANTIZER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_OCTREE_COLOR_QUANTIZER_H_

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/recording/gif_encoding_types.h"
#include "chromeos/ash/services/recording/rgb_video_frame.h"

namespace recording {

// GIF images can have a maximum number of 256 colors in their color tables.
// This means that the minimum number of bits needed to represent this count is
// 8, which is the max bit depth value.
inline constexpr size_t kMaxNumberOfColorsInPalette = 256;

// 8 bits for each color channel R, G, and B.
inline constexpr uint8_t kNumBitsPerColorChannel = 8;

// Defines an Octree-based (https://en.wikipedia.org/wiki/Octree) color
// quantizer that can efficiently extract the most important (up to
// `kMaxNumberOfColorsInPalette` (256)) colors from a given `rgb_video_frame`,
// and can be used to find the indices of the closest colors of the pixels in
// video frames based on the extracted palette.
class OctreeColorQuantizer {
 public:
  OctreeColorQuantizer();
  explicit OctreeColorQuantizer(const RgbVideoFrame& rgb_video_frame);
  OctreeColorQuantizer(OctreeColorQuantizer&&);
  OctreeColorQuantizer& operator=(OctreeColorQuantizer&&);
  ~OctreeColorQuantizer();

  // Fills in the given `out_color_palette` with the quantized (maximum of
  // `kMaxNumberOfColorsInPalette`) list of colors extracted from the
  // `rgb_video_frame` given to the constructor. It also assigns indices to
  // `Node::palette_index_` of all the leaf nodes in this tree.
  void ExtractColorPalette(ColorTable& out_color_palette);

  // Fills in the given `out_pixel_color_indices` with the indices of the
  // closest colors for each pixel in the given `rgb_video_frame` from the
  // quantized color palette extracted by calling `ExtractColorPalette()` above.
  // This means `ExtractColorPalette()` must be called once before calling this
  // for every received video frame (provided that the same `color_palette` is
  // still desired to be reused).
  // This also implements the Floyd-Steinberg dithering, meaning that the
  // resulting color indices in `out_pixel_color_indices` will be of a
  // quantized and dithered image of the given `rgb_video_frame` using the given
  // `color_palette`. The given `rgb_video_frame` will be modified in the
  // process of dithering to diffuse the color errors in each pixel over the
  // colors of neighboring pixels (See implementation for details).
  void ExtractPixelColorIndices(RgbVideoFrame& rgb_video_frame,
                                const ColorTable& color_palette,
                                ColorIndices& out_pixel_color_indices) const;

 private:
  // Defines a node in the Octree. Each node is a parent of up to 8 child nodes.
  // Depending on the colors in the given `rgb_video_frame`, not all nodes in
  // `child_nodes_` are available.
  class Node {
   public:
    Node();
    Node(Node&&);
    Node& operator=(Node&&);
    ~Node();

    bool is_leaf() const { return ref_count_ > 0; }

    // Gets the color that is represented by this node by averaging all the
    // accumulated `red_`, `green_` and `blue_` channels over the number of
    // times this node is referenced.
    RgbColor GetColor() const;

    // Returns the total sum of `ref_count_`s of all the immediate child nodes.
    // This is used for sorting all the nodes that belong to the same level when
    // we are reducing the colors to a maximum of `kMaxNumberOfColorsInPalette`.
    size_t ChildrenRefCount() const;

    // `red_`, `green_` and `blue_` Accumulate the values of the red, green and
    // blue color channels respectively every time this node (if it's a leaf
    // node) is referenced as we `InsertColor()`s into the Octree.
    uint32_t red_ = 0;
    uint32_t green_ = 0;
    uint32_t blue_ = 0;

    // The number of times this leaf node is referenced when we
    // `InsertColor()`s. Note that only leaf nodes have reference counts.
    size_t ref_count_ = 0;

    // A placeholder for the index of the color represented by this leaf node
    // when the color palette is built in `ExtractColorPalette()`.
    size_t palette_index_ = -1u;

    // Pointers to the next and previous nodes in a linked list tracking all the
    // leaf nodes. The linked list starts at `leaf_nodes_head_` below.
    // These pointers are safe to dangle as we never access them during the
    // destruction of the tree. The only time we explicitly remove a node in
    // `Reduce()` we call `RemoveLeafNode()` beforehand.
    raw_ptr<Node, DisableDanglingPtrDetection> next_ = nullptr;
    raw_ptr<Node, DisableDanglingPtrDetection> prev_ = nullptr;

    // The sub nodes of this node. A maximum of 8 child nodes can be populated.
    // A child node is added at index that is calculated from the RGB color
    // components of the color being added according to the level of `this` node
    // in the Octree. For example:
    // - At level 0: The indices are formed by combining the most significant
    //   bits (the 8th bits) of the RGB components to form a single number
    //   (0 to 7).
    // - At level 1: We form an index by combining the 7th bits of the RGB
    //   components together.
    // And so on. Please see `GetColorIndexAtLevel()`.
    std::array<std::unique_ptr<Node>, kNumBitsPerColorChannel> child_nodes_;
  };

  // Defines an array of lists of nodes at each level of the Octree, where the
  // levels are the indices of this array (0 to 7).
  using NodesPerLevel = std::array<std::vector<Node*>, kNumBitsPerColorChannel>;

  // Inserts the given `color` into this Octree starting at the `root_` node.
  // It uses the given `nodes_per_level` to track each newly created node by its
  // level (0 to 7) in the Octree. `nodes_per_level` can be used later by
  // `Reduce()` to start the leaf nodes reduction starting at the lower levels
  // and going up.
  void InsertColor(const RgbColor& color, NodesPerLevel& nodes_per_level);

  // Called recursively to insert the given `color` into the Octree. The given
  // `node` is the one being considered at the current iteration, and the one
  // whose children `child_nodes_` are at the given `level` of the Octree.
  // This means `root_::child_nodes_` are at level 0 of the Octree. Leaf
  // nodes are at level 7.
  // `nodes_per_level` will be populated as described above in `InsertColor()`.
  void InsertColorInternal(Node* node,
                           const RgbColor& color,
                           int level,
                           NodesPerLevel& nodes_per_level);

  // Reduces the number of leaf nodes (i.e. the number of colors) in this tree
  // to a maximum of `kMaxNumberOfColorsInPalette` (256) colors. This is done by
  // merging the color information of leaf nodes with their parent nodes (making
  // the parent nodes become leaf nodes), and discarding those old leaf nodes.
  // This is done in a way such that the most important colors (i.e. the ones
  // with the highest ref counts) are preferred to be kept in the tree. The
  // colors of the combined leaf nodes are averaged to produce a color that
  // represents all the discarded nodes.
  void Reduce(NodesPerLevel& nodes_per_level);

  // Returns the index of the given `color` into a color palette that has been
  // built by this Octree via `ExtractColorPalette()`. If `color` doesn't exist
  // in the Octree, it returns the index of its closest color that is in the
  // palette.
  size_t FindColorIndex(const RgbColor& color) const;

  // Called recursively by `FindColorIndex()` to get the index of the given
  // `color`. `node` is the one currently being considered which exists at the
  // given `level`. The index is found when we reach a leaf node by traversing
  // the tree.
  size_t FindColorIndexInternal(const Node* node,
                                int level,
                                const RgbColor& color) const;

  // Inserts the given leaf `node` at the front of the linked list that tracks
  // all leaf nodes. After this is called, `node` will become the new
  // `leaf_nodes_head_`.
  void InsertLeafNode(Node* node);

  // Removes the given leaf `node` from the linked list that tracks all the leaf
  // nodes in this tree.
  void RemoveLeafNode(Node* node);

  // The root node of this Octree. The immediate 8 child nodes of the root are
  // at level 0, which is the very first level of the tree.
  Node root_;

  // The head node of a linked list that tracks all the leaf nodes in this tree.
  // Note that leaf nodes are the ones that contain the color information.
  // This list will be used to build a color palette after it has been reduced
  // to `kMaxNumberOfColorsInPalette` in `Reduce()`.
  // This is also safe to dangle as we never access it during the destruction of
  // the tree.
  raw_ptr<Node, DisableDanglingPtrDetection> leaf_nodes_head_;

  // The total number of leaf nodes (i.e. the total number of colors in the this
  // tree).
  size_t leaf_nodes_count_ = 0;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_OCTREE_COLOR_QUANTIZER_H_
