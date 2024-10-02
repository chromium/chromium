// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_editor.h"

#include <stddef.h>

#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/strings/grit/components_strings.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

const BookmarkNode* CreateNewNode(BookmarkModel* model,
                                  const BookmarkNode* parent,
                                  const BookmarkEditor::EditDetails& details,
                                  const std::u16string& new_title,
                                  const GURL& new_url) {
  const BookmarkNode* node = nullptr;
  // When create the new one to right-clicked folder, add it to the next to the
  // folder's position. Because |details.index| has a index of the folder when
  // it was right-clicked, it might cause out of range exception when another
  // bookmark manager edits contents of the folder.
  // So we must check the range.
  size_t child_count = parent->children().size();
  size_t insert_index =
      (parent == details.parent_node && details.index.has_value() &&
       details.index.value() <= child_count)
          ? details.index.value()
          : child_count;
  switch (details.type) {
    case BookmarkEditor::EditDetails::NEW_URL:
      node = model->AddNewURL(parent, insert_index, new_title, new_url);
      break;
    case BookmarkEditor::EditDetails::NEW_FOLDER: {
      node = model->AddFolder(parent, insert_index, new_title);
      for (const auto& bookmark_data : details.bookmark_data.children) {
        if (bookmark_data.url.has_value()) {
          model->AddURL(node, node->children().size(), bookmark_data.title,
                        bookmark_data.url.value());
        } else {
          const BookmarkNode* nested_node = model->AddFolder(
              node, node->children().size(), bookmark_data.title);
          for (auto& child : bookmark_data.children) {
            // We do not expect to create new folders more than 2 levels deep.
            DCHECK(child.url.has_value());
            model->AddURL(nested_node, nested_node->children().size(),
                          child.title, child.url.value());
          }
          model->SetDateFolderModified(node, base::Time::Now());
        }
      }
      model->SetDateFolderModified(parent, base::Time::Now());

      break;
    }
    case BookmarkEditor::EditDetails::EXISTING_NODE:
      NOTREACHED_IN_MIGRATION();
  }

  return node;
}

}  // namespace

BookmarkEditor::EditDetails::BookmarkData::BookmarkData() = default;

BookmarkEditor::EditDetails::BookmarkData::BookmarkData(
    BookmarkData const& other) = default;

BookmarkEditor::EditDetails::BookmarkData::~BookmarkData() = default;

BookmarkEditor::EditDetails::EditDetails(Type node_type) : type(node_type) {}

BookmarkNode::Type BookmarkEditor::EditDetails::GetNodeType() const {
  BookmarkNode::Type node_type = BookmarkNode::URL;
  switch (type) {
    case EXISTING_NODE:
      node_type = existing_node->type();
      break;
    case NEW_URL:
      node_type = BookmarkNode::URL;
      break;
    case NEW_FOLDER:
      node_type = BookmarkNode::FOLDER;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return node_type;
}

int BookmarkEditor::EditDetails::GetWindowTitleId() const {
  int dialog_title = IDS_BOOKMARK_EDITOR_TITLE;
  switch (type) {
    case EditDetails::EXISTING_NODE:
    case EditDetails::NEW_URL:
      dialog_title = (type == EditDetails::EXISTING_NODE &&
                      existing_node->type() == BookmarkNode::FOLDER) ?
          IDS_BOOKMARK_FOLDER_EDITOR_WINDOW_TITLE :
          IDS_BOOKMARK_EDITOR_TITLE;
      break;
    case EditDetails::NEW_FOLDER:
      dialog_title = bookmark_data.children.empty()
                         ? IDS_BOOKMARK_FOLDER_EDITOR_WINDOW_TITLE_NEW
                         : IDS_BOOKMARK_ALL_TABS_DIALOG_TITLE;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return dialog_title;
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::EditNode(
    const BookmarkNode* node) {
  EditDetails details(EXISTING_NODE);
  details.existing_node = node;
  if (node)
    details.parent_node = node->parent();
  return details;
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::AddNodeInFolder(
    const BookmarkNode* parent_node,
    size_t index,
    const GURL& url,
    const std::u16string& title) {
  EditDetails details(NEW_URL);
  details.parent_node = parent_node;
  details.index = index;
  details.bookmark_data.url = url;
  details.bookmark_data.title = title;
  return details;
}

BookmarkEditor::EditDetails BookmarkEditor::EditDetails::AddFolder(
    const BookmarkNode* parent_node,
    size_t index) {
  EditDetails details(NEW_FOLDER);
  details.parent_node = parent_node;
  details.index = index;
  return details;
}

BookmarkEditor::EditDetails::EditDetails(const EditDetails& other) = default;

BookmarkEditor::EditDetails::~EditDetails() = default;

// static
const BookmarkNode* BookmarkEditor::ApplyEdits(BookmarkModel* model,
                                               const BookmarkNode* new_parent,
                                               const EditDetails& details,
                                               const std::u16string& new_title,
                                               const GURL& new_url) {
  if (details.type == EditDetails::NEW_URL ||
      details.type == EditDetails::NEW_FOLDER) {
    return CreateNewNode(model, new_parent, details, new_title, new_url);
  }

  const BookmarkNode* node = details.existing_node;
  DCHECK(node);

  if (new_parent != node->parent())
    model->Move(node, new_parent, new_parent->children().size());
  if (node->is_url())
    model->SetURL(node, new_url, bookmarks::metrics::BookmarkEditSource::kUser);
  model->SetTitle(node, new_title,
                  bookmarks::metrics::BookmarkEditSource::kUser);

  return node;
}
