// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include "base/callback.h"
#include "base/callback_forward.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_later_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

SidePanelCoordinator::SidePanelCoordinator(BrowserView* browser_view)
    : browser_view_(browser_view) {
  // TODO(pbos): Consider moving creation of SidePanelEntry into other functions
  // that can easily be unittested.
  window_registry_.Register(std::make_unique<SidePanelEntry>(
      l10n_util::GetStringUTF16(IDS_READ_LATER_TITLE),
      base::BindRepeating(
          [](SidePanelCoordinator* coordinator,
             Browser* browser) -> std::unique_ptr<views::View> {
            return std::make_unique<ReadLaterSidePanelWebView>(
                browser, base::BindRepeating(&SidePanelCoordinator::Close,
                                             base::Unretained(coordinator)));
          },
          this, browser_view->browser())));
}

SidePanelCoordinator::~SidePanelCoordinator() = default;

void SidePanelCoordinator::Show() {
  if (active_content_)
    return;
  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_HIDE));
  // TODO(pbos): Handle multiple entries.
  DCHECK_EQ(1u, window_registry_.entries().size());
  std::unique_ptr<views::View> content =
      window_registry_.entries()[0]->CreateContent();
  active_content_ = content.get();
  browser_view_->right_aligned_side_panel()->AddChildView(std::move(content));
}

void SidePanelCoordinator::Close() {
  if (!active_content_)
    return;

  // TODO(pbos): Make this button observe panel-visibility state instead.
  browser_view_->toolbar()->side_panel_button()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_SIDE_PANEL_SHOW));

  browser_view_->right_aligned_side_panel()->RemoveChildViewT(active_content_);
  active_content_ = nullptr;
}

void SidePanelCoordinator::Toggle() {
  if (active_content_) {
    Close();
  } else {
    Show();
  }
}
