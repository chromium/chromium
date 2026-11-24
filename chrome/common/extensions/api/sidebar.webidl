// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the possible alignment for the sidebar in the browser UI.
enum Side {
  "left",
  "right"
};

// Represents the layout of the sidebar, indicating which side it is shown on.
dictionary PanelLayout {
  required Side side;
};

// The contents of the "sidebar" manifest key.
[nodoc] dictionary SidebarManifestKey {
  // Relative path to the extension page that the sidebar shows by default.
  DOMString default_path;
};

partial dictionary ExtensionManifest {
  SidebarManifestKey sidebar;
};

// Use the `chrome.sidebar` API to host content in the browser's sidebar
// alongside the main content of a webpage.
interface Sidebar {
  // Returns the sidebar's current layout.
  // |PromiseValue|: layout
  static Promise<PanelLayout> getLayout();
};

partial interface Browser {
  static attribute Sidebar sidebar;
};
