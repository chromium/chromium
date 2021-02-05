// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestBrowserProxy} from '../../../test_browser_proxy.m.js';
// clang-format on

/** @implements {settings.PluginVmBrowserProxy} */
/* #export */ class TestPluginVmBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isRelaunchNeededForNewPermissions',
      'relaunchPluginVm',
    ]);
    this.pluginVmRunning = false;
  }

  /** @override */
  isRelaunchNeededForNewPermissions() {
    this.methodCalled('isRelaunchNeededForNewPermissions');
    return Promise.resolve(this.pluginVmRunning);
  }

  /** @override */
  relaunchPluginVm() {
    this.methodCalled('relaunchPluginVm');
  }
}
