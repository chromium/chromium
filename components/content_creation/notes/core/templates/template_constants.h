// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_CONSTANTS_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_CONSTANTS_H_

#include "components/content_creation/notes/core/templates/note_template.h"

namespace content_creation {

// Returns a NoteTemplate with the Classic style.
NoteTemplate GetClassicTemplate();

// Returns a NoteTemplate with the Friendly style.
NoteTemplate GetFriendlyTemplate();

// Returns a NoteTemplate with the Fresh style.
NoteTemplate GetFreshTemplate();

// Returns a NoteTemplate with the Powerful style.
NoteTemplate GetPowerfulTemplate();

// Returns a NoteTemplate with the Impactful style.
NoteTemplate GetImpactfulTemplate();

// Returns a NoteTemplate with the Lovely style.
NoteTemplate GetLovelyTemplate();

// Returns a NoteTemplate with the Groovy style.
NoteTemplate GetGroovyTemplate();

// Returns a NoteTemplate with the Monochrome style.
NoteTemplate GetMonochromeTemplate();

// Returns a NoteTemplate with the Bold style.
NoteTemplate GetBoldTemplate();

// Returns a NoteTemplate with the Dreamy style.
NoteTemplate GetDreamyTemplate();

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_TEMPLATES_TEMPLATE_CONSTANTS_H_
