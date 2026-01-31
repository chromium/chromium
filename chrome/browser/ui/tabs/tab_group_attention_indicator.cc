// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_attention_indicator.h"

TabGroupAttentionIndicator::TabGroupAttentionIndicator() = default;
TabGroupAttentionIndicator::~TabGroupAttentionIndicator() = default;

void TabGroupAttentionIndicator::SetHasAttention(bool has_attention) {
  if (has_attention_ == has_attention) {
    return;
  }

  has_attention_ = has_attention;
  for (Observer& observer : observers_) {
    observer.OnAttentionStateChanged();
  }
}
