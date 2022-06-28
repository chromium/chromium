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

  if (!value || !value->is_list())
    return;

  prefs->SetString(prefs::kManagedBookmarksFolderName, GetFolderName(*value));
  base::Value::List filtered(FilterBookmarks(std::move(value->GetList())));
  prefs->SetValue(prefs::kManagedBookmarks, base::Value(std::move(filtered)));
}

std::string ManagedBookmarksPolicyHandler::GetFolderName(
    const base::Value& list) {
  DCHECK(list.is_list());
  // Iterate over the list, and try to find the FolderName.
  for (const auto& el : list.GetListDeprecated()) {
    if (!el.is_dict())
      continue;

    const std::string* name =
        el.FindStringKey(ManagedBookmarksTracker::kFolderName);
    if (name)
      return *name;
  }

  // FolderName not present.
  return std::string();
}

base::Value::List ManagedBookmarksPolicyHandler::FilterBookmarks(
    base::Value::List list) {
  // Move over conforming values found.
  base::Value::List out;

  for (base::Value& item : list) {
    if (!item.is_dict())
      continue;

    base::Value::Dict& dict = item.GetDict();
    const std::string* name = dict.FindString(ManagedBookmarksTracker::kName);
    const std::string* url = dict.FindString(ManagedBookmarksTracker::kUrl);
    base::Value::List* children =
        dict.FindList(ManagedBookmarksTracker::kChildren);
    // Every bookmark must have a name, and then either a URL of a list of
    // child bookmarks.
    if (!name || (!url && !children))
      continue;

    if (children) {
      *children = FilterBookmarks(std::move(*children));
      // Ignore the URL if this bookmark has child nodes. Note that this needs
      // to be after `children` is overwritten, in case removing an entry from
      // the dictionary invalidates its pointers.
      dict.Remove(ManagedBookmarksTracker::kUrl);
    } else {
      // Make sure the URL is valid before passing a bookmark to the pref.
      dict.Remove(ManagedBookmarksTracker::kChildren);
      GURL gurl = url_formatter::FixupURL(*url, std::string());
      if (!gurl.is_valid()) {
        continue;
      }
      dict.Set(ManagedBookmarksTracker::kUrl, gurl.spec());
    }

    out.Append(std::move(item));
  }
  return out;
}

}  // namespace bookmarks
