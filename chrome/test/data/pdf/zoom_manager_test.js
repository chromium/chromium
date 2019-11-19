// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserApi} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/browser_api.js';
import {ZoomManager} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/zoom_manager.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';

chrome.test.runTests(function() {
  'use strict';

  class MockViewport {
    constructor() {
      /** @type {!Array<number>} */
      this.zooms = [];

      /** @type {number} */
      this.zoom = 1;

      /** @type {boolean} */
      this.browserOnlyZoomChange = false;

      /** @type {!EventTracker} */
      this.tracker_ = new EventTracker();
    }

    /** @param {!EventTarget} target */
    addZoomListeners(target) {
      this.tracker_.add(target, 'set-zoom', e => this.setZoom(e.detail));
      this.tracker_.add(
          target, 'update-zoom-from-browser',
          e => this.updateZoomFromBrowserChange(e.detail));
    }

    removeListeners() {
      this.tracker_.removeAll();
    }

    /** @param {number} zoom */
    setZoom(zoom) {
      this.zooms.push(zoom);
      this.zoom = zoom;
    }

    /** @return {number} The current zoom. */
    getZoom() {
      return this.zoom;
    }

    /** @param {number} oldBrowserZoom */
    updateZoomFromBrowserChange(oldBrowserZoom) {
      this.browserOnlyZoomChange = true;
    }
  }

  /**
   * A mock implementation of the function used by ZoomManager to set the
   * browser zoom level.
   */
  class MockBrowserZoomSetter {
    constructor() {
      this.zoom = 1;
      this.started = false;
    }

    /**
     * The function implementing setBrowserZoomFunction.
     * @param {number} zoom
     */
    setBrowserZoom(zoom) {
      chrome.test.assertFalse(this.started);

      this.zoom = zoom;
      this.started = true;
      return new Promise(function(resolve, reject) {
        this.resolve_ = resolve;
      }.bind(this));
    }

    /**
     * Resolves the promise returned by a call to setBrowserZoom.
     */
    complete() {
      this.resolve_();
      this.started = false;
    }
  }

  return [
    function testZoomChange() {
      const viewport = new MockViewport();
      const browserZoomSetter = new MockBrowserZoomSetter();
      const zoomManager = ZoomManager.create(
          BrowserApi.ZoomBehavior.MANAGE, () => viewport.getZoom(),
          zoom => browserZoomSetter.setBrowserZoom(zoom), 1);
      viewport.addZoomListeners(zoomManager.getEventTarget());
      viewport.zoom = 2;
      zoomManager.onPdfZoomChange();
      chrome.test.assertEq(2, browserZoomSetter.zoom);
      chrome.test.assertTrue(browserZoomSetter.started);
      viewport.removeListeners();
      chrome.test.succeed();
    },

    function testBrowserZoomChange() {
      const viewport = new MockViewport();
      const zoomManager = ZoomManager.create(
          BrowserApi.ZoomBehavior.MANAGE, () => viewport.getZoom(),
          chrome.test.fail, 1);
      viewport.addZoomListeners(zoomManager.getEventTarget());
      zoomManager.onBrowserZoomChange(3);
      chrome.test.assertEq(1, viewport.zooms.length);
      chrome.test.assertEq(3, viewport.zooms[0]);
      chrome.test.assertEq(3, viewport.zoom);
      viewport.removeListeners();
      chrome.test.succeed();
    },

    function testBrowserZoomChangeEmbedded() {
      const viewport = new MockViewport();
      const zoomManager = ZoomManager.create(
          BrowserApi.ZoomBehavior.PROPAGATE_PARENT,
          () => viewport.getZoom(), function() {
            return Promise.reject();
          }, 1);
      viewport.addZoomListeners(zoomManager.getEventTarget());

      // Zooming in the browser should not overwrite the viewport's zoom,
      // but be applied seperately.
      viewport.zoom = 2;
      zoomManager.onBrowserZoomChange(3);
      chrome.test.assertEq(2, viewport.zoom);
      chrome.test.assertTrue(viewport.browserOnlyZoomChange);
      viewport.removeListeners();

      chrome.test.succeed();
    },

    function testSmallZoomChange() {
      const viewport = new MockViewport();
      const browserZoomSetter = new MockBrowserZoomSetter();
      const zoomManager = ZoomManager.create(
          BrowserApi.ZoomBehavior.MANAGE, () => viewport.getZoom(),
          zoom => browserZoomSetter.setBrowserZoom(zoom), 2);
      viewport.addZoomListeners(zoomManager.getEventTarget());
      viewport.zoom = 2.0001;
      zoomManager.onPdfZoomChange();
      chrome.test.assertEq(1, browserZoomSetter.zoom);
      chrome.test.assertFalse(browserZoomSetter.started);
      viewport.removeListeners();
      chrome.test.succeed();
    },

    function testSmallBrowserZoomChange() {
      const viewport = new MockViewport();
      const zoomManager = ZoomManager.create(
          BrowserApi.ZoomBehavior.MANAGE, () => viewport.getZoom(),
          chrome.test.fail, 1);
      viewport.addZoomListeners(zoomManager.getEventTarget());
      zoomManager.onBrowserZoomChange(0.999);
      chrome.test.assertEq(0, viewport.zooms.length);
      chrome.test.assertEq(1, viewport.zoom);
      viewport.removeListeners();
      chrome.test.succeed();
    },

    function testMultiplePdfZoomChanges() {
      const viewport = new MockViewport();
      const browserZoomSetter = new MockBrowserZoomSetter();
      const zoomManager = ZoomManager.create(
          BrowserApi.ZoomBehavior.MANAGE, () => viewport.getZoom(),
          zoom => browserZoomSetter.setBrowserZoom(zoom), 1);
      viewport.addZoomListeners(zoomManager.getEventTarget());
      viewport.zoom = 2;
      zoomManager.onPdfZoomChange();
      viewport.zoom = 3;
      zoomManager.onPdfZoomChange();
      chrome.test.assertTrue(browserZoomSetter.started);
      chrome.test.assertEq(2, browserZoomSetter.zoom);
      browserZoomSetter.complete();
      Promise.resolve().then(function() {
        chrome.test.assertTrue(browserZoomSetter.started);
        chrome.test.assertEq(3, browserZoomSetter.zoom);
        viewport.removeListeners();
        chrome.test.succeed();
      });
    },

    function testMultipleBrowserZoomChanges() {
      const viewport = new MockViewport();
      const zoomManager = ZoomManager.create(
          BrowserApi.ZoomBehavior.MANAGE, () => viewport.getZoom(),
          chrome.test.fail, 1);
      viewport.addZoomListeners(zoomManager.getEventTarget());
      zoomManager.onBrowserZoomChange(2);
      zoomManager.onBrowserZoomChange(3);
      chrome.test.assertEq(2, viewport.zooms.length);
      chrome.test.assertEq(2, viewport.zooms[0]);
      chrome.test.assertEq(3, viewport.zooms[1]);
      chrome.test.assertEq(3, viewport.zoom);
      viewport.removeListeners();
      chrome.test.succeed();
    },
  ];
}());
