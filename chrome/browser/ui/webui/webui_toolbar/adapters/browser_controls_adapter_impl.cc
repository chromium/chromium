// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter_impl.h"

#include "base/check_deref.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/split_tabs_utils.h"
#include "chrome/browser/ui/webui/webui_toolbar/utils/toolbar_button_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace browser_controls_api {

BrowserControlsAdapterImpl::BrowserControlsAdapterImpl(
    BrowserWindowInterface* browser_interface,
    CommandUpdater* command_updater)
    : browser_(CHECK_DEREF(browser_interface)),
      command_updater_(CHECK_DEREF(command_updater)) {}

BrowserControlsAdapterImpl::~BrowserControlsAdapterImpl() {}

void BrowserControlsAdapterImpl::Reload(bool bypass_cache,
                                        WindowOpenDisposition disposition) {
  command_updater_->ExecuteCommandWithDisposition(
      bypass_cache ? IDC_RELOAD_BYPASSING_CACHE : IDC_RELOAD, disposition);
}

void BrowserControlsAdapterImpl::Stop() {
  command_updater_->ExecuteCommandWithDisposition(
      IDC_STOP, WindowOpenDisposition::CURRENT_TAB);
}

void BrowserControlsAdapterImpl::Back(WindowOpenDisposition disposition) {
  command_updater_->ExecuteCommandWithDisposition(IDC_BACK, disposition);
}

void BrowserControlsAdapterImpl::Forward(WindowOpenDisposition disposition) {
  command_updater_->ExecuteCommandWithDisposition(IDC_FORWARD, disposition);
}

void BrowserControlsAdapterImpl::BackButtonHovered() {
  auto* const active_tab = browser_.get().GetActiveTabInterface();
  if (!active_tab) {
    return;
  }
  auto* const contents = active_tab->GetContents();
  if (!contents) {
    return;
  }
  contents->BackNavigationLikely(chrome_preloading_predictor::kBackButtonHover,
                                 WindowOpenDisposition::CURRENT_TAB);
}

void BrowserControlsAdapterImpl::CreateNewSplitTab() {
  chrome::NewSplitTab(&browser_.get(), split_tabs::SplitTabLayout::kVertical,
                      split_tabs::SplitTabCreatedSource::kToolbarButton);
}

void BrowserControlsAdapterImpl::NavigateHome(
    WindowOpenDisposition disposition) {
  command_updater_->ExecuteCommandWithDisposition(IDC_HOME, disposition);
}

webui_toolbar::TabSplitStatus
BrowserControlsAdapterImpl::ComputeSplitTabStatus() {
  return webui_toolbar::ComputeTabSplitStatus(&browser_.get());
}

}  // namespace browser_controls_api
