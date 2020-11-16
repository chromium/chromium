// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from './browser_proxy.js'

/**
 * Represents file manager application. Starting point for the application
 * interaction.
 */
class FileManagerApp {
  constructor() {
    console.info('File manager app created ...');
  }

  /**
   * Lazily loads File App legacy code.
   */
  loadLegacyCode() {
    const legacyLoader = document.createElement('script');
    legacyLoader.src = 'legacy_main_scripts.js';
    document.body.appendChild(legacyLoader);
  }

  run() {
    this.loadLegacyCode();
  }
}

const app = new FileManagerApp();
document.addEventListener('DOMContentLoaded', () => {
  app.run();
});
