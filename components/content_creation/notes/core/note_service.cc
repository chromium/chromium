// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/note_service.h"

#include "base/functional/callback.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/notes/core/server/notes_repository.h"

namespace content_creation {

NoteService::NoteService(std::unique_ptr<TemplateStore> template_store,
                         std::unique_ptr<NotesRepository> notes_repository)
    : template_store_(std::move(template_store)),
      notes_repository_(std::move(notes_repository)) {}

NoteService::~NoteService() = default;

void NoteService::GetTemplates(GetTemplatesCallback callback) {
  DCHECK(IsStylizeEnabled());
  template_store_->GetTemplates(std::move(callback));
}

bool NoteService::IsPublishAvailable() {
  return notes_repository_->IsPublishAvailable();
}

void NoteService::PublishNote(const NoteData& note_data,
                              PublishNoteCallback callback) {
  notes_repository_->PublishNote(note_data, std::move(callback));
}

}  // namespace content_creation