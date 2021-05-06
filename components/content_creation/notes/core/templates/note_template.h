// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_NOTE_TEMPLATE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_NOTE_TEMPLATE_H_

#include <string>

namespace content_creation {

// Contains the information required to be able to render a note.
class NoteTemplate {
 public:
  explicit NoteTemplate(const std::string& localized_name);

  const std::string localized_name() const { return localized_name_; }

 private:
  // Name of the template to be shown to the users.
  std::string localized_name_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_NOTE_TEMPLATE_H_
