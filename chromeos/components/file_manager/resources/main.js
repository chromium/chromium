// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from './browser_proxy.js'
import {ScriptLoader} from './script_loader.js'

/**
 * @const {boolean}
 */
window.isSWA = true;

/**
 * Represents file manager application. Starting point for the application
 * interaction.
 */
class FileManagerApp {
  constructor() {
    /**
     * Creates a Mojo pipe to the C++ SWA container.
     * @private @const {!BrowserProxy}
     */
    this.browserProxy_ = new BrowserProxy();
  }

  /** @return {!BrowserProxy} */
  get browserProxy() {
    return this.browserProxy_;
  }

  /**
   * Start-up: load the page scripts in order: fakes first (to provide chrome.*
   * API that the files app foreground scripts expect for initial render), then
   * the files app foreground scripts. Note main_scripts.js should have 'defer'
   * true per crbug.com/496525.
   */
  async run() {
    await Promise.all([
        new ScriptLoader('file_manager_private_fakes.js').load(),
        new ScriptLoader('file_manager_fakes.js').load(),
    ]);

    await Promise.all([
      new ScriptLoader('foreground/js/elements_importer.js').load(),
      new ScriptLoader('foreground/js/main_scripts.js', {defer: true}).load(),
    ]);
    console.debug('Files app legacy UI loaded');
  }
}

const app = new FileManagerApp();
app.run();
