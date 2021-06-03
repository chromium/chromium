// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/server/notes_server_saver.h"

#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content_creation {

NotesServerSaver::NotesServerSaver(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    signin::IdentityManager* identity_manager,
    NoteData note_data,
    base::OnceCallback<void(SaveNoteResponse)> callback)
    : NotesServerBase{loader_factory, identity_manager},
      note_data_(note_data),
      save_callback_(std::move(callback)) {}

NotesServerSaver::~NotesServerSaver() {}

void NotesServerSaver::Start() {
  NOTIMPLEMENTED();
}

void NotesServerSaver::SendSaveNoteRequest() {
  NOTIMPLEMENTED();
}

void NotesServerSaver::OnSaveNoteComplete(
    std::unique_ptr<std::string> response_body) {
  NOTIMPLEMENTED();
}

void NotesServerSaver::AccessTokenFetchFinished(
    base::TimeTicks token_start_ticks,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  NOTIMPLEMENTED();
}

}  // namespace content_creation
