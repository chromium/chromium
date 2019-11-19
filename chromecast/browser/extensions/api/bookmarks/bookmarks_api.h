// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_

#include <vector>

#include "chromecast/common/extensions_api/bookmarks.h"
#include "extensions/browser/extension_function.h"

namespace extensions {
namespace cast {
namespace api {

class BookmarksStubFunction : public ExtensionFunction {
 public:
  ResponseAction Run() override;

 protected:
  ~BookmarksStubFunction() override;
};

class BookmarksGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.get", BOOKMARKS_GET)

 protected:
  ~BookmarksGetFunction() override {}

  ResponseAction Run() override;
};

class BookmarksGetChildrenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getChildren", BOOKMARKS_GETCHILDREN)

 protected:
  ~BookmarksGetChildrenFunction() override {}

  ResponseAction Run() override;
};

class BookmarksGetRecentFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getRecent", BOOKMARKS_GETRECENT)

 protected:
  ~BookmarksGetRecentFunction() override {}

  ResponseAction Run() override;
};

class BookmarksGetTreeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getTree", BOOKMARKS_GETTREE)

 protected:
  ~BookmarksGetTreeFunction() override {}

  ResponseAction Run() override;
};

class BookmarksGetSubTreeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.getSubTree", BOOKMARKS_GETSUBTREE)

 protected:
  ~BookmarksGetSubTreeFunction() override {}

  ResponseAction Run() override;
};

class BookmarksSearchFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.search", BOOKMARKS_SEARCH)

 protected:
  ~BookmarksSearchFunction() override {}

  ResponseAction Run() override;
};

class BookmarksRemoveFunction : public BookmarksStubFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.remove", BOOKMARKS_REMOVE)

 protected:
  ~BookmarksRemoveFunction() override {}
};

class BookmarksRemoveTreeFunction : public BookmarksStubFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.removeTree", BOOKMARKS_REMOVETREE)

 protected:
  ~BookmarksRemoveTreeFunction() override {}
};

class BookmarksCreateFunction : public BookmarksStubFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.create", BOOKMARKS_CREATE)

 protected:
  ~BookmarksCreateFunction() override {}
};

class BookmarksMoveFunction : public BookmarksStubFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.move", BOOKMARKS_MOVE)

 protected:
  ~BookmarksMoveFunction() override {}
};

class BookmarksUpdateFunction : public BookmarksStubFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.update", BOOKMARKS_UPDATE)

 protected:
  ~BookmarksUpdateFunction() override {}
};

class BookmarksImportFunction : public BookmarksStubFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.import", BOOKMARKS_IMPORT)

 private:
  ~BookmarksImportFunction() override {}
};

class BookmarksExportFunction : public BookmarksStubFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarks.export", BOOKMARKS_EXPORT)

 private:
  ~BookmarksExportFunction() override {}
};

}  // namespace api
}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_BOOKMARKS_BOOKMARKS_API_H_
