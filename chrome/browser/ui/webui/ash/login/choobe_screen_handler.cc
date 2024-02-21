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
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

ChoobeScreenHandler::ChoobeScreenHandler() : BaseScreenHandler(kScreenId) {}

ChoobeScreenHandler::~ChoobeScreenHandler() = default;

void ChoobeScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("choobeScreenTitle", IDS_OOBE_CHOOBE_TITLE);
  builder->AddF("choobeScreenDescription", IDS_OOBE_CHOOBE_DESCRIPTION,
                ui::GetChromeOSDeviceTypeResourceId());
  builder->Add("choobeScreenSkip", IDS_OOBE_CHOOBE_SKIP_BUTTON);
  builder->Add("choobeReturnButton", IDS_OOBE_CHOOBE_RETURN_BUTTON);

  // Accessibility Message.
  builder->Add("choobeSyncedTile", IDS_OOBE_CHOOBE_TILE_SYNCED);
  builder->Add("choobeVisitedTile", IDS_OOBE_CHOOBE_TILE_VISITED);
}

void ChoobeScreenHandler::Show(const std::vector<ScreenSummary>& screens) {
  base::Value::List screens_list;
  for (auto screen : screens) {
    base::Value::Dict screen_dict;
    screen_dict.Set("screenId", base::Value(screen.screen_id.name));
    screen_dict.Set("title", base::Value(screen.title_id));
    if (screen.subtitle_resource.has_value()) {
      screen_dict.Set("subtitle",
                      base::Value(screen.subtitle_resource.value()));
    }
    screen_dict.Set("icon", base::Value(screen.icon_id));
    screen_dict.Set("selected", false);
    screen_dict.Set("isRevisitable", screen.is_revisitable);
    screen_dict.Set("isSynced", screen.is_synced);
    screen_dict.Set("isCompleted", screen.is_completed.value_or(false));

    screens_list.Append(std::move(screen_dict));
  }

  base::Value::Dict data;
  data.Set("screens", std::move(screens_list));

  ShowInWebUI(std::move(data));
}

base::WeakPtr<ChoobeScreenView> ChoobeScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
