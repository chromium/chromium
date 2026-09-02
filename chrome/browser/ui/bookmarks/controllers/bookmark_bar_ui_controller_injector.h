// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_INJECTOR_H_
#define CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_INJECTOR_H_

class BookmarkBarActionAdapter;
class BookmarkBarModelAdapter;
class BookmarkBarPrefsAdapter;

class BookmarkBarUIControllerInjector {
 public:
  virtual ~BookmarkBarUIControllerInjector() = default;

  // Returns the preference adapter. The injector retains ownership.
  virtual BookmarkBarPrefsAdapter* GetPrefsAdapter() = 0;

  // Returns the action adapter. The injector retains ownership.
  virtual BookmarkBarActionAdapter* GetActionAdapter() = 0;

  // Returns the model adapter. The injector retains ownership.
  virtual BookmarkBarModelAdapter* GetModelAdapter() = 0;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_CONTROLLERS_BOOKMARK_BAR_UI_CONTROLLER_INJECTOR_H_
