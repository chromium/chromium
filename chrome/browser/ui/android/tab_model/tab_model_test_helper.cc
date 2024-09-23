// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"

#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "content/public/browser/web_contents.h"

TestTabModel::TestTabModel(Profile* profile)
    : TabModel(profile, chrome::android::ActivityType::kCustomTab) {}

TestTabModel::~TestTabModel() = default;

int TestTabModel::GetTabCount() const {
  return tab_count_ != 0 ? tab_count_
                         : static_cast<int>(web_contents_list_.size());
}

int TestTabModel::GetActiveIndex() const {
  return 0;
}

content::WebContents* TestTabModel::GetWebContentsAt(int index) const {
  if (index < static_cast<int>(web_contents_list_.size())) {
    return web_contents_list_[index];
  }
  return nullptr;
}

base::android::ScopedJavaLocalRef<jobject> TestTabModel::GetJavaObject() const {
  return nullptr;
}

void TestTabModel::CreateTab(TabAndroid* parent,
                             content::WebContents* web_contents,
                             bool select) {}

void TestTabModel::HandlePopupNavigation(TabAndroid* parent,
                                         NavigateParams* params) {}

content::WebContents* TestTabModel::CreateNewTabForDevTools(const GURL& url) {
  return nullptr;
}

bool TestTabModel::IsSessionRestoreInProgress() const {
  return false;
}

bool TestTabModel::IsActiveModel() const {
  return false;
}

TabAndroid* TestTabModel::GetTabAt(int index) const {
  return nullptr;
}

void TestTabModel::SetActiveIndex(int index) {}

void TestTabModel::CloseTabAt(int index) {}

void TestTabModel::AddObserver(TabModelObserver* observer) {
  observer_ = observer;
}

void TestTabModel::RemoveObserver(TabModelObserver* observer) {
  if (observer == observer_) {
    observer_ = nullptr;
  }
}

TabModelObserver* TestTabModel::GetObserver() {
  return observer_;
}

void TestTabModel::SetTabCount(int tab_count) {
  tab_count_ = tab_count;
}

void TestTabModel::SetWebContentsList(
    const std::vector<raw_ptr<content::WebContents>>& web_contents_list) {
  web_contents_list_ = web_contents_list;
}

int TestTabModel::GetTabCountNavigatedInTimeWindow(
    const base::Time& begin_time,
    const base::Time& end_time) const {
  return 0;
}

void TestTabModel::CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                                  const base::Time& end_time) {}
