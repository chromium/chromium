// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_H_

#include "build/chromeos_buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// gn check complains on Linux Ozone.
#include "ash/public/cpp/ash_typography.h"  // nogncheck
#endif

enum ChromeTextContext {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHROME_TEXT_CONTEXT_START = ash::ASH_TEXT_CONTEXT_END,
#else
  CHROME_TEXT_CONTEXT_START = views::style::VIEWS_TEXT_CONTEXT_END,
#endif

  // Headline text. Usually 20pt. Never multi-line.
  CONTEXT_HEADLINE = CHROME_TEXT_CONTEXT_START,

  // Smaller version of CONTEXT_DIALOG_BODY_TEXT. Usually 12pt.
  CONTEXT_DIALOG_BODY_TEXT_SMALL,

  // Text of the page title in the tab hover card.
  CONTEXT_TAB_HOVER_CARD_TITLE,

  // Text of the number of tabs in the tab counter used in tablet mode.
  CONTEXT_TAB_COUNTER,

  // Used in `ManagePasswordsDetailsView` error messages. It likely has little
  // to no effect in terms of typography (font size, font weight, etc.).
  CONTEXT_DEEMPHASIZED,

  // Text used in the following UI contexts:
  //   - Omnibox query row text entry
  //   - Location icon view in the Omnibox
  //   - Omnibox pedals / action chips (omnibox_suggestion_button_row_view.cc)
  //
  // This context is also used in the following UI components, but likely has
  // little to no effect in terms of typography (font size, weight, etc.):
  //   - Custom tab bar used by PWAs (custom_tab_bar_view.cc)
  //   - Picture-in-Picture view (picture_in_picture_browser_frame_view.cc)
  //   - TabsharingInfoBar CSC indicator chip (tab_sharing_infobar.cc)
  CONTEXT_OMNIBOX_PRIMARY,

  // Primary text in the omnibox dropdown.
  CONTEXT_OMNIBOX_POPUP,

  // Text in the suggestions section header in the omnibox dropdown.
  CONTEXT_OMNIBOX_SECTION_HEADER,

  // Text for suggestion row chips; e.g. the history embeddings chip.
  CONTEXT_OMNIBOX_POPUP_ROW_CHIP,

  // ToolbarButton label
  CONTEXT_TOOLBAR_BUTTON,

  // Most text in the download shelf.  Usually 13pt.
  CONTEXT_DOWNLOAD_SHELF,

  // Status labels in the download shelf.  Usually 10pt.
  CONTEXT_DOWNLOAD_SHELF_STATUS,

  // Title label in the IPH bubble. Usually 18pt.
  CONTEXT_IPH_BUBBLE_TITLE,

  // Body text label in the IPH bubble. Usually 14pt.
  CONTEXT_IPH_BUBBLE_BODY,

  // Title label in the browser side panel. Usually 13pt.
  CONTEXT_SIDE_PANEL_TITLE,

  // Body label in the toast. Usually 13pt.
  CONTEXT_TOAST_BODY_TEXT,
};

enum ChromeTextStyle {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHROME_TEXT_STYLE_START = ash::ASH_TEXT_STYLE_END,
#else
  CHROME_TEXT_STYLE_START = views::style::VIEWS_TEXT_STYLE_END,
#endif

  // A solid shade of red.
  STYLE_RED = CHROME_TEXT_STYLE_START,

  // A solid shade of green.
  STYLE_GREEN,
};

// Takes a desired font size and returns the size delta to request from
// ui::ResourceBundle that will result either in that font size, or the biggest
// font size that is smaller than the desired font size but will fit inside
// |available_height|.
int GetFontSizeDeltaBoundedByAvailableHeight(int available_height,
                                             int desired_font_size);

// Sets the |details| for text that should not be affected by the Harmony spec.
void ApplyCommonFontStyles(int context,
                           int style,
                           ui::ResourceBundle::FontDetails& details);

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_TYPOGRAPHY_H_
