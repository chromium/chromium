// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_TYPE_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_TYPE_H_

#include <string>

#include "url/origin.h"

// Exclusive access bubble types that inform UI content for various states.
// More comments about tab and browser fullscreen mode can be found in
// chrome/browser/ui/exclusive_access/fullscreen_controller.h.
enum ExclusiveAccessBubbleType {
  // This "type" typically signifies closing the exclusive access bubble, except
  // when `has_download` is true, in which a "download started" notice is added
  // to whatever else the exclusive access bubble type would have been.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE = 0,

  // For tab-initiated fullscreen mode.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,

  // For pointer lock, while in fullscreen mode.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION,

  // For pointer lock, while outside fullscreen mode.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION,

  // For keyboard lock; fullscreen mode is required and implied.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,

  // For browser-initiated fullscreen mode.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,

  // For extension-initiated fullscreen mode.
  EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION,
};

// Describes contents and traits of the exclusive access bubble.
struct ExclusiveAccessBubbleParams {
  // The origin with exclusive access; empty for browser or extension
  // fullscreen.
  url::Origin origin;
  // The type of bubble to show, which directly informs the text content.
  // Note: *_NONE and `has_download` means the current type should be kept.
  // TODO(msw): Use std::optional and nullopt to signify no type change.
  ExclusiveAccessBubbleType type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
  // Whether the bubble should also mention a new download.
  bool has_download = false;
  // Whether to show the bubble regardless of content changes.
  bool force_update = false;
};

namespace exclusive_access_bubble {

// Gets the text shown in the exclusive access bubble, including the origin and
// the instruction text.
// i.e. "<origin.com> - To exit fullscreen, press [Esc]".
//   |type|: the type of exclusive access mode that leads to the bubble being
//   showed.
//   |accelerator|: the keyboard shortcut used to exit the exclusive
//   access mode.
//   |origin|: the origin of the site requesting Exclusive Access.
//   |has_download|: True if download in progress, which affects the text
//   content.
//   |notify_overridden|: True if the bubble is showing and its text needs to be
//   overridden. Valid when |has_download| is True.
std::u16string GetInstructionTextForType(ExclusiveAccessBubbleType type,
                                         const std::u16string& accelerator,
                                         const url::Origin& origin,
                                         bool has_download,
                                         bool notify_overridden);

// Helpers to categorize different types of ExclusiveAccessBubbleType.
bool IsExclusiveAccessModeBrowserFullscreen(ExclusiveAccessBubbleType type);

}  // namespace exclusive_access_bubble

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_TYPE_H_
