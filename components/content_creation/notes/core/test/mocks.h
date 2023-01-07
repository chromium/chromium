// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEST_MOCKS_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEST_MOCKS_H_

#include "base/callback.h"
#include "components/content_creation/notes/core/server/note_data.h"
#include "components/content_creation/notes/core/server/notes_repository.h"
#include "components/content_creation/notes/core/templates/note_template.h"
#include "components/content_creation/notes/core/templates/template_store.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content_creation {
namespace test {

class MockTemplateStore : public TemplateStore {
 public:
  explicit MockTemplateStore();
  ~MockTemplateStore() override;

  MOCK_METHOD1(GetTemplates, void(GetTemplatesCallback));
};

class MockNotesRepository : public NotesRepository {
 public:
  explicit MockNotesRepository();
  ~MockNotesRepository() override;

  MOCK_METHOD2(PublishNote, void(const NoteData&, PublishNoteCallback));
};

}  // namespace test
}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEST_MOCKS_H_
