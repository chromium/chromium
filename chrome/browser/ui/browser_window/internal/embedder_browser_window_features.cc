// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/embedder_browser_window_features.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

DEFINE_USER_DATA(EmbedderBrowserWindowFeatures);

EmbedderBrowserWindowFeatures::EmbedderBrowserWindowFeatures(
    BrowserWindowInterface* browser) {}

EmbedderBrowserWindowFeatures::~EmbedderBrowserWindowFeatures() = default;

// static
EmbedderBrowserWindowFeatures* EmbedderBrowserWindowFeatures::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

void EmbedderBrowserWindowFeatures::InitPostWindowConstruction(
    BrowserWindowInterface* browser) {
  // Embedder implementation goes here.
}

void EmbedderBrowserWindowFeatures::InitPostBrowserViewConstruction(
    BrowserView* browser_view) {
  // Embedder implementation goes here.
}

void EmbedderBrowserWindowFeatures::TearDownPreBrowserWindowDestruction() {
  // Embedder implementation goes here.
}
