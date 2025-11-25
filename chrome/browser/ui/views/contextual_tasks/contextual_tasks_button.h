// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/prefs/pref_member.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

class BrowserWindowInterface;

class ContextualTasksButton : public ToolbarButton {
  METADATA_HEADER(ContextualTasksButton, ToolbarButton)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kContextualTasksToolbarButton);
  explicit ContextualTasksButton(
      BrowserWindowInterface* browser_window_interface);
  ~ContextualTasksButton() override;

 private:
  void OnButtonPress();
  void OnPinStateChanged();
  void OnShouldUpdateVisibility(bool should_show);

  BooleanPrefMember pin_state_;
  base::CallbackListSubscription should_update_visibility_subscription_;
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_BUTTON_H_
