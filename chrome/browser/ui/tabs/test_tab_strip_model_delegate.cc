// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"

#include <vector>

#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "components/tab_groups/tab_group_id.h"

TestTabStripModelDelegate::TestTabStripModelDelegate() = default;

TestTabStripModelDelegate::~TestTabStripModelDelegate() = default;

void TestTabStripModelDelegate::AddTabAt(
    const GURL& url,
    int index,
    bool foreground,
    base::Optional<tab_groups::TabGroupId> group) {}

Browser* TestTabStripModelDelegate::CreateNewStripWithContents(
    std::vector<NewStripContents> contentses,
    const gfx::Rect& window_bounds,
    bool maximize) {
  return NULL;
}

void TestTabStripModelDelegate::WillAddWebContents(
    content::WebContents* contents) {
  // Required to determine reloadability of tabs.
  CoreTabHelper::CreateForWebContents(contents);
  // Required to determine if tabs are app tabs.
  extensions::TabHelper::CreateForWebContents(contents);
}

int TestTabStripModelDelegate::GetDragActions() const {
  return 0;
}

bool TestTabStripModelDelegate::CanDuplicateContentsAt(int index) {
  return false;
}

bool TestTabStripModelDelegate::CanHighlightTabs() {
  return true;
}

void TestTabStripModelDelegate::DuplicateContentsAt(int index) {
}

void TestTabStripModelDelegate::MoveToExistingWindow(
    const std::vector<int>& indices,
    int browser_index) {}

std::vector<std::u16string>
TestTabStripModelDelegate::GetExistingWindowsForMoveMenu() {
  std::vector<std::u16string> existing_windows;
  return existing_windows;
}

bool TestTabStripModelDelegate::CanMoveTabsToWindow(
    const std::vector<int>& indices) {
  return false;
}

void TestTabStripModelDelegate::MoveTabsToNewWindow(
    const std::vector<int>& indices) {}

void TestTabStripModelDelegate::MoveGroupToNewWindow(
    const tab_groups::TabGroupId& group) {}

base::Optional<SessionID> TestTabStripModelDelegate::CreateHistoricalTab(
    content::WebContents* contents) {
  return base::nullopt;
}

void TestTabStripModelDelegate::CreateHistoricalGroup(
    const tab_groups::TabGroupId& group) {}

void TestTabStripModelDelegate::GroupCloseStopped(
    const tab_groups::TabGroupId& group) {}

bool TestTabStripModelDelegate::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return false;
}

bool TestTabStripModelDelegate::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return false;
}

bool TestTabStripModelDelegate::ShouldDisplayFavicon(
    content::WebContents* web_contents) const {
  return true;
}
