// Copyright 2021 The Chromium Authors
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

NoteTemplate::NoteTemplate(NoteTemplateIds id,
                           const std::string& localized_name,
                           const Background& main_background,
                           const Background& content_background,
                           const TextStyle& text_style,
                           const FooterStyle& footer_style)
    : id_(id),
      localized_name_(localized_name),
      main_background_(main_background),
      content_background_(content_background),
      text_style_(text_style),
      footer_style_(footer_style) {}

NoteTemplate::NoteTemplate(const proto::NoteTemplate& note_template)
    : id_(static_cast<NoteTemplateIds>(note_template.id())),
      // TODO(graysonlafleur): remove localized_name UI element.
      localized_name_(""),
      main_background_(Background::Init(note_template.mainbackground())),
      text_style_(note_template.textstyle()),
      footer_style_(note_template.footerstyle()) {
  if (note_template.has_contentbackground()) {
    content_background_ = Background::Init(note_template.contentbackground());
  }
}

NoteTemplate::NoteTemplate(const NoteTemplate& other)
    : id_(other.id()),
      localized_name_(other.localized_name()),
      main_background_(other.main_background()),
      text_style_(other.text_style()),
      footer_style_(other.footer_style()) {
  if (other.content_background()) {
    content_background_ = *other.content_background();
  }
}

NoteTemplate::~NoteTemplate() = default;

}  // namespace content_creation
