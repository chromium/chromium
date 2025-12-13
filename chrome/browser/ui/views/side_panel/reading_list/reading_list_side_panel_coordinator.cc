// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/reading_list/reading_list_side_panel_coordinator.h"

#include <memory>

#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/reading_list/read_later_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"

namespace {
std::unique_ptr<views::View> CreateReadingListWebView(
    Profile* profile,
    TabStripModel* tab_strip_model,
    SidePanelEntryScope& scope) {
  return std::make_unique<ReadLaterSidePanelWebView>(
      profile, tab_strip_model, scope, base::RepeatingClosure());
}
}  // namespace

ReadingListSidePanelCoordinator::ReadingListSidePanelCoordinator(
    Profile* profile,
    TabStripModel* tab_strip_model)
    : profile_(CHECK_DEREF(profile)),
      tab_strip_model_(CHECK_DEREF(tab_strip_model)) {}

ReadingListSidePanelCoordinator::~ReadingListSidePanelCoordinator() = default;

void ReadingListSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Key(SidePanelEntry::Id::kReadingList),
      base::BindRepeating(&CreateReadingListWebView, &profile_.get(),
                          &tab_strip_model_.get()),
      /*default_content_width_callback=*/base::NullCallback()));
}
