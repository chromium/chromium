// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_storage_impl.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"

namespace user_notes {

// `BLOCK_SHUTDOWN` is used to delay browser shutdown until storage operations
// are complete to prevent possible data corruption.
UserNoteStorageImpl::UserNoteStorageImpl(
    const base::FilePath& path_to_database_dir)
    : database_(base::ThreadPool::CreateSequencedTaskRunner(
                    {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                     base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                path_to_database_dir) {
  database_.AsyncCall(&UserNoteDatabase::Init);
}

void UserNoteStorageImpl::GetNoteMetadataForUrls(
    std::vector<GURL> urls,
    base::OnceCallback<void(UserNoteMetadataSnapshot)> callback) {
  database_.AsyncCall(&UserNoteDatabase::GetNoteMetadataForUrls)
      .WithArgs(std::move(urls))
      .Then(std::move(callback));
}

void UserNoteStorageImpl::GetNotesById(
    std::vector<base::UnguessableToken> ids,
    base::OnceCallback<void(std::vector<std::unique_ptr<UserNote>>)> callback) {
  database_.AsyncCall(&UserNoteDatabase::GetNotesById)
      .WithArgs(std::move(ids))
      .Then(std::move(callback));
}

void UserNoteStorageImpl::CreateNote(base::UnguessableToken id,
                                     std::string note_body_text,
                                     UserNoteTarget::TargetType target_type,
                                     std::string original_text,
                                     GURL target_page,
                                     std::string selector) {
  database_.AsyncCall(&UserNoteDatabase::CreateNote)
      .WithArgs(id, note_body_text, target_type, original_text, target_page,
                selector);
}

void UserNoteStorageImpl::UpdateNote(base::UnguessableToken id,
                                     std::string note_body_text) {
  database_.AsyncCall(&UserNoteDatabase::UpdateNote)
      .WithArgs(id, note_body_text);
}

void UserNoteStorageImpl::DeleteNote(const base::UnguessableToken& id) {
  database_.AsyncCall(&UserNoteDatabase::DeleteNote).WithArgs(id);
}

void UserNoteStorageImpl::DeleteAllForUrl(const GURL& url) {
  database_.AsyncCall(&UserNoteDatabase::DeleteAllForUrl).WithArgs(url);
}

void UserNoteStorageImpl::DeleteAllForOrigin(const url::Origin& origin) {
  database_.AsyncCall(&UserNoteDatabase::DeleteAllForOrigin).WithArgs(origin);
}

void UserNoteStorageImpl::DeleteAllNotes() {
  database_.AsyncCall(&UserNoteDatabase::DeleteAllNotes);
}

}  // namespace user_notes
