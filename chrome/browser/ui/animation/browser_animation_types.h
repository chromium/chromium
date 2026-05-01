// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_TYPES_H_
#define CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_TYPES_H_

#include "base/functional/callback_forward.h"
#include "ui/base/identifier/unique_identifier.h"

// Represents a group of animations, such as "Vertical Tabstrip Animations" or
// "Side Panel Animations".
DECLARE_UNIQUE_IDENTIFIER_TYPE(BrowserAnimationGroup);

// Represents a single motion within an animation group, such as "Expand" or
// "Collapse".
DECLARE_UNIQUE_IDENTIFIER_TYPE(BrowserAnimationMotion);

// Represents one or more animations that happen to a particular visual item,
// such as a UI element's size or opacity.
DECLARE_UNIQUE_IDENTIFIER_TYPE(BrowserAnimationSequence);

// Types used when registering for animation callbacks.
class BrowserAnimationController;
enum class BrowserAnimationUpdate { kStarted, kProgressed, kEnded, kCanceled };
using BrowserAnimationCallback =
    base::RepeatingCallback<void(const BrowserAnimationController*,
                                 BrowserAnimationUpdate)>;

// Convenience macros. You can use these for most cases where you want to
// declare animation elements.

#define DECLARE_BROWSER_ANIMATION_GROUP(Name) \
  DECLARE_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationGroup, Name)
#define DEFINE_BROWSER_ANIMATION_GROUP(Name) \
  DEFINE_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationGroup, Name)
#define DECLARE_CLASS_BROWSER_ANIMATION_GROUP(Name) \
  DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationGroup, Name)
#define DEFINE_CLASS_BROWSER_ANIMATION_GROUP(Class, Name) \
  DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(Class, BrowserAnimationGroup, Name)

#define DECLARE_BROWSER_ANIMATION_MOTION(Name) \
  DECLARE_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationMotion, Name)
#define DEFINE_BROWSER_ANIMATION_MOTION(Name) \
  DEFINE_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationMotion, Name)
#define DECLARE_CLASS_BROWSER_ANIMATION_MOTION(Name) \
  DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationMotion, Name)
#define DEFINE_CLASS_BROWSER_ANIMATION_MOTION(Class, Name) \
  DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(Class, BrowserAnimationMotion, Name)

#define DECLARE_BROWSER_ANIMATION_SEQUENCE(Name) \
  DECLARE_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationSequence, Name)
#define DEFINE_BROWSER_ANIMATION_SEQUENCE(Name) \
  DEFINE_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationSequence, Name)
#define DECLARE_CLASS_BROWSER_ANIMATION_SEQUENCE(Name) \
  DECLARE_CLASS_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationSequence, Name)
#define DEFINE_CLASS_BROWSER_ANIMATION_SEQUENCE(Class, Name) \
  DEFINE_CLASS_UNIQUE_IDENTIFIER_VALUE(Class, BrowserAnimationSequence, Name)

#endif  // CHROME_BROWSER_UI_ANIMATION_BROWSER_ANIMATION_TYPES_H_
