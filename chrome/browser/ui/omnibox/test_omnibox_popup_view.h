// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_POPUP_VIEW_H_

#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"

// Fake implementation of OmniboxPopupView for use in tests.
class TestOmniboxPopupView : public OmniboxPopupView {
 public:
  TestOmniboxPopupView() : OmniboxPopupView(/*controller=*/nullptr) {}
  ~TestOmniboxPopupView() override = default;
  bool IsOpen() const override;
  void InvalidateLine(size_t line) override {}
  void UpdatePopupAppearance() override {}
  void ProvideButtonFocusHint(size_t line) override {}
  void OnDragCanceled() override {}
  void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const override {}
  raw_ptr<OmniboxPopupViewWebUI> GetOmniboxPopupViewWebUI() override;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_POPUP_VIEW_H_
