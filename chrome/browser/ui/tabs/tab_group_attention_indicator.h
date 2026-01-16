// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_ATTENTION_INDICATOR_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_ATTENTION_INDICATOR_H_

// Manages the attention state of a tab group.
class TabGroupAttentionIndicator {
 public:
  TabGroupAttentionIndicator();
  ~TabGroupAttentionIndicator();

  bool has_attention() const { return has_attention_; }
  void set_has_attention(bool has_attention) { has_attention_ = has_attention; }

 private:
  bool has_attention_ = false;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_ATTENTION_INDICATOR_H_
