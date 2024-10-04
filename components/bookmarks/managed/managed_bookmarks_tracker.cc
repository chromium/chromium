// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmarks_tracker.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace bookmarks {

const char ManagedBookmarksTracker::kName[] = "name";
const char ManagedBookmarksTracker::kUrl[] = "url";
const char ManagedBookmarksTracker::kChildren[] = "children";
const char ManagedBookmarksTracker::kFolderName[] = "toplevel_name";

ManagedBookmarksTracker::ManagedBookmarksTracker(
    BookmarkModel* model,
    PrefService* prefs,
    GetManagementDomainCallback callback)
    : model_(model),
      managed_node_(nullptr),
      prefs_(prefs),
      get_management_domain_callback_(std::move(callback)) {}

ManagedBookmarksTracker::~ManagedBookmarksTracker() = default;

base::Value::List ManagedBookmarksTracker::GetInitialManagedBookmarks() {
  const base::Value::List& list = prefs_->GetList(prefs::kManagedBookmarks);
  return list.Clone();
}

// static
int64_t ManagedBookmarksTracker::LoadInitial(BookmarkNode* folder,
                                             const base::Value::List& list,
                                             int64_t next_node_id) {
  for (size_t i = 0; i < list.size(); ++i) {
    // Extract the data for the next bookmark from the |list|.
    std::u16string title;
    GURL url;
    const base::Value::List* children = nullptr;
    if (!LoadBookmark(list, i, &title, &url, &children))
      continue;

    BookmarkNode* child = folder->Add(std::make_unique<BookmarkNode>(
        next_node_id++, base::Uuid::GenerateRandomV4(), url));
    child->SetTitle(title);
    if (children) {
      child->set_date_folder_modified(base::Time::Now());
      next_node_id = LoadInitial(child, *children, next_node_id);
    } else {
      child->set_date_added(base::Time::Now());
    }
  }

  return next_node_id;
}

void ManagedBookmarksTracker::Init(BookmarkPermanentNode* managed_node) {
  managed_node_ = managed_node;
  registrar_.Init(prefs_);
  registrar_.Add(
      prefs::kManagedBookmarks,
      base::BindRepeating(&ManagedBookmarksTracker::ReloadManagedBookmarks,
                          base::Unretained(this)));
  registrar_.Add(
      prefs::kManagedBookmarksFolderName,
      base::BindRepeating(
          &ManagedBookmarksTracker::ReloadManagedBookmarksFolderTitle,
          base::Unretained(this)));
  // Reload now just in case something changed since the initial load started.
  // Note that  we must not load managed bookmarks until the cloud policy system
  // has been fully initialized (which will make our preference a managed
  // preference).
  if (prefs_->IsManagedPreference(prefs::kManagedBookmarks))
    ReloadManagedBookmarks();
}

std::u16string ManagedBookmarksTracker::GetBookmarksFolderTitle() const {
  std::string name = prefs_->GetString(prefs::kManagedBookmarksFolderName);
  if (!name.empty())
    return base::UTF8ToUTF16(name);

  const std::string domain = get_management_domain_callback_.Run();
  if (domain.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_BOOKMARK_BAR_MANAGED_FOLDER_DEFAULT_NAME);
  }
  return l10n_util::GetStringFUTF16(IDS_BOOKMARK_BAR_MANAGED_FOLDER_DOMAIN_NAME,
                                    base::UTF8ToUTF16(domain));
}

void ManagedBookmarksTracker::ReloadManagedBookmarksFolderTitle() {
  model_->SetTitle(managed_node_, GetBookmarksFolderTitle(),
                   bookmarks::metrics::BookmarkEditSource::kOther);
}

void ManagedBookmarksTracker::ReloadManagedBookmarks() {
  // In case the user just signed into or out of the account.
  ReloadManagedBookmarksFolderTitle();
  // Recursively update all the managed bookmarks and folders.
  const base::Value::List& list = prefs_->GetList(prefs::kManagedBookmarks);
  UpdateBookmarks(managed_node_, list);
}

void ManagedBookmarksTracker::UpdateBookmarks(const BookmarkNode* folder,
                                              const base::Value::List& list) {
  size_t folder_index = 0;
  for (size_t i = 0; i < list.size(); ++i) {
    // Extract the data for the next bookmark from the |list|.
    std::u16string title;
    GURL url;
    const base::Value::List* children = nullptr;
    if (!LoadBookmark(list, i, &title, &url, &children)) {
      // Skip this bookmark from |list| but don't advance |folder_index|.
      continue;
    }

    // Look for a bookmark at |folder_index| or ahead that matches the current
    // bookmark from the pref.
    const auto matches_current = [&title, &url, children](const auto& node) {
      return node->GetTitle() == title &&
             (children ? node->is_folder() : (node->url() == url));
    };
    const auto j = std::find_if(folder->children().cbegin() + folder_index,
                                folder->children().cend(), matches_current);
    if (j != folder->children().cend()) {
      // Reuse the existing node. The Move() is a nop if |existing| is already
      // at |folder_index|.
      const BookmarkNode* existing = j->get();
      model_->Move(existing, folder, folder_index);
      if (children)
        UpdateBookmarks(existing, *children);
    } else if (children) {
      UpdateBookmarks(model_->AddFolder(folder, folder_index, title),
                      *children);
    } else {
      model_->AddURL(folder, folder_index, title, url);
    }

    // The |folder_index| index of |folder| has been updated, so advance it.
    ++folder_index;
  }

  // Remove any extra children of |folder| that haven't been reused.
  while (folder->children().size() != folder_index)
    model_->Remove(folder->children()[folder_index].get(),
                   bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);
}

// static
bool ManagedBookmarksTracker::LoadBookmark(const base::Value::List& list,
                                           size_t index,
                                           std::u16string* title,
                                           GURL* url,
                                           const base::Value::List** children) {
  *url = GURL();
  *children = nullptr;
  const base::Value::Dict* dict = list[index].GetIfDict();
  if (!dict) {
    // Should never happen after policy validation.
    NOTREACHED_IN_MIGRATION();
    return false;
  }
  const std::string* name = dict->FindString(kName);
  const std::string* spec = dict->FindString(kUrl);
  const base::Value::List* children_list = dict->FindList(kChildren);
  if (!name || (!spec && !children_list)) {
    // Should never happen after policy validation.
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  *title = base::UTF8ToUTF16(*name);
  *children = children_list;
  if (!*children) {
    *url = GURL(*spec);
    DCHECK(url->is_valid());
  }
  return true;
}

}  // namespace bookmarks
