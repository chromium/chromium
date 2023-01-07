// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/containers/id_map.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class CookieTreeNode;

class CookiesTreeModelUtil {
 public:
  CookiesTreeModelUtil();

  CookiesTreeModelUtil(const CookiesTreeModelUtil&) = delete;
  CookiesTreeModelUtil& operator=(const CookiesTreeModelUtil&) = delete;

  ~CookiesTreeModelUtil();

  // Finds or creates an ID for given |node| and returns it as string.
  std::string GetTreeNodeId(const CookieTreeNode* node);

  // Return the details of the child nodes of `parent`.
  // DEPRECATED(crbug.com/1271155): The cookies tree model is slowly being
  // deprecated, during this process the semantics of the model are nuanced
  // w.r.t partitioned storage, and should not be used in new locations.
  base::Value::List GetChildNodeDetailsDeprecated(const CookieTreeNode* parent);

  // Gets tree node from |path| under |root|. |path| is comma separated list of
  // ids. |id_map| translates ids into object pointers. Return NULL if |path|
  // is not valid.
  const CookieTreeNode* GetTreeNodeFromPath(const CookieTreeNode* root,
                                            const std::string& path);

  // Gets tree node from |title| under |root|. |title| is a node title. Return
  // NULL if |title| is not found.
  const CookieTreeNode* GetTreeNodeFromTitle(const CookieTreeNode* root,
                                             const std::u16string& title);

 private:
  using CookiesTreeNodeIdMap = base::IDMap<const CookieTreeNode*>;
  using CookieTreeNodeMap = std::map<const CookieTreeNode*, int32_t>;

  // Returns a Value::Dict populated with cookie tree node properties. `id_map`
  // maps a CookieTreeNode to an ID and creates a new ID if `node` is not in the
  // maps. Returns nullopt if the `node` does not need to be shown.
  absl::optional<base::Value::Dict> GetCookieTreeNodeDictionary(
      const CookieTreeNode& node);

  // IDMap to create unique ID and look up the object for an ID.
  CookiesTreeNodeIdMap id_map_;

  // Reverse look up map to find the ID for a node.
  CookieTreeNodeMap node_map_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
