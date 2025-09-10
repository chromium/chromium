// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/masked_domain_list.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "components/ip_protection/common/flat/masked_domain_list_generated.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace ip_protection {

namespace {

using ::masked_domain_list::Resource;
using ::masked_domain_list::ResourceOwner;

// Given a domain string, split it on `.` and reverse the result.
std::vector<std::string_view> ReversedAtoms(const std::string& domain) {
  std::vector<std::string_view> atoms = base::SplitStringPiece(
      domain, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::ranges::reverse(atoms);
  return atoms;
}

// Return a unique integer to represent `str`, based on `strings`. The resulting
// values of `strings` will be consecutive integers starting with one.
size_t MemoizeString(const std::string_view str_view,
                     std::map<std::string, size_t>& strings) {
  auto str = std::string(str_view);
  auto existing = strings.lower_bound(str);
  if (existing != strings.end() && existing->first == str_view) {
    return existing->second;
  }

  // Add a new string.
  size_t index = strings.size() + 1;
  strings[std::move(str)] = index;
  return index;
}

// Add a node to the builder, recursively adding all children first and
// returning the new node.
flatbuffers::Offset<flat::TreeNode> AddNode(
    size_t tree_node_idx,
    flatbuffers::FlatBufferBuilder& builder,
    const std::vector<MaskedDomainList::TreeNode>& tree_nodes,
    const std::vector<flatbuffers::Offset<flatbuffers::String>>&
        string_offsets) {
  const MaskedDomainList::TreeNode& tree_node = tree_nodes[tree_node_idx];

  std::vector<flatbuffers::Offset<flat::ChildNode>> children;
  for (const auto& [str_idx, child_idx] : tree_node.children) {
    flatbuffers::Offset<flat::TreeNode> child_node =
        AddNode(child_idx, builder, tree_nodes, string_offsets);
    auto child_builder = flat::ChildNodeBuilder(builder);
    child_builder.add_atom(string_offsets[str_idx]);
    child_builder.add_tree_node(child_node);
    children.push_back(child_builder.Finish());
  }

  // The flatbuffers API requires that CreateVectorOfSortedTables be completed
  // before beginning the builder for the TreeNode.
  flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flat::ChildNode>>>
      sorted_children;
  if (children.size()) {
    sorted_children =
        builder.CreateVectorOfSortedTables<flat::ChildNode>(&children);
  }

  // Build the TreeNode.
  auto tree_node_builder = flat::TreeNodeBuilder(builder);
  if (children.size()) {
    tree_node_builder.add_children(sorted_children);
  }
  if (tree_node.is_wildcard) {
    tree_node_builder.add_is_wildcard(true);
  }
  if (tree_node.is_resource) {
    tree_node_builder.add_is_resource(true);
  }
  if (tree_node.owner_id) {
    tree_node_builder.add_owner_id(tree_node.owner_id);
  }
  return tree_node_builder.Finish();
}

}  // namespace

MaskedDomainList::TreeNode::TreeNode() = default;
MaskedDomainList::TreeNode::~TreeNode() = default;
MaskedDomainList::TreeNode::TreeNode(TreeNode&) = default;
MaskedDomainList::TreeNode::TreeNode(TreeNode&&) = default;

MaskedDomainList::Builder::Builder() {
  // Create the root tree node.
  tree_nodes_.emplace_back();
}

MaskedDomainList::Builder::~Builder() = default;

bool MaskedDomainList::Builder::AddOwner(const std::string& domain,
                                         uint32_t owner_id,
                                         bool is_resource,
                                         bool is_wildcard) {
  DCHECK_NE(owner_id, 0u);

  // Memoize each of the atoms, resulting in indices into strings_.
  std::vector<size_t> atoms;
  for (const auto& atom_str : ReversedAtoms(domain)) {
    atoms.push_back(MemoizeString(atom_str, strings_));
  }

  size_t tree_node_idx = 0;
  for (size_t atom : atoms) {
    TreeNode& tree_node = tree_nodes_[tree_node_idx];
    auto child = tree_node.children.find(atom);
    if (child != tree_node.children.end()) {
      tree_node_idx = child->second;
      continue;
    }

    // Add a new node and link it to the parent. Note that the `emplace_back`
    // call may invalidate the reference `tree_node`, so it must be used first.
    tree_node_idx = tree_nodes_.size();
    tree_node.children[atom] = tree_node_idx;
    tree_nodes_.emplace_back();
  }

  TreeNode& tree_node = tree_nodes_[tree_node_idx];

  if (tree_node.owner_id != 0) {
    // Node already existed.
    return false;
  }
  tree_node.is_wildcard = is_wildcard;
  tree_node.owner_id = owner_id;
  tree_node.is_resource = is_resource;

  return true;
}

bool MaskedDomainList::Builder::Finish(base::FilePath file_name) {
  flatbuffers::FlatBufferBuilder builder;

  // Begin by writing out all strings, keeping track of their Offsets.
  std::vector<flatbuffers::Offset<flatbuffers::String>> strings(
      strings_.size() + 1);
  for (const auto& [str, idx] : strings_) {
    strings[idx] = builder.CreateString(str);
  }
  strings_.clear();

  // Recursively add the tree nodes, depth-first.
  flatbuffers::Offset<flat::TreeNode> root_node =
      AddNode(0, builder, tree_nodes_, strings);
  tree_nodes_.clear();

  // Write out the flatbuffer data.
  auto mdl = flat::CreateMaskedDomainList(builder, root_node);
  builder.Finish(mdl);
  return base::WriteFile(file_name, builder.GetBufferSpan());
}

bool MaskedDomainList::GetResult::operator==(const GetResult& other) const {
  return owner_id == other.owner_id && is_resource == other.is_resource;
}

MaskedDomainList::MaskedDomainList(base::File file, uint64_t file_size)
    : MaskedDomainList(std::move(file),
                       base::MemoryMappedFile::Region{
                           .offset = 0,
                           .size = base::checked_cast<size_t>(file_size)}) {}

MaskedDomainList::MaskedDomainList(base::File file,
                                   base::MemoryMappedFile::Region region) {
  if (!mdl_file_.Initialize(std::move(file), region)) {
    CHECK(!mdl_file_.IsValid());
  } else {
    mdl_ = flat::GetMaskedDomainList(mdl_file_.data());
  }
}

bool MaskedDomainList::Verify() {
  return mdl_file_.IsValid() && mdl_;
}

MaskedDomainList::GetResult MaskedDomainList::Get(
    const std::string& domain) const {
  CHECK(mdl_);

  const flat::TreeNode* tree_node = mdl_->tree_node();
  const flat::TreeNode* last_wildcard_node = nullptr;

  std::vector<std::string_view> atoms = ReversedAtoms(domain);
  for (const auto& atom : atoms) {
    if (tree_node->is_wildcard()) {
      last_wildcard_node = tree_node;
    }
    const flatbuffers::Vector<
        flatbuffers::Offset<ip_protection::flat::ChildNode>>* children =
        tree_node->children();
    if (!children) {
      tree_node = last_wildcard_node;
      break;
    }
    auto* child = children->LookupByKey(atom);
    if (!child) {
      tree_node = last_wildcard_node;
      break;
    }
    tree_node = child->tree_node();
    CHECK(tree_node);
  }

  if (tree_node) {
    return GetResult{
        .owner_id = tree_node->owner_id(),
        .is_resource = tree_node->is_resource(),
    };
  } else {
    return GetResult{.owner_id = 0, .is_resource = false};
  }
}

bool MaskedDomainList::IsOwnedResource(
    const std::string& request_domain) const {
  GetResult request_info = Get(request_domain);
  return request_info.owner_id != 0 && request_info.is_resource;
}

bool MaskedDomainList::Matches(const std::string& request_domain,
                               const std::string& top_frame_domain) const {
  GetResult request_info = Get(request_domain);
  if (request_info.owner_id == 0 || !request_info.is_resource) {
    return false;
  }

  GetResult top_frame_info = Get(top_frame_domain);
  if (top_frame_info.owner_id == request_info.owner_id) {
    return false;
  }

  return true;
}

// static
bool MaskedDomainList::BuildFromProto(
    const masked_domain_list::MaskedDomainList& mdl,
    base::FilePath default_file_name,
    base::FilePath regular_browsing_file_name) {
  MaskedDomainList::Builder default_builder;
  MaskedDomainList::Builder regular_browsing_builder;

  // Insert the relevant owners into the builders for the two MDLs. If a
  // domain appears multiple times in the input data, the first appearance
  // is used.
  base::CheckedNumeric<uint32_t> owner_id = 1;
  for (const ResourceOwner& owner : mdl.resource_owners()) {
    for (const auto& resource : owner.owned_resources()) {
      for (MdlType mdl_type : FromMdlResourceProto(resource)) {
        auto& builder = mdl_type == MdlType::kIncognito
                            ? default_builder
                            : regular_browsing_builder;
        builder.AddOwner(resource.domain(), owner_id.ValueOrDie(),
                         /*is_resource=*/true,
                         /*is_wildcard=*/true);
      }
    }
    for (const auto& property : owner.owned_properties()) {
      default_builder.AddOwner(property, owner_id.ValueOrDie(),
                               /*is_resource=*/false,
                               /*is_wildcard=*/true);
      regular_browsing_builder.AddOwner(property, owner_id.ValueOrDie(),
                                        /*is_resource=*/false,
                                        /*is_wildcard=*/true);
    }
    owner_id++;
  }

  if (!default_builder.Finish(default_file_name)) {
    DLOG(ERROR) << "Could not write default MDL file";
    return false;
  }

  if (!regular_browsing_builder.Finish(regular_browsing_file_name)) {
    DLOG(ERROR) << "Could not write regular browsing MDL file";
    return false;
  }

  return true;
}

}  // namespace ip_protection
