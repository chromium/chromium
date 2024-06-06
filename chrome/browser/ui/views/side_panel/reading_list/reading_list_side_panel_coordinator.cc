// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"

#include <memory>

#include "base/functional/callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_later_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/vector_icons.h"

ReadingListSidePanelCoordinator::ReadingListSidePanelCoordinator(
    Browser* browser)
    : BrowserUserData<ReadingListSidePanelCoordinator>(*browser) {}

ReadingListSidePanelCoordinator::~ReadingListSidePanelCoordinator() = default;

void ReadingListSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadingList,
      base::BindRepeating(
          &ReadingListSidePanelCoordinator::CreateReadingListWebView,
          base::Unretained(this))));
}

std::unique_ptr<views::View>
ReadingListSidePanelCoordinator::CreateReadingListWebView() {
  return std::make_unique<ReadLaterSidePanelWebView>(&GetBrowser(),
                                                     base::RepeatingClosure());
}

BROWSER_USER_DATA_KEY_IMPL(ReadingListSidePanelCoordinator);
