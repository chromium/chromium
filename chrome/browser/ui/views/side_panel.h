// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_H_

#include <memory>

#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class SidePanel : public views::View, public views::ViewObserver {
 public:
  METADATA_HEADER(SidePanel);
  SidePanel();
  SidePanel(const SidePanel&) = delete;
  SidePanel& operator=(const SidePanel&) = delete;
  ~SidePanel() override;

  void SetPanelWidth(int width);

 private:
  void UpdateVisibility();

  // views::View:
  void OnThemeChanged() override;
  void ChildVisibilityChanged(View* child) override;

  // views::ViewObserver:
  void OnChildViewAdded(View* observed_view, View* child) override;
  void OnChildViewRemoved(View* observed_view, View* child) override;

  // TODO(pbos): Separate BDDV use from its content so we can host the content
  // without a bubble delegate. SidePanel is not a bubble.
  std::vector<std::unique_ptr<views::BubbleDialogDelegateView>> owned_children_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_H_
