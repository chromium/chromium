// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CATEGORY_STATUS_H_
#define COMPONENTS_NTP_SNIPPETS_CATEGORY_STATUS_H_

namespace ntp_snippets {

// Represents the status of a category of content suggestions.
// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum class CategoryStatus {
  // The provider is still initializing and it is not yet determined whether
  // content suggestions will be available or not.
  INITIALIZING,

  // Content suggestions are available (though the list of available suggestions
  // may be empty simply because there are no reasonable suggestions to be made
  // at the moment).
  AVAILABLE,
  // Content suggestions are provided but not yet loaded.
  AVAILABLE_LOADING,

  // There is no provider that provides suggestions for this category.
  NOT_PROVIDED,
  // The entire content suggestions feature has explicitly been disabled as part
  // of the service configuration.
  ALL_SUGGESTIONS_EXPLICITLY_DISABLED,
  // Content suggestions from a specific category have been disabled as part of
  // the service configuration. Any suggestions from this category should be
  // removed from the UI immediately.
  CATEGORY_EXPLICITLY_DISABLED,

  // Content suggestions are not available because an error occurred when
  // loading or updating them. Any suggestions from this category should be
  // removed from the UI immediately.
  LOADING_ERROR
};

// Determines whether the given status is one of the AVAILABLE statuses.
bool IsCategoryStatusAvailable(CategoryStatus status);

// Determines whether the given status is INITIALIZING or one of the AVAILABLE
// statuses. All of these statuses have in common that there is or will soon be
// content.
bool IsCategoryStatusInitOrAvailable(CategoryStatus status);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CATEGORY_STATUS_H_
