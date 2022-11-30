// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_MODEL_USER_NOTE_MODEL_TEST_UTILS_H_
#define COMPONENTS_USER_NOTES_MODEL_USER_NOTE_MODEL_TEST_UTILS_H_

#include "base/time/time.h"
#include "components/user_notes/model/user_note_body.h"
#include "components/user_notes/model/user_note_metadata.h"
#include "components/user_notes/model/user_note_target.h"

namespace user_notes {

extern std::unique_ptr<UserNoteMetadata> GetTestUserNoteMetadata();

extern std::unique_ptr<UserNoteBody> GetTestUserNoteBody();

extern std::unique_ptr<UserNoteTarget> GetTestUserNotePageTarget(
    const std::string& url = "https://www.example.com");

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_MODEL_USER_NOTE_MODEL_TEST_UTILS_H_
