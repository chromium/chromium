// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_EMBEDDER_BROWSER_WINDOW_FEATURES_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_EMBEDDER_BROWSER_WINDOW_FEATURES_H_

#include <memory>

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserView;
class BrowserWindowInterface;

// This class should only be used by Chromium embedders, where it will own
// embedder-specific browser window features.
// Chromium features themselves should not be added to this class, rather to
// BrowserWindowFeatures directly.
class EmbedderBrowserWindowFeatures {
 public:
  DECLARE_USER_DATA(EmbedderBrowserWindowFeatures);

  // Constructor is called exactly once to initialize features prior to
  // instantiating BrowserView, to allow the view hierarchy to depend on state
  // in this class.
  explicit EmbedderBrowserWindowFeatures(BrowserWindowInterface* browser);
  ~EmbedderBrowserWindowFeatures();

  static EmbedderBrowserWindowFeatures* From(BrowserWindowInterface* browser);

  // Called exactly once to initialize features that depend on the window object
  // being created.
  void InitPostWindowConstruction(BrowserWindowInterface* browser);

  // Called exactly once to initialize features that depend on the view
  // hierarchy in BrowserView.
  void InitPostBrowserViewConstruction(BrowserView* browser_view);

  // Called exactly once to tear down state that depends on the window object.
  void TearDownPreBrowserWindowDestruction();

 private:
  // Embedder-specific feature members:
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_PUBLIC_EMBEDDER_BROWSER_WINDOW_FEATURES_H_
