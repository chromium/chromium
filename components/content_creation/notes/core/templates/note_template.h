// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_NOTE_TEMPLATE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_NOTE_TEMPLATE_H_

#include <string>

#include "components/content_creation/notes/core/templates/template_storage.pb.h"
#include "components/content_creation/notes/core/templates/template_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content_creation {

// Contains the information required to be able to render a note.
class NoteTemplate {
 public:
  // Constructor for a basic NoteTemplate.
  explicit NoteTemplate(NoteTemplateIds id,
                        const std::string& localized_name,
                        const Background& main_background,
                        const TextStyle& text_style,
                        const FooterStyle& footer_style);

  // Constructor for a NoteTemplate with a content background.
  explicit NoteTemplate(NoteTemplateIds id,
                        const std::string& localized_name,
                        const Background& main_background,
                        const Background& content_background,
                        const TextStyle& text_style,
                        const FooterStyle& footer_style);

  // Created a NoteTemplate object based on a protobuf NoteTemplate object.
  explicit NoteTemplate(const proto::NoteTemplate& note_template);

  NoteTemplate(const NoteTemplate& other);

  ~NoteTemplate();

  NoteTemplateIds id() const { return id_; }
  const std::string& localized_name() const { return localized_name_; }
  const Background& main_background() const { return main_background_; }
  const Background* content_background() const {
    return content_background_.has_value() ? &content_background_.value()
                                           : nullptr;
  }
  const TextStyle& text_style() const { return text_style_; }
  const FooterStyle& footer_style() const { return footer_style_; }

 private:
  // The template's identifier.
  NoteTemplateIds id_;

  // Name of the template to be shown to the users.
  std::string localized_name_;

  // Styling of the main background.
  Background main_background_;

  // Styling of the content's background, drawn on top of the main background
  // but behind the main text.
  absl::optional<Background> content_background_;

  // Styling of the main text.
  TextStyle text_style_;

  // Styling of the footer part.
  FooterStyle footer_style_;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_NOTE_TEMPLATE_H_
