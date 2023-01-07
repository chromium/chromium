// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_CONTENT_PROXY_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_CONTENT_PROXY_H_

#include <algorithm>
#include <memory>

#include "base/functional/callback.h"
#include "ui/views/view.h"

// SidePanelContentProxy is used by the SidePanelCoordinator to check that the
// content view provided in the callback from the SidePanelEntry constructor is
// ready to be shown. This should be used for any side panel content that is not
// immediately loaded and ready to be shown (i.e. WebUI content) to prevent
// loading artifacts from being visible in the browser UI.
class SidePanelContentProxy final {
 public:
  explicit SidePanelContentProxy(bool available = true);
  SidePanelContentProxy(const SidePanelContentProxy&) = delete;
  SidePanelContentProxy& operator=(const SidePanelContentProxy&) = delete;
  ~SidePanelContentProxy();

  bool IsAvailable() { return available_; }
  void SetAvailable(bool available);
  void SetAvailableCallback(base::OnceCallback<void()> callback) {
    available_callback_ = std::move(callback);
  }
  void ResetAvailableCallback();

 private:
  bool available_;
  base::OnceClosure available_callback_;
};

extern const ui::ClassProperty<SidePanelContentProxy*>* const
    kSidePanelContentProxyKey;

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_CONTENT_PROXY_H_
