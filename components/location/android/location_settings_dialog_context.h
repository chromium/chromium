// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_DIALOG_CONTEXT_H_
#define COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_DIALOG_CONTEXT_H_

// An enum to describe the context in which a system location setting prompt
// is triggered to allow the prompt UI to be customized to the given context.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.location
enum LocationSettingsDialogContext {
  // Default context.
  DEFAULT = 1,
  // Prompt triggered in the context of a search.
  SEARCH = 2,
};

#endif  // COMPONENTS_LOCATION_ANDROID_LOCATION_SETTINGS_DIALOG_CONTEXT_H_
