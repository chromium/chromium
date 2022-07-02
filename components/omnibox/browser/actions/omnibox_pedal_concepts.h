// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//       _____                           _                 _  _  _
//      |  __ \                         | |               | |(_)| |
//      | |  | |  ___      _ __    ___  | |_      ___   __| | _ | |_
//      | |  | | / _ \    | '_ \  / _ \ | __|    / _ \ / _` || || __|
//      | |__| || (_) |   | | | || (_) || |_    |  __/| (_| || || |_  _
//      |_____/  \___/    |_| |_| \___/  \__|    \___| \__,_||_| \__|(_)
// DO NOT EDIT. This file is tool-generated using pedal_processor.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_

// This value is generated during Pedal concept data processing, and written
// to all data files as well as the source code here to ensure synchrony.
// The runtime loaded data must match this version exactly or it won't load.
constexpr int OMNIBOX_PEDAL_CONCEPTS_DATA_VERSION = 16391116;

// Unique identifiers for Pedals, used to bind loaded data to implementations.
// Also used in the Omnibox.SuggestionUsed.Pedal histogram. Do not remove or
// reuse values. If any pedal types are removed from Chrome, the associated ID
// will remain and be marked as obsolete.
//
// Automatically generate a corresponding Java enum:
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.omnibox.action
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: OmniboxPedalType
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
  // DO NOT EDIT. See comment at top.

  // Last value, used to track the upper bounds when recording type histograms.
  // This intentionally does not have an assigned value to ensure that it's
  // always 1 greater than the last assigned value.
  TOTAL_COUNT
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_
