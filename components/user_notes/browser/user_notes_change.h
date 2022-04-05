// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTES_CHANGE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTES_CHANGE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace user_notes {

typedef std::vector<base::UnguessableToken> ChangeList;

// A container to represent changes to a web page's (or more precisely, a render
// frame's) displayed User Notes. Includes logic to actually apply the changes
// in the associated frame and the User Notes UI.
class UserNotesChange {
 public:
  UserNotesChange(content::RenderFrameHost* rfh,
                  const ChangeList& new_notes,
                  const ChangeList& modified_notes,
                  const ChangeList& deleted_notes);
  UserNotesChange(content::RenderFrameHost* rfh,
                  ChangeList&& new_notes,
                  ChangeList&& modified_notes,
                  ChangeList&& deleted_notes);
  ~UserNotesChange();
  UserNotesChange(const UserNotesChange&) = delete;
  UserNotesChange& operator=(const UserNotesChange&) = delete;

  // Kicks off the asynchronous logic to propagate the Notes changes to the web
  // page and UI. This includes creating and removing highlights in the frame,
  // and requesting the UI to update its list of displayed notes if the web page
  // associated with this change was in the active tab of its browser.
  void Apply();

 private:
  raw_ptr<content::RenderFrameHost> rfh_;
  ChangeList new_notes_;
  ChangeList modified_notes_;
  ChangeList deleted_notes_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTES_CHANGE_H_
