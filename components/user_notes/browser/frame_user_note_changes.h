// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_
#define COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_

#include <vector>

#include "base/functional/callback.h"
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
                       content::WeakDocumentPtr document,
                       const ChangeList& notes_added,
                       const ChangeList& notes_modified,
                       const ChangeList& notes_removed);
  FrameUserNoteChanges(base::SafeRef<UserNoteService> service,
                       content::WeakDocumentPtr document,
                       ChangeList&& notes_added,
                       ChangeList&& notes_modified,
                       ChangeList&& notes_removed);
  FrameUserNoteChanges(const FrameUserNoteChanges&) = delete;
  FrameUserNoteChanges& operator=(const FrameUserNoteChanges&) = delete;
  FrameUserNoteChanges(FrameUserNoteChanges&& other);
  virtual ~FrameUserNoteChanges();

  const base::UnguessableToken& id() const { return id_; }
  const ChangeList& notes_added() const { return notes_added_; }
  const ChangeList& notes_modified() const { return notes_modified_; }
  const content::RenderFrameHost* render_frame_host() const {
    return document_.AsRenderFrameHostIfValid();
  }

  // Kicks off the asynchronous logic to add and remove highlights in the frame
  // as necessary. Invokes the provided callback after the changes have fully
  // propagated to the note manager and the new notes have had their highlights
  // created in the web page. Marked virtual for tests to override.
  virtual void Apply(base::OnceClosure callback);

 protected:
  // Called by `Apply()` to construct a new note instance pointing to the
  // provided model. Can be overridden by tests to construct a mocked instance.
  virtual std::unique_ptr<UserNoteInstance> MakeNoteInstance(
      const UserNote* note_model,
      UserNoteManager* manager) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(UserNoteUtilsTest, CalculateNoteChanges);

  // An internal ID for this change, so it can be stored and retrieved later.
  base::UnguessableToken id_;

  base::SafeRef<UserNoteService> service_;
  content::WeakDocumentPtr document_;
  ChangeList notes_added_;
  ChangeList notes_modified_;
  ChangeList notes_removed_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_
