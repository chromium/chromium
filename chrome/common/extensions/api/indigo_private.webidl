// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use the <code>chrome.indigoPrivate</code> API for specific browser
// functionality that the Indigo extension needs.
interface IndigoPrivate {
  // Notifies the browser that the extension frame is ready to be rendered.
  static Promise<undefined> readyToRender();
};

partial interface Browser {
  static attribute IndigoPrivate indigoPrivate;
};
