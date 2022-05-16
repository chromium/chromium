// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_
#define COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_

#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/unguessable_token.h"
#include "components/user_notes/browser/user_note_instance.h"
#include "components/user_notes/browser/user_note_service.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace user_notes {

// A container to represent changes to a frame's displayed User Notes. Includes
// the logic to apply the changes in the associated frame.
class FrameUserNoteChanges {
 public:
  using ChangeList = std::vector<base::UnguessableToken>;

  FrameUserNoteChanges(base::SafeRef<UserNoteService> service,
                       content::RenderFrameHost* rfh,
                       const ChangeList& notes_added,
                       const ChangeList& notes_modified,
                       const ChangeList& notes_removed);
  FrameUserNoteChanges(base::SafeRef<UserNoteService> service,
                       content::RenderFrameHost* rfh,
                       ChangeList&& notes_added,
                       ChangeList&& notes_modified,
                       ChangeList&& notes_removed);
  FrameUserNoteChanges(const FrameUserNoteChanges&) = delete;
  FrameUserNoteChanges& operator=(const FrameUserNoteChanges&) = delete;
  FrameUserNoteChanges(FrameUserNoteChanges&& other);
  virtual ~FrameUserNoteChanges();

  // Kicks off the asynchronous logic to add and remove highlights in the frame
  // as necessary. Invokes the provided callback after the changes have fully
  // propagated to the note manager and the new notes have had their highlights
  // created in the web page.
  void Apply(base::OnceClosure callback);

 protected:
  // Called by `Apply()` to construct a new note instance pointing to the
  // provided model. Can be overridden by tests to construct a mocked instance.
  virtual std::unique_ptr<UserNoteInstance> MakeNoteInstance(
      const UserNote* note_model,
      UserNoteManager* manager) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(UserNoteUtilsTest, CalculateNoteChanges);

  base::SafeRef<UserNoteService> service_;
  raw_ptr<content::RenderFrameHost> rfh_;
  ChangeList notes_added_;
  ChangeList notes_modified_;
  ChangeList notes_removed_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_
