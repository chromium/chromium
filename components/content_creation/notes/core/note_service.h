// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_SERVICE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_SERVICE_H_

#include "base/supports_user_data.h"
#include "components/content_creation/notes/core/notes_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content_creation {

// Keyed service to be used by user-facing surfaces to retrieve templating
// information for generating stylized notes.
class NoteService : public KeyedService, public base::SupportsUserData {
 public:
  explicit NoteService();
  ~NoteService() override;

  // Gets the set of templates to be used for generating stylized notes. Will
  // invoke |callback| with the results.
  void GetTemplates(GetTemplatesCallback callback);
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_SERVICE_H_