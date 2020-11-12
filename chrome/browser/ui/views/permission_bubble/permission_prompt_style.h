// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_STYLE_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_STYLE_H_

// Represents permission prompt UI styles on desktop.
enum class PermissionPromptStyle {
  // The permission prompt bubble is shown directly.
  kBubbleOnly,
  // The permission chip view in the location bar.
  kChip,
  // The prompt as an indicator in the right side of the omnibox.
  kQuiet
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_PERMISSION_PROMPT_STYLE_H_
