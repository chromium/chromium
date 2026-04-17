// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PASS_KEY_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PASS_KEY_H_

namespace page_actions {

class PageActionController;
class PageActionControllerImpl;
class PageActionView;
class ScopedPageActionActivity;
class PageActionModel;
class MockPageActionController;

// This class acts as a shared 'secret' for the page actions framework.
// Only the core logic (Controller) and the UI implementation (View)
// can construct it, ensuring that random feature clients cannot call
// sensitive internal methods.
class PageActionPassKey {
 public:
  ~PageActionPassKey() = default;
  PageActionPassKey(const PageActionPassKey&) = default;
  PageActionPassKey& operator=(const PageActionPassKey&) = default;

  static PageActionPassKey PassKeyForTesting() { return PageActionPassKey(); }

 private:
  friend class PageActionController;
  friend class PageActionControllerImpl;
  friend class PageActionView;
  friend class ScopedPageActionActivity;
  friend class PageActionModel;
  friend class MockPageActionController;

  PageActionPassKey() = default;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_PASS_KEY_H_
