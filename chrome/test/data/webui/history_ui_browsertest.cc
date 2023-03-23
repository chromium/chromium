// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/history_ui_browsertest.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"

HistoryUIBrowserTest::HistoryUIBrowserTest() : history_(nullptr) {}
HistoryUIBrowserTest::~HistoryUIBrowserTest() {}

void HistoryUIBrowserTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();

  history_ = HistoryServiceFactory::GetForProfile(
      browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  ui_test_utils::WaitForHistoryToLoad(history_);
}

void HistoryUIBrowserTest::SetDeleteAllowed(bool allowed) {
  browser()->profile()->GetPrefs()->
      SetBoolean(prefs::kAllowDeletingBrowserHistory, allowed);
}
