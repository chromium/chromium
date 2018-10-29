// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.CrostiniBrowserProxy} */
class TestCrostiniBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestCrostiniInstallerView',
      'requestRemoveCrostini',
      'getCrostiniSharedPathsDisplayText',
      'removeCrostiniSharedPath',
    ]);
    this.enabled = false;
    this.sharedPaths = ['path1', 'path2'];
  }

  /** @override */
  requestCrostiniInstallerView() {
    this.methodCalled('requestCrostiniInstallerView');
    this.enabled = true;
  }

  /** override */
  requestRemoveCrostini() {
    this.methodCalled('requestRemoveCrostini');
    this.enabled = false;
  }

  /** override */
  getCrostiniSharedPathsDisplayText(paths) {
    this.methodCalled('getCrostiniSharedPathsDisplayText');
    return Promise.resolve(paths.map(path => path + '-displayText'));
  }

  /** override */
  removeCrostiniSharedPath(path) {
    this.sharedPaths = this.sharedPaths.filter(p => p !== path);
  }
}
