// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from './browser_proxy.js'
import {ScriptLoader} from './script_loader.js'

/**
 * Represents file manager application. Starting point for the application
 * interaction.
 */
class FileManagerApp {
  constructor() {
    console.info('File manager app created ...');
  }

  async run() {
    await new ScriptLoader('legacy_main_scripts.js').load();
    console.debug('Legacy code loaded');
  }
}

const app = new FileManagerApp();
document.addEventListener('DOMContentLoaded', () => {
  app.run();
});
