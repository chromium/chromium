// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.system.display for testing.
 */

import {assert} from 'chrome://resources/ash/common/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';

/**
 * Fake of the chrome.settings.display API.
 * @constructor
 * @implements {SystemDisplay}
 */
export function FakeSystemDisplay() {
  /** @type {!Array<!chrome.system.display.DisplayUnitInfo>} */
  this.fakeDisplays = [];
  this.fakeLayouts = [];
  this.getInfoCalled = new PromiseResolver();
  this.getLayoutCalled = new PromiseResolver();
}

FakeSystemDisplay.prototype = {
  // Public testing methods.
  /**
   * @param {!chrome.system.display.DisplayUnitInfo>} display
   */
  addDisplayForTest(display) {
    this.fakeDisplays.push(display);
    this.updateLayouts_();
  },

  // SystemDisplay overrides.
  /** @override */
  getInfo(flags) {
    return new Promise((resolve) => {
      setTimeout(() => {
        // Create a shallow copy to trigger Polymer data binding updates.
        let displays;
        if (this.fakeDisplays.length > 0 &&
            this.fakeDisplays[0].mirroringSourceId) {
          // When mirroring is enabled, send only the info for the display
          // being mirrored.
          const display =
              this.getFakeDisplay_(this.fakeDisplays[0].mirroringSourceId);
          assert(!!display);
          displays = [display];
        } else {
          displays = this.fakeDisplays.slice();
        }
        resolve(displays);
        this.getInfoCalled.resolve();
        // Reset the promise resolver.
        this.getInfoCalled = new PromiseResolver();
      });
    });
  },

  /** @override */
  setDisplayProperties(id, info) {
    const display = this.getFakeDisplay_(id);
    if (!display) {
      chrome.runtime.lastError = 'Display not found.';
      return Promise.reject();
    }

    if (info.mirroringSourceId !== undefined) {
      for (const d of this.fakeDisplays) {
        d.mirroringSourceId = info.mirroringSourceId;
      }
    }

    if (info.isPrimary !== undefined) {
      let havePrimary = info.isPrimary;
      for (const d of this.fakeDisplays) {
        if (d.id === id) {
          d.isPrimary = info.isPrimary;
        } else if (havePrimary) {
          d.isPrimary = false;
        } else {
          d.isPrimary = true;
          havePrimary = true;
        }
      }
      this.updateLayouts_();
    }
    if (info.rotation !== undefined) {
      display.rotation = info.rotation;
    }
    return Promise.resolve();
  },

  /** @override */
  getDisplayLayout() {
    return new Promise((resolve) => {
      setTimeout(() => {
        // Create a shallow copy to trigger Polymer data binding updates.
        resolve(this.fakeLayouts.slice());
        this.getLayoutCalled.resolve();
        // Reset the promise resolver.
        this.getLayoutCalled = new PromiseResolver();
      });
    });
  },

  /** @override */
  setDisplayLayout(layouts) {
    this.fakeLayouts = layouts;
    return Promise.resolve();
  },

  /** @override */
  setMirrorMode(info) {
    let mirroringSourceId = '';
    if (info.mode === chrome.system.display.MirrorMode.NORMAL) {
      // Select the primary display as the mirroring source.
      for (const d of this.fakeDisplays) {
        if (d.isPrimary) {
          mirroringSourceId = d.id;
          break;
        }
      }
    }
    for (const d of this.fakeDisplays) {
      d.mirroringSourceId = mirroringSourceId;
    }
    return Promise.resolve();
  },

  /** @override */
  onDisplayChanged: new FakeChromeEvent(),

  /** @private */
  getFakeDisplay_(id) {
    const idx = this.fakeDisplays.findIndex(function(display) {
      return display.id === id;
    });
    if (idx >= 0) {
      return this.fakeDisplays[idx];
    }
    return undefined;
  },

  /** @private */
  updateLayouts_() {
    this.fakeLayouts = [];
    let primaryId = '';
    for (const d of this.fakeDisplays) {
      if (d.isPrimary) {
        primaryId = d.id;
        break;
      }
    }
    for (const d of this.fakeDisplays) {
      this.fakeLayouts.push({
        id: d.id,
        parentId: d.isPrimary ? '' : primaryId,
        position: chrome.system.display.LayoutPosition.RIGHT,
        offset: 0,
      });
    }
  },
};
