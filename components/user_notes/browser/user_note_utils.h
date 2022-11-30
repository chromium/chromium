// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_UTILS_H_
#define COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_UTILS_H_

#include <memory>
#include <unordered_map>
#include <vector>

namespace content {
class WeakDocumentPtr;
}  // namespace content

namespace user_notes {

class FrameUserNoteChanges;
class UserNoteMetadataSnapshot;
class UserNoteService;

// Compares the notes each frame currently contains with the notes it should
// actually contain based on the provided metadata snapshot. A
// `FrameUserNoteChanges` object is generated for each frame where notes
// don't match the metadata.
std::vector<std::unique_ptr<FrameUserNoteChanges>> CalculateNoteChanges(
    const UserNoteService& note_service,
    const std::vector<content::WeakDocumentPtr>& documents,
    const UserNoteMetadataSnapshot& metadata_snapshot);

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_BROWSER_USER_NOTE_UTILS_H_
