// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/bookmarks/bookmarks_api.h"

namespace extensions {
namespace cast {
namespace api {

BookmarksStubFunction::~BookmarksStubFunction() {}

ExtensionFunction::ResponseAction BookmarksStubFunction::Run() {
  return RespondNow(Error("Not implemented"));
}

ExtensionFunction::ResponseAction BookmarksGetFunction::Run() {
  return RespondNow(ArgumentList(cast::api::bookmarks::Get::Results::Create(
      std::vector<cast::api::bookmarks::BookmarkTreeNode>())));
}

ExtensionFunction::ResponseAction BookmarksGetChildrenFunction::Run() {
  return RespondNow(
      ArgumentList(cast::api::bookmarks::GetChildren::Results::Create(
          std::vector<cast::api::bookmarks::BookmarkTreeNode>())));
}

ExtensionFunction::ResponseAction BookmarksGetRecentFunction::Run() {
  return RespondNow(
      ArgumentList(cast::api::bookmarks::GetRecent::Results::Create(
          std::vector<cast::api::bookmarks::BookmarkTreeNode>())));
}

ExtensionFunction::ResponseAction BookmarksGetTreeFunction::Run() {
  return RespondNow(ArgumentList(cast::api::bookmarks::GetTree::Results::Create(
      std::vector<cast::api::bookmarks::BookmarkTreeNode>())));
}

ExtensionFunction::ResponseAction BookmarksGetSubTreeFunction::Run() {
  return RespondNow(
      ArgumentList(cast::api::bookmarks::GetSubTree::Results::Create(
          std::vector<cast::api::bookmarks::BookmarkTreeNode>())));
}

ExtensionFunction::ResponseAction BookmarksSearchFunction::Run() {
  return RespondNow(ArgumentList(cast::api::bookmarks::Search::Results::Create(
      std::vector<cast::api::bookmarks::BookmarkTreeNode>())));
}

}  // namespace api
}  // namespace cast
}  // namespace extensions
