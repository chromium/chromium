// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/overscroll_pref_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

OverscrollPrefManager::OverscrollPrefManager(Browser* browser)
    : browser_(browser) {
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    overscroll_history_navigation_enabled_ =
        local_state->GetBoolean(prefs::kOverscrollHistoryNavigationEnabled);
    local_state_pref_registrar_.Init(local_state);
    local_state_pref_registrar_.Add(
        prefs::kOverscrollHistoryNavigationEnabled,
        base::BindRepeating(
            &OverscrollPrefManager::OnOverscrollHistoryNavigationEnabledChanged,
            base::Unretained(this)));
  }
}

OverscrollPrefManager::~OverscrollPrefManager() = default;

void OverscrollPrefManager::OnOverscrollHistoryNavigationEnabledChanged() {
  overscroll_history_navigation_enabled_ =
      g_browser_process->local_state()->GetBoolean(
          prefs::kOverscrollHistoryNavigationEnabled);

  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  for (int tab_index = 0; tab_index < tab_strip_model->count(); ++tab_index) {
    content::WebContents* web_contents =
        tab_strip_model->GetWebContentsAt(tab_index);
    web_contents->SetOverscrollNavigationEnabled(
        overscroll_history_navigation_enabled_);
  }
}

bool OverscrollPrefManager::IsOverscrollHistoryNavigationEnabled() const {
  return overscroll_history_navigation_enabled_;
}
