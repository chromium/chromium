// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmarks_policy_handler.h"

#include <utility>

#include "base/values.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmarks_tracker.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

using bookmarks::ManagedBookmarksTracker;

namespace bookmarks {

ManagedBookmarksPolicyHandler::ManagedBookmarksPolicyHandler(
    policy::Schema chrome_schema)
    : SchemaValidatingPolicyHandler(
          policy::key::kManagedBookmarks,
          chrome_schema.GetKnownProperty(policy::key::kManagedBookmarks),
          policy::SCHEMA_ALLOW_UNKNOWN) {}

ManagedBookmarksPolicyHandler::~ManagedBookmarksPolicyHandler() = default;

void ManagedBookmarksPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  std::unique_ptr<base::Value> value;
  if (!CheckAndGetValue(policies, nullptr, &value))
    return;

  base::ListValue* list = nullptr;
  if (!value || !value->GetAsList(&list))
    return;

  prefs->SetString(prefs::kManagedBookmarksFolderName, GetFolderName(*list));
  FilterBookmarks(list);
  prefs->SetValue(prefs::kManagedBookmarks,
                  base::Value::FromUniquePtrValue(std::move(value)));
}

std::string ManagedBookmarksPolicyHandler::GetFolderName(
    const base::ListValue& list) {
  // Iterate over the list, and try to find the FolderName.
  for (const auto& el : list.GetList()) {
    const base::DictionaryValue* dict = nullptr;
    if (!el.GetAsDictionary(&dict))
      continue;

    std::string name;
    if (dict->GetString(ManagedBookmarksTracker::kFolderName, &name)) {
      return name;
    }
  }

  // FolderName not present.
  return std::string();
}

void ManagedBookmarksPolicyHandler::FilterBookmarks(base::ListValue* list) {
  // Remove any non-conforming values found.
  auto it = list->GetList().begin();
  while (it != list->GetList().end()) {
    base::DictionaryValue* dict = nullptr;
    if (!it->GetAsDictionary(&dict)) {
      it = list->Erase(it, nullptr);
      continue;
    }

    std::string name;
    std::string url;
    base::ListValue* children = nullptr;
    // Every bookmark must have a name, and then either a URL of a list of
    // child bookmarks.
    if (!dict->GetString(ManagedBookmarksTracker::kName, &name) ||
        (!dict->GetList(ManagedBookmarksTracker::kChildren, &children) &&
         !dict->GetString(ManagedBookmarksTracker::kUrl, &url))) {
      it = list->Erase(it, nullptr);
      continue;
    }

    if (children) {
      // Ignore the URL if this bookmark has child nodes.
      dict->Remove(ManagedBookmarksTracker::kUrl, nullptr);
      FilterBookmarks(children);
    } else {
      // Make sure the URL is valid before passing a bookmark to the pref.
      dict->Remove(ManagedBookmarksTracker::kChildren, nullptr);
      GURL gurl = url_formatter::FixupURL(url, std::string());
      if (!gurl.is_valid()) {
        it = list->Erase(it, nullptr);
        continue;
      }
      dict->SetString(ManagedBookmarksTracker::kUrl, gurl.spec());
    }

    ++it;
  }
}

}  // namespace bookmarks
