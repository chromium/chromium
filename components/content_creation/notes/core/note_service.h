// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_SERVICE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_SERVICE_H_

#include <memory>

#include "base/supports_user_data.h"
#include "components/content_creation/notes/core/server/note_data.h"
#include "components/content_creation/notes/core/server/save_note_response.h"
#include "components/content_creation/notes/core/templates/template_store.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content_creation {

using PublishNoteCallback = base::OnceCallback<void(std::string)>;

class NotesRepository;

// Keyed service to be used by user-facing surfaces to retrieve templating
// information for generating stylized notes.
class NoteService : public KeyedService, public base::SupportsUserData {
 public:
  explicit NoteService(std::unique_ptr<TemplateStore> template_store,
                       std::unique_ptr<NotesRepository> notes_repository);
  ~NoteService() override;

  // Not copyable or movable.
  NoteService(const NoteService&) = delete;
  NoteService& operator=(const NoteService&) = delete;

  // Gets the set of templates to be used for generating stylized notes. Will
  // invoke |callback| with the results.
  void GetTemplates(GetTemplatesCallback callback);

  // Whether the Publish functionality is available.
  bool IsPublishAvailable();

  // Saves and publishes the |note| to the server. Will invoke |callback| with
  // results and URL to access the published note.
  void PublishNote(const NoteData& note_data, PublishNoteCallback callback);

 private:
  std::unique_ptr<TemplateStore> template_store_;

  std::unique_ptr<NotesRepository> notes_repository_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_SERVICE_H_