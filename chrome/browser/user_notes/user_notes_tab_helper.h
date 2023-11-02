// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_NOTES_USER_NOTES_TAB_HELPER_H_
#define CHROME_BROWSER_USER_NOTES_USER_NOTES_TAB_HELPER_H_

#include "base/gtest_prod_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class Page;
class WebContents;
}  // namespace content

namespace user_notes {

class UserNoteService;

// An observer of WebContents that attaches User Notes page data to primary
// pages, and notifies the User Notes service when new URLs are navigated to so
// it can display notes.
class UserNotesTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<UserNotesTabHelper> {
 public:
  // Exposes a way to construct this object from unit tests. Do not use in
  // product code; instead, use `UserNotesTabHelper::CreateForWebContents`,
  // inherited from `WebContentsUserData`.
  static std::unique_ptr<UserNotesTabHelper> CreateForTest(
      content::WebContents* web_contents);

  UserNotesTabHelper(const UserNotesTabHelper&) = delete;
  UserNotesTabHelper& operator=(const UserNotesTabHelper&) = delete;

  ~UserNotesTabHelper() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(UserNotesTabHelperTest,
                           NotifyServiceOfNavigationWhenNeeded);
  friend class content::WebContentsUserData<UserNotesTabHelper>;
  friend class MockUserNotesTabHelper;

  explicit UserNotesTabHelper(content::WebContents* web_contents);

  UserNoteService* service() const;

  // WebContentsObserver implementation.
  void PrimaryPageChanged(content::Page& page) override;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace user_notes

#endif  // CHROME_BROWSER_USER_NOTES_USER_NOTES_TAB_HELPER_H_
