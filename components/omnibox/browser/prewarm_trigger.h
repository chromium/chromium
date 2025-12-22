// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_PREWARM_TRIGGER_H_
#define COMPONENTS_OMNIBOX_BROWSER_PREWARM_TRIGGER_H_

// This enum is used to mark the source of the prewarm trigger
enum class PrewarmTrigger {
  // Triggered when the user focuses on the omnibox on the New Tab Page.
  kZeroSuggest,

  // Triggered upon the first interaction with the omnibox.
  kUserInteraction,
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_PREWARM_TRIGGER_H_
