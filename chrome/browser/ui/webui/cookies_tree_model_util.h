// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/strings/string16.h"

class CookieTreeNode;

namespace base {
class DictionaryValue;
class ListValue;
}

class CookiesTreeModelUtil {
 public:
  CookiesTreeModelUtil();
  ~CookiesTreeModelUtil();

  // Finds or creates an ID for given |node| and returns it as string.
  std::string GetTreeNodeId(const CookieTreeNode* node);

  // Append the details of the child nodes of |parent|.
  void GetChildNodeDetails(const CookieTreeNode* parent,
                           bool include_quota_nodes,
                           base::ListValue* list);

  // Append the children nodes of |parent| in specified range to |nodes| list.
  void GetChildNodeList(const CookieTreeNode* parent,
                        size_t start,
                        size_t count,
                        bool include_quota_nodes,
                        base::ListValue* nodes);

  // Gets tree node from |path| under |root|. |path| is comma separated list of
  // ids. |id_map| translates ids into object pointers. Return NULL if |path|
  // is not valid.
  const CookieTreeNode* GetTreeNodeFromPath(const CookieTreeNode* root,
                                            const std::string& path);

  // Gets tree node from |title| under |root|. |title| is a node title. Return
  // NULL if |title| is not found.
  const CookieTreeNode* GetTreeNodeFromTitle(const CookieTreeNode* root,
                                             const base::string16& title);

 private:
  using CookiesTreeNodeIdMap = base::IDMap<const CookieTreeNode*>;
  using CookieTreeNodeMap = std::map<const CookieTreeNode*, int32_t>;

  // Populate given |dict| with cookie tree node properties. |id_map| maps
  // a CookieTreeNode to an ID and creates a new ID if |node| is not in the
  // maps. Returns false if the |node| does not need to be shown.
  bool GetCookieTreeNodeDictionary(const CookieTreeNode& node,
                                   bool include_quota_nodes,
                                   base::DictionaryValue* dict);

  // IDMap to create unique ID and look up the object for an ID.
  CookiesTreeNodeIdMap id_map_;

  // Reverse look up map to find the ID for a node.
  CookieTreeNodeMap node_map_;

  DISALLOW_COPY_AND_ASSIGN(CookiesTreeModelUtil);
};

#endif  // CHROME_BROWSER_UI_WEBUI_COOKIES_TREE_MODEL_UTIL_H_
