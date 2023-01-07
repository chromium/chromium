// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_H_
#define CHROME_BROWSER_UI_TABS_TAB_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

template <typename TContents>
class TabBase {
 public:
  explicit TabBase(std::unique_ptr<TContents> contents)
      : contents_(std::move(contents)) {}

  TabBase(const TabBase&) = delete;
  TabBase& operator=(const TabBase&) = delete;

  TContents* contents() const { return contents_.get(); }
  TContents* opener() const { return opener_; }
  bool reset_opener_on_active_tab_change() const {
    return reset_opener_on_active_tab_change_;
  }
  bool pinned() const { return pinned_; }
  bool blocked() const { return blocked_; }
  absl::optional<tab_groups::TabGroupId> group() const { return group_; }

  void set_contents(std::unique_ptr<TContents> contents) {
    contents_ = contents;
  }
  void set_opener(TContents* opener) { opener_ = opener; }
  void set_reset_opener_on_active_tab_change(
      bool reset_opener_on_active_tab_change) {
    reset_opener_on_active_tab_change_ = reset_opener_on_active_tab_change;
  }
  void set_pinned(bool pinned) { pinned_ = pinned; }
  void set_blocked(bool blocked) { blocked_ = blocked; }
  void set_group(absl::optional<tab_groups::TabGroupId> group) {
    group_ = group;
  }

  std::unique_ptr<TContents> ReplaceContents(
      std::unique_ptr<TContents> contents) {
    contents_.swap(contents);
    return contents;
  }

 private:
  std::unique_ptr<TContents> contents_;
  raw_ptr<TContents> opener_ = nullptr;
  bool reset_opener_on_active_tab_change_ = false;
  bool pinned_ = false;
  bool blocked_ = false;
  absl::optional<tab_groups::TabGroupId> group_ = absl::nullopt;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_H_
