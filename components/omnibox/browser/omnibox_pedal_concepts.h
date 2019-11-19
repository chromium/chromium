// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is tool-generated using pedal_processor.  Do not edit.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_CONCEPTS_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_CONCEPTS_H_

// The runtime loaded data must match this version exactly.
constexpr int OMNIBOX_PEDAL_CONCEPTS_DATA_VERSION = 14776860;

// Unique identifiers for Pedals, used to bind loaded data to implementations.
enum class OmniboxPedalId {
  NONE = 0,

  CLEAR_BROWSING_DATA = 1,
  CHANGE_SEARCH_ENGINE = 2,
  MANAGE_PASSWORDS = 3,
  CHANGE_HOME_PAGE = 4,
  UPDATE_CREDIT_CARD = 5,
  LAUNCH_INCOGNITO = 6,
  TRANSLATE = 7,
  UPDATE_CHROME = 8,
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_PEDAL_CONCEPTS_H_
