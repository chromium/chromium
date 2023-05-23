// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"

#include "base/logging.h"

#include "chrome/browser/ash/login/choobe_flow_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

ChoobeScreenHandler::ChoobeScreenHandler() : BaseScreenHandler(kScreenId) {}

ChoobeScreenHandler::~ChoobeScreenHandler() = default;

void ChoobeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("choobeScreenTitle", IDS_OOBE_CHOOBE_TITLE);
  builder->Add("choobeScreenDescription", IDS_OOBE_CHOOBE_DESCRIPTION);
  builder->Add("choobeScreenSkip", IDS_OOBE_CHOOBE_SKIP_BUTTON);
}

void ChoobeScreenHandler::Show(const std::vector<ScreenSummary>& screens) {
  base::Value::List screens_list;
  for (auto screen : screens) {
    base::Value::Dict screen_dict;
    screen_dict.Set("screenID", base::Value(screen.screen_id.name));
    screen_dict.Set("title", base::Value(screen.title_id));
    screen_dict.Set("icon", base::Value(screen.icon_id));
    screen_dict.Set("selected", false);
    screen_dict.Set("is_revisitable", screen.is_revisitable);
    screen_dict.Set("is_synced", screen.is_synced);
    screens_list.Append(std::move(screen_dict));
  }

  base::Value::Dict data;
  data.Set("screens", std::move(screens_list));

  ShowInWebUI(std::move(data));
}

}  // namespace ash
