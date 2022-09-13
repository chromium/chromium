// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_FEATURES_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace content_creation {

// Main feature for the Web Notes Stylize project.
extern const base::Feature kWebNotesStylizeEnabled;

// Feature parameter for Web Notes Stylize which controls whether the ordering
// of templates is randomized for each client or not.
extern const base::FeatureParam<bool> kRandomizeOrderParam;

// Feature for publishing Web Notes to the server.
extern const base::Feature kWebNotesPublish;

// Feature for enabling dynamic templates.
extern const base::Feature kWebNotesDynamicTemplates;

// Returns true if the Web Notes Stylize feature is enabled.
bool IsStylizeEnabled();

// Returns true if the order randomization parameter is set to true for the Web
// Notes Stylize feature.
bool IsRandomizeOrderEnabled();

// Returns whether the Web Notes Publish feature is enabled.
bool IsPublishEnabled();

// Returns whether the Dynamic Templates feature is enabled.
bool IsDynamicTemplatesEnabled();

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_NOTE_FEATURES_H_