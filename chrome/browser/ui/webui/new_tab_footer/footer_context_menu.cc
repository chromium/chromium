// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(FooterContextMenu,
                                      kHideFooterIdForTesting);

namespace new_tab_footer {
void RecordContextMenuClick(FooterContextMenuItem item) {
  base::UmaHistogramEnumeration("NewTabPage.Footer.ContextMenuClicked", item);
}
}  // namespace new_tab_footer

FooterContextMenu::FooterContextMenu(Profile* profile)
    : ui::SimpleMenuModel(this), profile_(profile) {
  const int icon_size = 16;
  AddItemWithIcon(
      COMMAND_CLOSE_FOOTER,
      // TODO(crbug.com/424872616): Change string.
      l10n_util::GetStringFUTF16(
          IDS_NTP_MODULES_DISMISS_BUTTON_TEXT,
          l10n_util::GetStringUTF16(IDS_NEW_TAB_FOOTER_NAME)),
      ui::ImageModel::FromVectorIcon(vector_icons::kVisibilityOffIcon,
                                     ui::kColorIcon, icon_size));
  SetElementIdentifierAt(GetIndexOfCommandId(COMMAND_CLOSE_FOOTER).value(),
                         kHideFooterIdForTesting);
}

FooterContextMenu::~FooterContextMenu() = default;

void FooterContextMenu::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case COMMAND_CLOSE_FOOTER: {
      new_tab_footer::RecordContextMenuClick(
          new_tab_footer::FooterContextMenuItem::kHideFooter);
      profile_->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, false);
      break;
    }
  }
}
