
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_SERVICE_DELEGATE_H_
#define COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_SERVICE_DELEGATE_H_

#include <vector>

namespace content {
class WebContents;
}  // namespace content

namespace user_notes {

class UserNotesUI;

// Interface that embedders of the UserNoteService must implement.
class UserNoteServiceDelegate {
 public:
  UserNoteServiceDelegate() = default;
  UserNoteServiceDelegate(const UserNoteServiceDelegate&) = delete;
  UserNoteServiceDelegate& operator=(const UserNoteServiceDelegate&) = delete;
  virtual ~UserNoteServiceDelegate() = default;

  // Finds the list of all |WebContents| currently open for the profile
  // associated with the UserNoteService. The service will use this information
  // to fetch the relevant notes from storage and add highlights to the
  // webpages.
  virtual std::vector<content::WebContents*> GetAllWebContents() = 0;

  // Finds and returns the UI coordinator associated with the given
  // |WebContents|. The service will use this to post commands to the UI, such
  // as bringing a note into focus, or starting the UX flow to create a new
  // note.
  virtual UserNotesUI* GetUICoordinatorForWebContents(
      const content::WebContents* wc) = 0;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_SERVICE_DELEGATE_H_
