// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"

class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}

// CommentsSidePanelCoordinator handles the creation and registration of
// the comments SidePanelEntry.
class CommentsSidePanelCoordinator {
 public:
  CommentsSidePanelCoordinator() = default;
  ~CommentsSidePanelCoordinator() = default;

  // Returns whether CommentsSidePanelCoordinator is supported.
  // If this returns false, it should not be registered with the side
  // panel registry.
  static bool IsSupported();

  // Creates and registers the comments side panel entry in the global registry.
  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  // Creates the comments web view that will be used as the content of the
  // comments side panel entry.
  std::unique_ptr<views::View> CreateCommentsWebView(
      SidePanelEntryScope& scope);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMMENTS_COMMENTS_SIDE_PANEL_COORDINATOR_H_
