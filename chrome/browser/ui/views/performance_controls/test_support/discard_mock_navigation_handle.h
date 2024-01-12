// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TEST_SUPPORT_DISCARD_MOCK_NAVIGATION_HANDLE_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TEST_SUPPORT_DISCARD_MOCK_NAVIGATION_HANDLE_H_

#include "content/public/test/mock_navigation_handle.h"

class DiscardMockNavigationHandle : public content::MockNavigationHandle {
 public:
  void SetWasDiscarded(bool was_discarded);
  bool ExistingDocumentWasDiscarded() const override;
  void SetWebContents(content::WebContents* web_contents);
  content::WebContents* GetWebContents() override;

 private:
  bool was_discarded_ = false;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TEST_SUPPORT_DISCARD_MOCK_NAVIGATION_HANDLE_H_
