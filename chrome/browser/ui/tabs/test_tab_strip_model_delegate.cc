// Copyright 2012 The Chromium Authors
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
    std::optional<tab_groups::TabGroupId> group) {}

Browser* TestTabStripModelDelegate::CreateNewStripWithTabs(
    std::vector<NewStripContents> tabs,
    const gfx::Rect& window_bounds,
    bool maximize) {
  return nullptr;
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

bool TestTabStripModelDelegate::IsTabStripEditable() {
  return true;
}

void TestTabStripModelDelegate::DuplicateContentsAt(int index) {
}

void TestTabStripModelDelegate::MoveToExistingWindow(
    const std::vector<int>& indices,
    int browser_index) {}

bool TestTabStripModelDelegate::CanMoveTabsToWindow(
    const std::vector<int>& indices) {
  return false;
}

void TestTabStripModelDelegate::MoveTabsToNewWindow(
    const std::vector<int>& indices) {}

void TestTabStripModelDelegate::MoveGroupToNewWindow(
    const tab_groups::TabGroupId& group) {}

std::optional<SessionID> TestTabStripModelDelegate::CreateHistoricalTab(
    content::WebContents* contents) {
  return std::nullopt;
}

void TestTabStripModelDelegate::CreateHistoricalGroup(
    const tab_groups::TabGroupId& group) {}

void TestTabStripModelDelegate::GroupAdded(
    const tab_groups::TabGroupId& group) {}

void TestTabStripModelDelegate::WillCloseGroup(
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

bool TestTabStripModelDelegate::CanReload() const {
  return true;
}

void TestTabStripModelDelegate::AddToReadLater(
    content::WebContents* web_contents) {}

bool TestTabStripModelDelegate::SupportsReadLater() {
  return true;
}

bool TestTabStripModelDelegate::IsForWebApp() {
  return false;
}

void TestTabStripModelDelegate::CopyURL(content::WebContents* web_contents) {}

void TestTabStripModelDelegate::GoBack(content::WebContents* web_contents) {}

bool TestTabStripModelDelegate::CanGoBack(content::WebContents* web_contents) {
  return false;
}

bool TestTabStripModelDelegate::IsNormalWindow() {
  return true;
}

BrowserWindowInterface* TestTabStripModelDelegate::GetBrowserWindowInterface() {
  return nullptr;
}

void TestTabStripModelDelegate::OnGroupsDestruction(
    const std::vector<tab_groups::TabGroupId>& group_ids,
    base::OnceCallback<void()> callback,
    bool is_bulk_operation) {
  std::move(callback).Run();
}

void TestTabStripModelDelegate::OnRemovingAllTabsFromGroups(
    const std::vector<tab_groups::TabGroupId>& group_ids,
    base::OnceCallback<void()> callback) {
  std::move(callback).Run();
}
