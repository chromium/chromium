// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_H_

#include "base/macros.h"
#include "ui/gfx/font.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

#if defined(OS_CHROMEOS)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/ash_typography.h"  // nogncheck
#endif

enum ChromeTextContext {
#if defined(OS_CHROMEOS)
  CHROME_TEXT_CONTEXT_START = ash::ASH_TEXT_CONTEXT_END,
#else
  CHROME_TEXT_CONTEXT_START = views::style::VIEWS_TEXT_CONTEXT_END,
#endif

  // Headline text. Usually 20pt. Never multi-line.
  CONTEXT_HEADLINE = CHROME_TEXT_CONTEXT_START,

  // "Body 1". Usually 13pt.
  CONTEXT_BODY_TEXT_LARGE,

  // "Body 2". Usually 12pt.
  CONTEXT_BODY_TEXT_SMALL,

  // Text of the page title in the tab hover card.
  CONTEXT_TAB_HOVER_CARD_TITLE,

  // Text in the location bar entry, and primary text in the omnibox dropdown.
  CONTEXT_OMNIBOX_PRIMARY,

  // Text that goes inside location bar decorations such as the keyword hint.
  CONTEXT_OMNIBOX_DECORATION,

  // Text in omnibox answer results that is slightly smaller than primary font.
  CONTEXT_OMNIBOX_DEEMPHASIZED,

  // Text for titles, body text and buttons that appear in dialogs attempting to
  // mimic the native Windows 10 look and feel.
  CONTEXT_WINDOWS10_NATIVE,

  // ToolbarButton label
  CONTEXT_TOOLBAR_BUTTON,
};

enum ChromeTextStyle {
  CHROME_TEXT_STYLE_START = views::style::VIEWS_TEXT_STYLE_END,

  // Similar to views::style::STYLE_PRIMARY but with a monospaced typeface.
  STYLE_PRIMARY_MONOSPACED = CHROME_TEXT_STYLE_START,

  // Similar to views::style::STYLE_SECONDARY but with a monospaced typeface.
  STYLE_SECONDARY_MONOSPACED,

  // "Hint" text, usually a line that gives context to something more important.
  STYLE_HINT,

  // A solid shade of red.
  STYLE_RED,

  // A solid shade of green.
  STYLE_GREEN,

  // Used to draw attention to a section of body text such as an extension name
  // or hostname.
  STYLE_EMPHASIZED,

  // Emphasized secondary style. Like STYLE_EMPHASIZED but styled to match
  // surrounding STYLE_SECONDARY text.
  STYLE_EMPHASIZED_SECONDARY,
};

// Takes a desired font size and returns the size delta to request from
// ui::ResourceBundle that will result either in that font size, or the biggest
// font size that is smaller than the desired font size but will fit inside
// |available_height|.
int GetFontSizeDeltaBoundedByAvailableHeight(int available_height,
                                             int desired_font_size);

// Sets the |size_delta| and |font_weight| for text that should not be affected
// by the Harmony spec.
void ApplyCommonFontStyles(int context,
                           int style,
                           int* size_delta,
                           gfx::Font::Weight* weight);

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_H_
