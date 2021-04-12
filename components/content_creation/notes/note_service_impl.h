// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_NOTE_SERVICE_IMPL_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_NOTE_SERVICE_IMPL_H_

#include "components/content_creation/notes/note_service.h"
#include "components/content_creation/notes/notes_types.h"

namespace content_creation {

class NoteServiceImpl : public NoteService {
 public:
  NoteServiceImpl();
  ~NoteServiceImpl() override;

  void GetTemplates(GetTemplatesCallback callback) override;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_NOTE_SERVICE_IMPL_H_