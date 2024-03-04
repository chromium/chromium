// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {ReadAnythingElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/app.js';

export function emitEvent(
    app: ReadAnythingElement, name: string, options?: any): void {
  app.$.toolbar.dispatchEvent(new CustomEvent(name, options));
  flush();
}

/**
 * Suppresses harmless ResizeObserver errors due to a browser bug.
 * yaqs/2300708289911980032
 */
export function suppressInnocuousErrors() {
  const onerror = window.onerror;
  window.onerror = (message, url, lineNumber, column, error) => {
    if ([
          'ResizeObserver loop limit exceeded',
          'ResizeObserver loop completed with undelivered notifications.',
        ].includes(message.toString())) {
      console.info('Suppressed ResizeObserver error: ', message);
      return;
    }
    if (onerror) {
      onerror.apply(window, [message, url, lineNumber, column, error]);
    }
  };
}
