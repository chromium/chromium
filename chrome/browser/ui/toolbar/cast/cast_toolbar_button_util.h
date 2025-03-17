// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_TOOLBAR_BUTTON_UTIL_H_
#define CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_TOOLBAR_BUTTON_UTIL_H_

namespace actions {
class ActionItem;
}
class Browser;

class CastToolbarButtonUtil {
 public:
  // Adds child actions to the provided |cast_action|. Child actions are
  // displayed as context menu options on the cast toolbar button.
  static void AddCastChildActions(actions::ActionItem* cast_action,
                                  Browser* browser);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_CAST_CAST_TOOLBAR_BUTTON_UTIL_H_
