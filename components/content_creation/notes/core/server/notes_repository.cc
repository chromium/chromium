// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/server/notes_repository.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/notes/core/server/note_data.h"
#include "components/content_creation/notes/core/server/notes_server_saver.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content_creation {

NotesRepository::NotesRepository(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      channel_(channel) {}

NotesRepository::~NotesRepository() = default;

void NotesRepository::PublishNote(const NoteData& note_data,
                                  PublishNoteCallback callback) {
  // Only start publishing if the functionality is available.
  if (!IsPublishAvailable()) {
    return;
  }

  // Only start publishing if it has not failed in the past.
  if (has_failed_publish_) {
    return;
  }

  notes_saver_ = std::make_unique<NotesServerSaver>(
      url_loader_factory_, identity_manager_, note_data,
      base::BindOnce(&NotesRepository::OnNotePublished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  notes_saver_->Start();
}

bool NotesRepository::IsPublishAvailable() const {
  return channel_ == version_info::Channel::CANARY && IsPublishEnabled() &&
         !has_failed_publish_;
}

// Used for tests.
NotesRepository::NotesRepository() : channel_(version_info::Channel::UNKNOWN) {}

void NotesRepository::OnNotePublished(PublishNoteCallback callback,
                                      SaveNoteResponse save_response) {
  DCHECK(IsPublishAvailable());

  if (save_response.account_id.empty() && save_response.note_id.empty()) {
    has_failed_publish_ = true;
  }

  notes_saver_.reset();

  std::move(callback).Run("https://channel-staging.sandbox.google.com/user/" +
                          save_response.account_id + "/" +
                          save_response.note_id);
}

}  // namespace content_creation
