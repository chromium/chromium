// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_CONTENT_CREATION_NOTES_NOTES_TYPES_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_NOTES_TYPES_H_

#include <string>
#include <vector>

#include "base/callback.h"

namespace content_creation {

// Contains the information required to be able to render a note.
struct NoteTemplate {
  // Name of the template to be shown to the users.
  std::string localized_name;
};

using GetTemplatesCallback =
    base::OnceCallback<void(std::vector<NoteTemplate>)>;

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_NOTES_TYPES_H_
