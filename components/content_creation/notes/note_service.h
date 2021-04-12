// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_CONTENT_CREATION_NOTES_NOTE_SERVICE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_NOTE_SERVICE_H_

#include "components/content_creation/notes/notes_types.h"

namespace content_creation {

class NoteService {
 public:
  virtual ~NoteService() = default;

  virtual void GetTemplates(GetTemplatesCallback callback) = 0;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_NOTE_SERVICE_H_
