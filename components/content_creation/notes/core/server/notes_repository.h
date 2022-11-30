// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_REPOSITORY_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_REPOSITORY_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/content_creation/notes/core/server/save_note_response.h"
#include "components/version_info/channel.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network
namespace signin {
class IdentityManager;
}  // namespace signin

namespace content_creation {

using PublishNoteCallback = base::OnceCallback<void(std::string)>;

struct NoteData;
class NotesServerSaver;

// Instance in charge of saving and publishing the notes to the server.
class NotesRepository {
 public:
  NotesRepository(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel);
  virtual ~NotesRepository();

  // Not copyable or movable.
  NotesRepository(const NotesRepository&) = delete;
  NotesRepository& operator=(const NotesRepository&) = delete;

  // Saves and publishes the |note| to the server. Will invoke |callback| with
  // results and URL to access the published note.
  virtual void PublishNote(const NoteData& note_data,
                           PublishNoteCallback callback);

  // Returns whether the publishing functionality is available.
  bool IsPublishAvailable() const;

 protected:
  // Used for tests.
  NotesRepository();

  virtual void OnNotePublished(PublishNoteCallback callback,
                               SaveNoteResponse save_response);

 private:
  raw_ptr<signin::IdentityManager> identity_manager_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<NotesServerSaver> notes_saver_;
  const version_info::Channel channel_;
  bool has_failed_publish_ = false;

  base::WeakPtrFactory<NotesRepository> weak_ptr_factory_{this};
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTES_REPOSITORY_H_