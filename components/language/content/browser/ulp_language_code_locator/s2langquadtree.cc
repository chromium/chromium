// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"

#include <string>

#include "third_party/s2cellid/src/s2/s2cellid.h"

S2LangQuadTreeNode::S2LangQuadTreeNode() = default;
S2LangQuadTreeNode::S2LangQuadTreeNode(const S2LangQuadTreeNode& other) =
    default;
S2LangQuadTreeNode::~S2LangQuadTreeNode() = default;

std::string S2LangQuadTreeNode::Get(const S2CellId& cell,
                                    int* level_ptr) const {
  const S2LangQuadTreeNode* node = this;
  for (int current_level = 0; current_level <= cell.level(); current_level++) {
    if (node->IsLeaf()) {
      *level_ptr = current_level;
      return node->language_;
    }
    if (current_level < cell.level())
      node = &(node->GetChild(cell.child_position(current_level + 1)));
  }
  *level_ptr = -1;
  return "";
}

S2LangQuadTreeNode S2LangQuadTreeNode::Deserialize(
    const SerializedLanguageTree* serialized_langtree) {
  S2LangQuadTreeNode root;
  int bits_per_lang_index = serialized_langtree->GetBitsPerLanguageIndex();
  DeserializeSubtree(serialized_langtree, bits_per_lang_index, 0, &root);
  return root;
}

size_t S2LangQuadTreeNode::DeserializeSubtree(
    const SerializedLanguageTree* serialized_langtree,
    int bits_per_lang_index,
    size_t bit_offset,
    S2LangQuadTreeNode* root) {
  if (serialized_langtree->GetBitAt(bit_offset)) {
    int index = 0;
    for (int bit_index = 1; bit_index <= bits_per_lang_index; bit_index++) {
      index <<= 1;
      index += serialized_langtree->GetBitAt(bit_offset + bit_index);
    }
    if (index != 0)
      root->language_ = serialized_langtree->GetLanguageAt(index - 1);
    return bits_per_lang_index + 1;
  } else {
    size_t subtree_size = 1;
    root->children_.reserve(4);
    for (int child_index = 0; child_index < 4; child_index++) {
      S2LangQuadTreeNode child;
      subtree_size +=
          DeserializeSubtree(serialized_langtree, bits_per_lang_index,
                             bit_offset + subtree_size, &child);
      root->children_.push_back(child);
    }
    return subtree_size;
  }
}

const S2LangQuadTreeNode& S2LangQuadTreeNode::GetChild(
    const int child_index) const {
  DCHECK(!children_.empty());
  return children_[child_index];
}

bool S2LangQuadTreeNode::IsLeaf() const {
  return children_.empty();
}
