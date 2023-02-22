// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_USER_NOTES_PREFS_H_
#define COMPONENTS_USER_NOTES_USER_NOTES_PREFS_H_

class PrefRegistrySimple;

namespace prefs {

// Boolean that stores whether user notes should be sorted by newest and is used
// for the sort order to display user's notes.
extern const char kUserNotesSortByNewest[];

}  // namespace prefs

namespace user_notes {

// Registers user preferences related to user notes.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_USER_NOTES_PREFS_H_