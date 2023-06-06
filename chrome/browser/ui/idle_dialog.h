// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_IDLE_DIALOG_H_
#define CHROME_BROWSER_UI_IDLE_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/views/widget/widget.h"

class Browser;

// Idle timeout dialog. This is shown to users to inform them that Chrome will
// be closed by the IdleService, as dictated by the IdleProfileCloseTimeout
// policy.
class IdleDialog {
 public:
  // The dialog needs to know what actions are configured, so it can display a
  // more helpful string to the user.
  //
  // SetActions() can't take a flat_set<ActionType>, because we can't include
  // action.h from here. Pass this struct instead, which is what we really need
  // to know.
  struct ActionSet {
    bool close;  // True if ActionType::kCloseBrowsers is present.
    bool clear;  // True if any of ActionType::kClear* is present.
  };

  // Implemented in //chrome/browser/ui/views/idle_dialog_view.cc
  static base::WeakPtr<views::Widget> Show(Browser* browser,
                                           base::TimeDelta dialog_duration,
                                           base::TimeDelta idle_threshold,
                                           ActionSet actions,
                                           base::OnceClosure on_close_by_user);
};

#endif  // CHROME_BROWSER_UI_IDLE_DIALOG_H_
