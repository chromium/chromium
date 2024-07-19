// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_EDITOR_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_EDITOR_TYPES_H_

#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"

namespace chromeos::editor_menu {

enum EditorMode {
  kHardBlocked,
  kSoftBlocked,
  kWrite,
  kRewrite,
  kPromoCard,
};

struct EditorContext {
  EditorContext(EditorMode mode,
                bool consent_status_settled,
                PresetTextQueries preset_queries);
  EditorContext(const EditorContext&);
  ~EditorContext();

  EditorMode mode;

  // indicating whether the editor consent status is already determined or still
  // unset.
  bool consent_status_settled;
  PresetTextQueries preset_queries;
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_UTILS_EDITOR_TYPES_H_
