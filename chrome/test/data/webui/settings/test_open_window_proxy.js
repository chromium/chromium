// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.OpenWindowProxy} */
class TestOpenWindowProxy extends TestBrowserProxy {
  constructor() {
    super([
      'openURL',
    ]);
  }

  /** @override */
  openURL(url) {
    this.methodCalled('openURL', url);
  }
}
