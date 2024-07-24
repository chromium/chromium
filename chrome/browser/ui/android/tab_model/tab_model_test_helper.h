// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_TEST_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_TEST_HELPER_H_

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "content/public/browser/web_contents.h"

// TestTabModel is a TabModel that can be used for testing Android tab behavior.
class TestTabModel : public TabModel {
 public:
  explicit TestTabModel(Profile* profile);
  ~TestTabModel() override;

  // TabModel:
  // Returns tab_count_ if not 0. Otherwise, returns size of web_contents_list_.
  int GetTabCount() const override;
  int GetActiveIndex() const override;
  content::WebContents* GetWebContentsAt(int index) const override;
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const override;
  void CreateTab(TabAndroid* parent,
                 content::WebContents* web_contents,
                 bool select) override;
  void HandlePopupNavigation(TabAndroid* parent,
                             NavigateParams* params) override;
  content::WebContents* CreateNewTabForDevTools(const GURL& url) override;
  bool IsSessionRestoreInProgress() const override;
  bool IsActiveModel() const override;
  TabAndroid* GetTabAt(int index) const override;
  void SetActiveIndex(int index) override;
  void CloseTabAt(int index) override;
  void AddObserver(TabModelObserver* observer) override;
  void RemoveObserver(TabModelObserver* observer) override;

  TabModelObserver* GetObserver();
  void SetTabCount(int tab_count);
  void SetWebContentsList(
      const std::vector<raw_ptr<content::WebContents>>& web_contents_list);
  int GetTabCountNavigatedInTimeWindow(
      const base::Time& begin_time,
      const base::Time& end_time) const override;
  void CloseTabsNavigatedInTimeWindow(const base::Time& begin_time,
                                      const base::Time& end_time) override;

 private:
  // A fake value for the current number of tabs.
  int tab_count_ = 0;

  raw_ptr<TabModelObserver> observer_ = nullptr;
  std::vector<raw_ptr<content::WebContents>> web_contents_list_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_TEST_HELPER_H_
