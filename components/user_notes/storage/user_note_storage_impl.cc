// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_storage_impl.h"

#include "base/bind.h"
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
  // An empty `Then()` is needed to satisfy a DCHECK in `AsyncCall` because
  // `UserNoteDatabase::Init` returns a value.
  database_.AsyncCall(&UserNoteDatabase::Init)
      .Then(base::BindOnce([](bool result) {}));
}

UserNoteStorageImpl::~UserNoteStorageImpl() = default;

void UserNoteStorageImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UserNoteStorageImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UserNoteStorageImpl::GetNoteMetadataForUrls(
    const std::vector<GURL>& urls,
    base::OnceCallback<void(UserNoteMetadataSnapshot)> callback) {
  database_.AsyncCall(&UserNoteDatabase::GetNoteMetadataForUrls)
      .WithArgs(std::move(urls))
      .Then(std::move(callback));
}

void UserNoteStorageImpl::GetNotesById(
    const std::vector<base::UnguessableToken>& ids,
    base::OnceCallback<void(std::vector<std::unique_ptr<UserNote>>)> callback) {
  database_.AsyncCall(&UserNoteDatabase::GetNotesById)
      .WithArgs(std::move(ids))
      .Then(std::move(callback));
}

void UserNoteStorageImpl::UpdateNote(const UserNote* model,
                                     std::u16string note_body_text,
                                     bool is_creation) {
  database_.AsyncCall(&UserNoteDatabase::UpdateNote)
      .WithArgs(model, note_body_text, is_creation)
      .Then(base::BindOnce(&UserNoteStorageImpl::OnNotesChanged,
                           weak_factory_.GetWeakPtr()));
}

void UserNoteStorageImpl::DeleteNote(const base::UnguessableToken& id) {
  database_.AsyncCall(&UserNoteDatabase::DeleteNote)
      .WithArgs(id)
      .Then(base::BindOnce(&UserNoteStorageImpl::OnNotesChanged,
                           weak_factory_.GetWeakPtr()));
}

void UserNoteStorageImpl::DeleteAllForUrl(const GURL& url) {
  database_.AsyncCall(&UserNoteDatabase::DeleteAllForUrl)
      .WithArgs(url)
      .Then(base::BindOnce(&UserNoteStorageImpl::OnNotesChanged,
                           weak_factory_.GetWeakPtr()));
}

void UserNoteStorageImpl::DeleteAllForOrigin(const url::Origin& origin) {
  database_.AsyncCall(&UserNoteDatabase::DeleteAllForOrigin)
      .WithArgs(origin)
      .Then(base::BindOnce(&UserNoteStorageImpl::OnNotesChanged,
                           weak_factory_.GetWeakPtr()));
}

void UserNoteStorageImpl::DeleteAllNotes() {
  database_.AsyncCall(&UserNoteDatabase::DeleteAllNotes)
      .Then(base::BindOnce(&UserNoteStorageImpl::OnNotesChanged,
                           weak_factory_.GetWeakPtr()));
}

void UserNoteStorageImpl::OnNotesChanged(bool notes_changed) {
  if (!notes_changed)
    return;

  for (auto& observer : observers_)
    observer.OnNotesChanged();
}

}  // namespace user_notes
