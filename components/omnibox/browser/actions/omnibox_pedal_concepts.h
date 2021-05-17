// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is tool-generated using pedal_processor.  Do not edit.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_

// This value is generated during Pedal concept data processing, and written
// to all data files as well as the source code here to ensure synchrony.
// The runtime loaded data must match this version exactly or it won't load.
constexpr int OMNIBOX_PEDAL_CONCEPTS_DATA_VERSION = 15865521;

// Unique identifiers for Pedals, used to bind loaded data to implementations.
// Also used in the Omnibox.SuggestionUsed.Pedal histogram. Do not remove or
// reuse values. If any pedal types are removed from Chrome, the associated ID
// will remain and be marked as obsolete.
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

  // Last value, used to track the upper bounds when recording type histograms.
  // This intentionally does not have an assigned value to ensure that it's
  // always 1 greater than the last assigned value.
  TOTAL_COUNT
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_CONCEPTS_H_
