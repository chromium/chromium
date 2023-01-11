// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_SERVER_SAVER_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_SERVER_SAVER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/content_creation/notes/core/server/note_data.h"
#include "components/content_creation/notes/core/server/notes_server_base.h"
#include "components/content_creation/notes/core/server/save_note_response.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}

namespace content_creation {

// Class used to save a note to the Notes Server.
class NotesServerSaver : NotesServerBase {
 public:
  explicit NotesServerSaver(
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      signin::IdentityManager* identity_manager,
      NoteData note_data,
      base::OnceCallback<void(SaveNoteResponse)> callback);
  ~NotesServerSaver() override;

  // Starts the process of saving a note on the server.
  void Start() override;

  // Not copyable or movable.
  NotesServerSaver(const NotesServerSaver&) = delete;
  NotesServerSaver& operator=(const NotesServerSaver&) = delete;

 private:
  // Called when the note has been saved to the server. Calls back
  // |save_callback_|.
  void OnSaveNoteComplete(std::unique_ptr<std::string> response_body);

  // Creates and sends the request to put the webnote to the server. Should be
  // called after getting the access token.
  void SendSaveNoteRequest();

  // Called when the access token have finished fetching. Will start the process
  // to save the note to the server.
  void AccessTokenFetchFinished(
      base::TimeTicks token_start_ticks,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info) override;

  // The note data to post to the server.
  NoteData note_data_;

  // The callback used after saving the note.
  base::OnceCallback<void(SaveNoteResponse)> save_callback_;

  base::WeakPtrFactory<NotesServerSaver> weak_factory_{this};
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_SERVER_SAVER_H_
