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

  /**
   * Demonstrates Mojo interactions.
   */
  demoMojo() {
    // Basic example of establishing communication with the backend.
    const browserProxy = new BrowserProxy();

    // There must be only one listener returning values.
    browserProxy.callbackRouter.getBar.addListener((foo) => {
      console.log('GetBar(' + foo + ')');
      return Promise.resolve({bar: 'baz'});
    });

    // Listen-only callbacks can be multiple.
    browserProxy.callbackRouter.onSomethingHappened.addListener(
        (something, other) => {
          console.log('OnSomethingHappened(' + something + ', ' + other + ')');
        });
    browserProxy.callbackRouter.onSomethingHappened.addListener(
        (something, other) => {
          console.log('eh? ' + something + '. what? ' + other);
        });

    // Show the interaction via Mojo.
    window.setTimeout(() => {
      browserProxy.handler.setFoo('foo-value');
      browserProxy.handler.doABarrelRoll();
    }, 1000);
  }

  run() {
    this.loadLegacyCode();
    this.demoMojo();
  }
}

const app = new FileManagerApp();
document.addEventListener('DOMContentLoaded', () => {
  app.run();
});
