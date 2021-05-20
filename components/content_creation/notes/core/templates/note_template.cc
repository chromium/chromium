// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/note_template.h"

namespace content_creation {

NoteTemplate::NoteTemplate(NoteTemplateIds id,
                           const std::string& localized_name,
                           const Background& main_background,
                           const TextStyle& text_style,
                           const FooterStyle& footer_style)
    : id_(id),
      localized_name_(localized_name),
      main_background_(main_background),
      text_style_(text_style),
      footer_style_(footer_style) {}

NoteTemplate::NoteTemplate(const NoteTemplate& other)
    : id_(other.id()),
      localized_name_(other.localized_name()),
      main_background_(other.main_background()),
      text_style_(other.text_style()),
      footer_style_(other.footer_style()) {}

}  // namespace content_creation
