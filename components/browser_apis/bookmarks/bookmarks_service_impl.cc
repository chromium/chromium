// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/bookmarks/bookmarks_service_impl.h"

#include <memory>

#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected_macros.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace bookmarks_api {

namespace {

template <typename T>
base::expected<void, mojo_base::mojom::ErrorPtr> CheckNotNull(
    const T& ptr,
    std::string_view name) {
  if (!ptr) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     base::StrCat({name, " cannot be null"})));
  }
  return base::ok();
}

mojo_base::mojom::ErrorPtr MakeError(mojo_base::mojom::Code code,
                                     std::string_view message) {
  return mojo_base::mojom::Error::New(code, std::string(message));
}

}  // namespace

BookmarksServiceImpl::BookmarksServiceImpl(std::unique_ptr<BookmarksView> view)
    : view_(std::move(view)) {
  CHECK(view_);
  translator_ = std::make_unique<BookmarkEventTranslator>(view_.get(), this);
}

BookmarksServiceImpl::~BookmarksServiceImpl() = default;

void BookmarksServiceImpl::Accept(
    mojo::PendingReceiver<mojom::BookmarksService> receiver) {
  receivers_.Add(&bridge_, std::move(receiver));
}

mojom::BookmarksService::GetBookmarksResult
BookmarksServiceImpl::GetBookmarks() {
  auto snapshot = mojom::BookmarksSnapshot::New();
  snapshot->root = ConvertRootNode(view_->GetRootNode());

  mojo::AssociatedRemote<mojom::BookmarksObserver> stream;
  auto pending_receiver = stream.BindNewEndpointAndPassReceiver();
  observers_.Add(std::move(stream));
  snapshot->stream = std::move(pending_receiver);

  return snapshot;
}

mojom::BookmarksService::GetBookmarkResult BookmarksServiceImpl::GetBookmark(
    const base::Uuid& id) {
  ASSIGN_OR_RETURN(const bookmarks::BookmarkNode* node,
                   view_->FindNodeByUuid(id), &MakeError,
                   mojo_base::mojom::Code::kNotFound, "Bookmark not found");
  return ConvertNode(node);
}

mojom::BookmarkNodePtr BookmarksServiceImpl::ConvertNode(
    const bookmarks::BookmarkNode* node) {
  return BookmarkEventTranslator::ConvertNode(node, view_.get());
}

mojom::RootNodePtr BookmarksServiceImpl::ConvertRootNode(
    const bookmarks::BookmarkNode* node) {
  return BookmarkEventTranslator::ConvertRootNode(node, view_.get());
}

mojom::BookmarksService::CreateBookmarkNodeResult
BookmarksServiceImpl::CreateBookmarkNode(const base::Uuid& parent_id,
                                         std::optional<int32_t> index,
                                         mojom::BookmarkNodePtr node) {
  if (!node) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "Node cannot be null"));
  }

  ASSIGN_OR_RETURN(
      const bookmarks::BookmarkNode* parent, view_->FindNodeByUuid(parent_id),
      &MakeError, mojo_base::mojom::Code::kNotFound, "Parent folder not found");

  if (!parent->is_folder()) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "Parent node is not a folder"));
  }

  size_t visual_index;
  if (index.has_value()) {
    int32_t val = index.value();
    if (val < 0) {
      return base::unexpected(
          mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                       "Index cannot be negative"));
    }
    visual_index = static_cast<size_t>(val);
    if (visual_index > view_->GetChildren(parent).size()) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "Index out of range"));
    }
  } else {
    visual_index = view_->GetChildren(parent).size();
  }

  switch (node->which()) {
    case mojom::BookmarkNode::Tag::kUrl: {
      const auto& url_data = node->get_url();
      GURL gurl = url_data->url;
      if (!gurl.is_valid()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument, "Invalid URL"));
      }
      auto* new_node = view_->AddURL(parent, visual_index,
                                     base::UTF8ToUTF16(url_data->title), gurl);
      if (!new_node) {
        return base::unexpected(
            mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInternal,
                                         "Failed to create bookmark node"));
      }
      return ConvertNode(new_node);
    }
    case mojom::BookmarkNode::Tag::kFolder: {
      const auto& folder_data = node->get_folder();
      auto* new_node = view_->AddFolder(parent, visual_index,
                                        base::UTF8ToUTF16(folder_data->title));
      if (!new_node) {
        return base::unexpected(
            mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInternal,
                                         "Failed to create bookmark node"));
      }
      return ConvertNode(new_node);
    }
  }
}

mojom::BookmarksService::UpdateBookmarkNodeResult
BookmarksServiceImpl::UpdateBookmarkNode(mojom::BookmarkNodePtr node) {
  if (!node) {
    return base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kInvalidArgument, "Node cannot be null"));
  }

  base::Uuid id;
  switch (node->which()) {
    case mojom::BookmarkNode::Tag::kUrl: {
      const auto& url_data = node->get_url();
      if (!url_data->id.has_value()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "bookmark id is required"));
      }
      id = url_data->id.value();
      break;
    }
    case mojom::BookmarkNode::Tag::kFolder: {
      const auto& folder_data = node->get_folder();
      if (!folder_data->id.has_value()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument, "folder id is required"));
      }
      id = folder_data->id.value();
      break;
    }
  }

  ASSIGN_OR_RETURN(
      const bookmarks::BookmarkNode* model_node, view_->FindNodeByUuid(id),
      &MakeError, mojo_base::mojom::Code::kNotFound, "Bookmark node not found");

  if (view_->IsPermanentNode(model_node)) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "Cannot update permanent node"));
  }

  switch (node->which()) {
    case mojom::BookmarkNode::Tag::kUrl: {
      const auto& url_data = node->get_url();
      GURL gurl = url_data->url;
      if (!gurl.is_valid()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument, "Invalid URL"));
      }
      if (model_node->is_folder()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "Cannot update folder with URL node"));
      }
      view_->SetTitle(model_node, base::UTF8ToUTF16(url_data->title),
                      bookmarks::metrics::BookmarkEditSource::kUser);
      view_->SetURL(model_node, gurl,
                    bookmarks::metrics::BookmarkEditSource::kUser);
      ASSIGN_OR_RETURN(const bookmarks::BookmarkNode* updated_node,
                       view_->FindNodeByUuid(id), &MakeError,
                       mojo_base::mojom::Code::kInternal,
                       "Failed to refetch updated node");
      return ConvertNode(updated_node);
    }
    case mojom::BookmarkNode::Tag::kFolder: {
      if (model_node->is_url()) {
        return base::unexpected(mojo_base::mojom::Error::New(
            mojo_base::mojom::Code::kInvalidArgument,
            "Cannot update URL node with folder node"));
      }
      const auto& folder_data = node->get_folder();
      view_->SetTitle(model_node, base::UTF8ToUTF16(folder_data->title),
                      bookmarks::metrics::BookmarkEditSource::kUser);
      ASSIGN_OR_RETURN(const bookmarks::BookmarkNode* updated_node,
                       view_->FindNodeByUuid(id), &MakeError,
                       mojo_base::mojom::Code::kInternal,
                       "Failed to refetch updated node");
      return ConvertNode(updated_node);
    }
  }
}

mojom::BookmarksService::MoveBookmarkNodeResult
BookmarksServiceImpl::MoveBookmarkNode(const base::Uuid& id,
                                       const base::Uuid& new_parent_id,
                                       std::optional<int32_t> index) {
  ASSIGN_OR_RETURN(
      const bookmarks::BookmarkNode* node, view_->FindNodeByUuid(id),
      &MakeError, mojo_base::mojom::Code::kNotFound, "Bookmark node not found");

  if (view_->IsPermanentNode(node)) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "Cannot move permanent node"));
  }

  ASSIGN_OR_RETURN(const bookmarks::BookmarkNode* new_parent,
                   view_->FindNodeByUuid(new_parent_id), &MakeError,
                   mojo_base::mojom::Code::kNotFound,
                   "New parent folder not found");

  if (!new_parent->is_folder()) {
    return base::unexpected(
        mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                     "New parent node is not a folder"));
  }

  size_t visual_index;
  if (index.has_value()) {
    if (index.value() < 0 || static_cast<size_t>(index.value()) >
                                 view_->GetChildren(new_parent).size()) {
      return base::unexpected(mojo_base::mojom::Error::New(
          mojo_base::mojom::Code::kInvalidArgument, "Index out of range"));
    }
    visual_index = static_cast<size_t>(index.value());
  } else {
    visual_index = view_->GetChildren(new_parent).size();
  }

  view_->Move(node, new_parent, visual_index);

  return std::monostate();
}

mojom::BookmarksService::DeleteBookmarkNodesResult
BookmarksServiceImpl::DeleteBookmarkNodes(const std::vector<base::Uuid>& ids) {
  std::vector<const bookmarks::BookmarkNode*> nodes_to_remove;
  for (const auto& id : ids) {
    ASSIGN_OR_RETURN(const bookmarks::BookmarkNode* node,
                     view_->FindNodeByUuid(id), &MakeError,
                     mojo_base::mojom::Code::kNotFound,
                     "Bookmark node not found");

    if (view_->IsPermanentNode(node)) {
      return base::unexpected(
          mojo_base::mojom::Error::New(mojo_base::mojom::Code::kInvalidArgument,
                                       "Cannot delete permanent node"));
    }
    nodes_to_remove.push_back(node);
  }

  view_->RemoveNodes(nodes_to_remove,
                     bookmarks::metrics::BookmarkEditSource::kUser, FROM_HERE);

  return std::monostate();
}

void BookmarksServiceImpl::OnBookmarkEvents(
    const std::vector<mojom::BookmarksEventPtr>& events) {
  BroadcastEvents(events);
}

void BookmarksServiceImpl::BroadcastEvents(
    const std::vector<mojom::BookmarksEventPtr>& events) {
  for (auto& observer : observers_) {
    std::vector<mojom::BookmarksEventPtr> copy;
    for (const auto& event : events) {
      copy.push_back(event.Clone());
    }
    observer->OnBookmarksEvents(std::move(copy));
  }
}

}  // namespace bookmarks_api
