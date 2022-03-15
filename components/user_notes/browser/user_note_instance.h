// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_

#include "base/memory/weak_ptr.h"
#include "components/user_notes/model/user_note.h"

namespace user_notes {

// A class that represents the manifestation of a note within a specific web
// page.
class UserNoteInstance {
 public:
  explicit UserNoteInstance(base::WeakPtr<UserNote> model);
  ~UserNoteInstance();
  UserNoteInstance(const UserNoteInstance&) = delete;
  UserNoteInstance& operator=(const UserNoteInstance&) = delete;

  const UserNote* model() { return model_.get(); }

 private:
  // A weak pointer to the backing model of this note instance. The model is
  // owned by |UserNoteService|. The model is generally expected to outlive
  // this class, but due to destruction order on browser shut down, using a
  // weak pointer here is safer.
  base::WeakPtr<UserNote> model_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_INSTANCE_H_
