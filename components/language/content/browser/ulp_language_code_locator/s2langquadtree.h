// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_S2LANGQUADTREE_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_S2LANGQUADTREE_H_

#include <algorithm>
#include <bitset>
#include <map>
#include <string>

#include "base/bits.h"

class S2CellId;

// A serialized language tree. The bits given by GetBitAt represent a
// depth-first traversal of the tree with (1) internal nodes represented by a
// single bit 0, and (2) leaf nodes represented by a bit 1 followed by the
// binary form of the index of the language at that leaf into the tree's
// languages, accessible through GetLanguageAt. We assume those indexes have the
// smallest number of bits necessary. Indices are 1-based; index 0 represents
// absent language.
class SerializedLanguageTree {
 public:
  virtual ~SerializedLanguageTree() {}

  virtual std::string GetLanguageAt(const size_t pos) const = 0;
  virtual size_t GetNumLanguages() const = 0;
  virtual bool GetBitAt(const size_t pos) const = 0;

  int GetBitsPerLanguageIndex() const {
    return base::bits::Log2Ceiling(GetNumLanguages() + 1);
  }
};

// An implementation of SerializedLanguageTree that backs the tree's structure
// by a bitset.
template <size_t numbits>
class BitsetSerializedLanguageTree : public SerializedLanguageTree {
 public:
  BitsetSerializedLanguageTree(std::vector<std::string> languages,
                               std::bitset<numbits> bits)
      : languages_(languages), bits_(bits) {}
  ~BitsetSerializedLanguageTree() override {}

  // SerializedTree implementation
  std::string GetLanguageAt(const size_t pos) const override {
    return languages_[pos];
  }

  size_t GetNumLanguages() const override { return languages_.size(); }

  bool GetBitAt(const size_t pos) const override { return bits_[pos]; }

 private:
  std::vector<std::string> languages_;
  std::bitset<numbits> bits_;
};

// The node of a S2Cell-based quadtree holding string languages in its leaves.
class S2LangQuadTreeNode {
 public:
  S2LangQuadTreeNode();
  S2LangQuadTreeNode(const S2LangQuadTreeNode& other);
  ~S2LangQuadTreeNode();

  // Return language of the leaf containing the given |cell|.
  // Empty string if a null-leaf contains given |cell|.
  // |level_ptr| is set to the level (see S2CellId::level) of the leaf. (-1 if
  // |cell| matches an internal node).
  // |cell|'s face needs to match the face of data used to create the tree;
  // there is no check in place to verify that.
  std::string Get(const S2CellId& cell, int* level_ptr) const;
  std::string Get(const S2CellId& cell) const {
    int level;
    return Get(cell, &level);
  }

  // Reconstruct a S2LangQuadTree with structure and languages given by
  // |serialized_langtree|. The bitset size is templated to allow this method to
  // be re-used for the small number of different-sized serialized trees we
  // have.
  static S2LangQuadTreeNode Deserialize(
      const SerializedLanguageTree* serialized_langtree);

 private:
  static size_t DeserializeSubtree(
      const SerializedLanguageTree* serialized_langtree,
      int bits_per_lang_index,
      size_t bit_offset,
      S2LangQuadTreeNode* root);

  const S2LangQuadTreeNode& GetChild(const int child_index) const;

  // Return true iff the node is a leaf.
  bool IsLeaf() const;

  std::vector<S2LangQuadTreeNode> children_;
  std::string language_;
};

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_ULP_LANGUAGE_CODE_LOCATOR_S2LANGQUADTREE_H_
