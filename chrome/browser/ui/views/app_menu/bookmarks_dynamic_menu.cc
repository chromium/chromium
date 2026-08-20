// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/bookmarks_dynamic_menu.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_id.h"

BookmarksDynamicMenu::BookmarksDynamicMenu(BrowserWindowInterface* browser)
    : browser_window_interface_(browser) {}

BookmarksDynamicMenu::~BookmarksDynamicMenu() = default;

void BookmarksDynamicMenu::BuildBookmarksActions(
    actions::BaseAction* parent_item) {
  if (!parent_item || !browser_window_interface_) {
    return;
  }

  BookmarkMergedSurfaceService* service =
      BookmarkMergedSurfaceServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile());
  if (!service || !service->loaded()) {
    return;
  }

  BookmarkParentFolderChildren children =
      service->GetChildren(BookmarkParentFolder::BookmarkBarFolder());

  for (const auto* node : children) {
    if (node) {
      AddBookmarkNodeAction(parent_item, node, service);
    }
  }

  BookmarkParentFolder other_folder = BookmarkParentFolder::OtherFolder();
  if (service->GetChildrenCount(other_folder) > 0) {
    AddBookmarkFolderAction(parent_item, other_folder, service);
  }

  BookmarkParentFolder mobile_folder = BookmarkParentFolder::MobileFolder();
  if (service->GetChildrenCount(mobile_folder) > 0) {
    AddBookmarkFolderAction(parent_item, mobile_folder, service);
  }
}

void BookmarksDynamicMenu::AddBookmarkNodeAction(
    actions::BaseAction* parent_item,
    const bookmarks::BookmarkNode* node,
    BookmarkMergedSurfaceService* service) {
  if (!node) {
    return;
  }

  if (node->is_folder()) {
    AddBookmarkFolderAction(
        parent_item, BookmarkParentFolder::FromFolderNode(node), service);
  } else if (node->is_url()) {
    auto builder = actions::ActionItem::Builder();
    std::u16string title = node->GetTitle().empty()
                               ? base::UTF8ToUTF16(node->url().spec())
                               : node->GetTitle();
    builder.SetText(title);

    bookmarks::BookmarkModel* model = service->bookmark_model();
    if (model) {
      const gfx::Image& image = model->GetFavicon(node);
      if (!image.IsEmpty()) {
        builder.SetImage(ui::ImageModel::FromImage(image));
      } else {
        builder.SetImage(favicon::GetDefaultFaviconModel());
      }
    }

    GURL url = node->url();
    builder.SetInvokeActionCallback(base::BindRepeating(
        [](BrowserWindowInterface* browser, GURL url, actions::ActionItem* item,
           actions::ActionInvocationContext context) {
          if (browser) {
            browser->OpenGURL(url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
          }
        },
        browser_window_interface_, url));

    parent_item->AddChild(std::move(builder).Build());
  }
}

void BookmarksDynamicMenu::AddBookmarkFolderAction(
    actions::BaseAction* parent_item,
    const BookmarkParentFolder& folder,
    BookmarkMergedSurfaceService* service) {
  BookmarkParentFolderChildren children = service->GetChildren(folder);
  if (children.size() == 0 && !folder.HoldsNonPermanentFolder()) {
    return;
  }

  std::vector<const bookmarks::BookmarkNode*> underlying_nodes =
      service->GetUnderlyingNodes(folder);
  if (underlying_nodes.empty()) {
    return;
  }

  auto builder = actions::ActionItem::Builder();
  builder.SetText(underlying_nodes[0]->GetTitle())
      .SetImage(chrome::GetBookmarkFolderIcon(
          chrome::BookmarkFolderIconType::kNormal, ui::kColorMenuIcon));
  auto folder_action = std::move(builder).Build();

  if (children.size() == 0) {
    auto empty_builder = actions::ActionItem::Builder();
    empty_builder.SetText(u"(empty)").SetEnabled(false);
    folder_action->AddChild(std::move(empty_builder).Build());
  } else {
    for (const auto* child : children) {
      if (child) {
        AddBookmarkNodeAction(folder_action.get(), child, service);
      }
    }
  }

  parent_item->AddChild(std::move(folder_action));
}
