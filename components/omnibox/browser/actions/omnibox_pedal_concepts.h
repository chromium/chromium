// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_

// Unique identifiers for Pedals, used in the Omnibox.SuggestionUsed.Pedal
// histograms. Do not remove or reuse values. If any pedal types are removed
// from Chrome, the associated ID will remain and be marked as obsolete.
//
// Automatically generate a corresponding Java enum:
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.omnibox.action
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: OmniboxPedalId
enum class OmniboxPedalId {
  NONE = 0,

  CLEAR_BROWSING_DATA = 1,
  MANAGE_PASSWORDS = 2,
  UPDATE_CREDIT_CARD = 3,
  LAUNCH_INCOGNITO = 4,
  TRANSLATE = 5,
  UPDATE_CHROME = 6,
  RUN_CHROME_SAFETY_CHECK = 7,
  MANAGE_SECURITY_SETTINGS = 8,
  MANAGE_COOKIES = 9,
  MANAGE_ADDRESSES = 10,
  MANAGE_SYNC = 11,
  MANAGE_SITE_SETTINGS = 12,
  CREATE_GOOGLE_DOC = 13,
  CREATE_GOOGLE_SHEET = 14,
  CREATE_GOOGLE_SLIDE = 15,
  CREATE_GOOGLE_CALENDAR_EVENT = 16,
  CREATE_GOOGLE_SITE = 17,
  CREATE_GOOGLE_KEEP_NOTE = 18,
  CREATE_GOOGLE_FORM = 19,
  COMPOSE_EMAIL_IN_GMAIL = 20,
  SEE_CHROME_TIPS = 21,
  MANAGE_GOOGLE_ACCOUNT = 22,
  CLEAR_YOUTUBE_HISTORY = 23,
  CHANGE_GOOGLE_PASSWORD = 24,
  INCOGNITO_CLEAR_BROWSING_DATA = 25,
  CLOSE_INCOGNITO_WINDOWS = 26,
  PLAY_CHROME_DINO_GAME = 27,
  FIND_MY_PHONE = 28,
  MANAGE_GOOGLE_PRIVACY = 29,
  MANAGE_GOOGLE_AD_SETTINGS = 30,
  MANAGE_CHROME_SETTINGS = 31,
  MANAGE_CHROME_DOWNLOADS = 32,
  VIEW_CHROME_HISTORY = 33,
  SHARE_THIS_PAGE = 34,
  MANAGE_CHROME_ACCESSIBILITY = 35,
  CUSTOMIZE_CHROME_FONTS = 36,
  MANAGE_CHROME_THEMES = 37,
  CUSTOMIZE_SEARCH_ENGINES = 38,
  MANAGE_CHROMEOS_ACCESSIBILITY = 39,
  SET_CHROME_AS_DEFAULT_BROWSER = 40,

  // Last value, used to track the upper bounds when recording type histograms.
  // This intentionally does not have an assigned value to ensure that it's
  // always 1 greater than the last assigned value.
  TOTAL_COUNT
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_
