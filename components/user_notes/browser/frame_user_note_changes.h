// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_
#define COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace user_notes {

typedef std::vector<base::UnguessableToken> ChangeList;

// A container to represent changes to a frame's displayed User Notes. Includes
// the logic to apply the changes in the associated frame.
class FrameUserNoteChanges {
 public:
  FrameUserNoteChanges(content::RenderFrameHost* rfh,
                       const ChangeList& notes_added,
                       const ChangeList& notes_modified,
                       const ChangeList& notes_removed);
  FrameUserNoteChanges(content::RenderFrameHost* rfh,
                       ChangeList&& notes_added,
                       ChangeList&& notes_modified,
                       ChangeList&& notes_removed);
  FrameUserNoteChanges(const FrameUserNoteChanges&) = delete;
  FrameUserNoteChanges& operator=(const FrameUserNoteChanges&) = delete;
  FrameUserNoteChanges(FrameUserNoteChanges&& other);
  ~FrameUserNoteChanges();

  // Kicks off the asynchronous logic to propagate the note changes to the
  // frame, namely creating and removing highlights as necessary. Then, notifies
  // the note service that the changes have been fully applied for this frame so
  // it can request the UI to refresh itself as needed.
  void Apply();

 private:
  FRIEND_TEST_ALL_PREFIXES(UserNoteUtilsTest, CalculateNoteChanges);

  raw_ptr<content::RenderFrameHost> rfh_;
  ChangeList notes_added_;
  ChangeList notes_modified_;
  ChangeList notes_removed_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_FRAME_USER_NOTE_CHANGES_H_
