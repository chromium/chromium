// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_NOTES_USER_NOTES_TAB_HELPER_H_
#define CHROME_BROWSER_USER_NOTES_USER_NOTES_TAB_HELPER_H_

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
  UserNotesTabHelper(const UserNotesTabHelper&) = delete;
  UserNotesTabHelper& operator=(const UserNotesTabHelper&) = delete;

  ~UserNotesTabHelper() override;

 private:
  friend class content::WebContentsUserData<UserNotesTabHelper>;

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
