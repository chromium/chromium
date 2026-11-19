// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use the `chrome.sidebar` API to host content in the browser's sidebar
// alongside the main content of a webpage.
interface Sidebar {};

partial interface Browser {
  static attribute Sidebar sidebar;
};
