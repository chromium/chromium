// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_
#define COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_

#include "base/component_export.h"
#include "base/token.h"

namespace tab_groups {

class COMPONENT_EXPORT(TAB_GROUPS) TabGroupId {
 public:
  static TabGroupId GenerateNew();

  // This should only called with |token| returned from a previous |token()|
  // call on a valid TabGroupId.
  static TabGroupId FromRawToken(base::Token token);

  // Should only be used if intending to populate the TabGroupId by reference,
  // using a valid existing ID. Primarily needed for the Tab Groups extensions
  // API.
  static TabGroupId CreateEmpty();

  TabGroupId(const TabGroupId& other);

  TabGroupId& operator=(const TabGroupId& other);

  bool operator==(const TabGroupId& other) const;
  bool operator!=(const TabGroupId& other) const;
  bool operator<(const TabGroupId& other) const;

  const base::Token& token() const { return token_; }

  bool is_empty() { return token_.is_zero(); }

  std::string ToString() const;

 private:
  explicit TabGroupId(base::Token token);

  base::Token token_;
};

// For use in std::unordered_map.
struct TabGroupIdHash {
 public:
  size_t operator()(const tab_groups::TabGroupId& group_id) const {
    return base::TokenHash()(group_id.token());
  }
};

}  // namespace tab_groups

#endif  // COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_
