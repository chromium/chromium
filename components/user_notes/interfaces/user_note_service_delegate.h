// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_SERVICE_DELEGATE_H_
#define COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_SERVICE_DELEGATE_H_

#include <vector>

namespace content {
class RenderFrameHost;
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

  // Called by the `UserNoteService` to get the list of all frames for which
  // notes should be updated.
  virtual std::vector<content::RenderFrameHost*> GetAllFramesForUserNotes() = 0;

  // Called by the `UserNoteService` to get a handle to the UI coordinator
  // associated with the given frame so it can post commands to the UI, for
  // example to bring a note into focus or start the note creation flow.
  virtual UserNotesUI* GetUICoordinatorForFrame(
      const content::RenderFrameHost* rfh) = 0;

  // Called by the `UserNoteService` to determine whether the given frame is
  // part of the active tab of its owning browser window.
  virtual bool IsFrameInActiveTab(const content::RenderFrameHost* rfh) = 0;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_SERVICE_DELEGATE_H_
