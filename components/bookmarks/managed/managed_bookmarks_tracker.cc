// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/managed/managed_bookmarks_tracker.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
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
    const GetManagementDomainCallback& callback)
    : model_(model),
      managed_node_(nullptr),
      prefs_(prefs),
      get_management_domain_callback_(callback) {}

ManagedBookmarksTracker::~ManagedBookmarksTracker() {}

std::unique_ptr<base::ListValue>
ManagedBookmarksTracker::GetInitialManagedBookmarks() {
  const base::ListValue* list = prefs_->GetList(prefs::kManagedBookmarks);
  return base::WrapUnique(list->DeepCopy());
}

// static
int64_t ManagedBookmarksTracker::LoadInitial(BookmarkNode* folder,
                                             const base::ListValue* list,
                                             int64_t next_node_id) {
  for (size_t i = 0; i < list->GetSize(); ++i) {
    // Extract the data for the next bookmark from the |list|.
    base::string16 title;
    GURL url;
    const base::ListValue* children = nullptr;
    if (!LoadBookmark(list, i, &title, &url, &children))
      continue;

    BookmarkNode* child = folder->Add(std::make_unique<BookmarkNode>(
        next_node_id++, base::GenerateGUID(), url));
    child->SetTitle(title);
    if (children) {
      child->set_date_folder_modified(base::Time::Now());
      next_node_id = LoadInitial(child, children, next_node_id);
    } else {
      child->set_date_added(base::Time::Now());
    }
  }

  return next_node_id;
}

void ManagedBookmarksTracker::Init(BookmarkPermanentNode* managed_node) {
  managed_node_ = managed_node;
  registrar_.Init(prefs_);
  registrar_.Add(prefs::kManagedBookmarks,
                 base::Bind(&ManagedBookmarksTracker::ReloadManagedBookmarks,
                            base::Unretained(this)));
  // Reload now just in case something changed since the initial load started.
  // Note that  we must not load managed bookmarks until the cloud policy system
  // has been fully initialized (which will make our preference a managed
  // preference).
  if (prefs_->IsManagedPreference(prefs::kManagedBookmarks))
    ReloadManagedBookmarks();
}

base::string16 ManagedBookmarksTracker::GetBookmarksFolderTitle() const {
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

void ManagedBookmarksTracker::ReloadManagedBookmarks() {
  // In case the user just signed into or out of the account.
  model_->SetTitle(managed_node_, GetBookmarksFolderTitle());

  // Recursively update all the managed bookmarks and folders.
  const base::ListValue* list = prefs_->GetList(prefs::kManagedBookmarks);
  UpdateBookmarks(managed_node_, list);

  // The managed bookmarks folder isn't visible when that pref isn't present.
  managed_node_->set_visible(!managed_node_->children().empty());
}

void ManagedBookmarksTracker::UpdateBookmarks(const BookmarkNode* folder,
                                              const base::ListValue* list) {
  size_t folder_index = 0;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    // Extract the data for the next bookmark from the |list|.
    base::string16 title;
    GURL url;
    const base::ListValue* children = nullptr;
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
        UpdateBookmarks(existing, children);
    } else if (children) {
      UpdateBookmarks(model_->AddFolder(folder, folder_index, title), children);
    } else {
      model_->AddURL(folder, folder_index, title, url);
    }

    // The |folder_index| index of |folder| has been updated, so advance it.
    ++folder_index;
  }

  // Remove any extra children of |folder| that haven't been reused.
  while (folder->children().size() != folder_index)
    model_->Remove(folder->children()[folder_index].get());
}

// static
bool ManagedBookmarksTracker::LoadBookmark(const base::ListValue* list,
                                           size_t index,
                                           base::string16* title,
                                           GURL* url,
                                           const base::ListValue** children) {
  std::string spec;
  *url = GURL();
  *children = nullptr;
  const base::DictionaryValue* dict = nullptr;
  if (!list->GetDictionary(index, &dict) ||
      !dict->GetString(kName, title) ||
      (!dict->GetString(kUrl, &spec) &&
       !dict->GetList(kChildren, children))) {
    // Should never happen after policy validation.
    NOTREACHED();
    return false;
  }
  if (!*children) {
    *url = GURL(spec);
    DCHECK(url->is_valid());
  }
  return true;
}

}  // namespace policy
