// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @const {boolean}
 */
window.isSWA = true;

import './crt0.js';

/**
 * Load modules.
 */
import {BrowserProxy} from './browser_proxy.js'
import {ScriptLoader} from './script_loader.js'
import {VolumeManagerImpl} from '../../../../../ui/file_manager/file_manager/background/js/volume_manager_impl.m.js';
import '../../../../../ui/file_manager/file_manager/background/js/metrics_start.m.js';
import {background} from '../../../../../ui/file_manager/file_manager/background/js/background.m.js';

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

    // Temporarily remove window.cr while the foreground script bundle loads.
    const origCr = window.cr;
    delete window.cr;
    // Avoid double loading the LoadTimeData strings.
    window.loadTimeData.data_ = null;

    await Promise.all([
      new ScriptLoader('foreground/js/elements_importer.js').load(),
      new ScriptLoader('foreground/js/main_scripts.js', {defer: true}).load(),
    ]);
    // Restore the window.cr object.
    Object.assign(window.cr, origCr);
    console.debug('Files app UI loaded');
  }
}

const app = new FileManagerApp();
app.run();
