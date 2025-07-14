// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_FOOTER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_FOOTER_CONTEXT_MENU_H_

#include "base/memory/raw_ptr.h"
#include "ui/menus/simple_menu_model.h"

namespace new_tab_footer {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class FooterContextMenuItem {
  kHideFooter = 0,
  kMaxValue = kHideFooter,
};
}  // namespace new_tab_footer

class Profile;

// The context menu for the New Tab Page footer.
class FooterContextMenu : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHideFooterIdForTesting);

  explicit FooterContextMenu(Profile* profile);
  ~FooterContextMenu() override;

  FooterContextMenu(const FooterContextMenu&) = delete;
  FooterContextMenu& operator=(const FooterContextMenu&) = delete;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  enum CommandID { COMMAND_CLOSE_FOOTER };

  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_FOOTER_FOOTER_CONTEXT_MENU_H_
