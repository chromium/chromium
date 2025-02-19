// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_H_
#define COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"

namespace ip_protection {

namespace flat {
struct MaskedDomainList;
}  // namespace flat

// A class representing a populated MDL, based on a memory-mapped file.
//
// This serves to map domains to an owner ID and an is_resource flag, allowing
// for wildcards. Note that domains are assumed to end in the TLD, not a dot
// (e.g., `example.com` and not `example.com.`).
class MaskedDomainList {
 public:
  // A node in the MDL tree. This is only used during building; reading the
  // tree is done entirely with flatbuffer types.
  struct TreeNode {
    TreeNode();
    TreeNode(TreeNode&);
    TreeNode(TreeNode&&);
    ~TreeNode();

    bool is_wildcard = false;
    uint32_t owner_id = 0;
    bool is_resource = false;
    // Map of children, keyed by the index of the atom in `Builder::strings_`,
    // and indexing nodes in `Builder::tree_nodes_`.
    std::map<size_t, size_t> children;
  };

  // A builder for the MDL data structure. This stores the structure in memory
  // during building, but then serializes it to a file.
  //
  // The Builder guarantees to create a file that `MaskedDomainList(path)` can
  // read, but this compatibility is not guaranteed between Chromium versions.
  // Files should be written and read by the same build.
  class Builder {
   public:
    Builder();
    Builder(Builder&) = delete;
    ~Builder();

    // Add an owner to the MDL.
    //
    // If `is_wildcard` is true, the MDL will match any domain with this suffix,
    // which does not have a more specific owner.
    //
    // Returns false if the domain is already in the MDL.
    bool AddOwner(const std::string& domain,
                  uint32_t owner_id,
                  bool is_resource,
                  bool is_wildcard);

    // Finish, writing the completed flatbuffer data to the given file. The
    // builder cannot be used after this call. Returns false on failure.
    bool Finish(base::FilePath file_name);

   private:
    // Map of strings to indices, used to de-duplicate strings and put
    // them in one part of the flatbuffer.
    std::map<std::string, size_t> strings_;

    // Tree nodes, with index zero being the root.
    std::vector<TreeNode> tree_nodes_;
  };

  // The result of a call to `Get`.
  struct GetResult {
    // The owner ID found in the list, or 0 if none was found.
    uint32_t owner_id;
    // Whether the domain is an owned resource (false for a property).
    bool is_resource;

    bool operator==(const GetResult& other) const;
  };

  // Create a new MDL reading from the given file, with a file size.
  MaskedDomainList(base::File file, uint64_t file_size);
  MaskedDomainList(base::File file, base::MemoryMappedFile::Region region);

  // Verify that the MDL was read correctly. This will be false if the file
  // could not be read. This does not validate the contents of the file, as
  // that would be a time-consuming process.
  bool Verify();

  // Determine whether the given request matches the MDL.
  //
  // A match occurs when the request domain exists in the MDL and is a resource.
  // However, if the top-level domain is also in the MDL, with the same owner
  // (whether it is a resource or not), then the request is not considered a
  // match.
  bool Matches(const std::string& request_domain,
               const std::string& top_frame_domain) const;

  // Determine if the given domain has an owner and is a resource.
  bool IsOwnedResource(const std::string& request_domain) const;

  // Build a representation of the given proto data and write it to the given
  // filenames. Returns false on failure.
  static bool BuildFromProto(const masked_domain_list::MaskedDomainList& mdl,
                             base::FilePath default_file_name,
                             base::FilePath regular_browsing_file_name);

 private:
  FRIEND_TEST_ALL_PREFIXES(MaskedDomainListTest, AddOwnerCollision);
  FRIEND_TEST_ALL_PREFIXES(MaskedDomainListTest, GetSubdomainHandling);

  // Get the value for the given domain. If the domain is not in
  // the MDL, returns a result with an `owner_id` of 0 and `is_resource` false.
  GetResult Get(const std::string& domain) const;

  // This pointer points into the memory map for mdl_file_.
  raw_ptr<const flat::MaskedDomainList> mdl_;
  base::MemoryMappedFile mdl_file_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_MASKED_DOMAIN_LIST_H_
