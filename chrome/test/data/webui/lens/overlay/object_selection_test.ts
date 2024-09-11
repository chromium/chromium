// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens-overlay/selection_overlay.js';

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {BrowserProxyImpl} from 'chrome-untrusted://lens-overlay/browser_proxy.js';
import type {CenterRotatedBox} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import {UserAction} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {OverlayObject} from 'chrome-untrusted://lens-overlay/overlay_object.mojom-webui.js';
import type {SelectionOverlayElement} from 'chrome-untrusted://lens-overlay/selection_overlay.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome-untrusted://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {assertBoxesWithinThreshold, createObject} from '../utils/object_utils.js';
import {simulateClick} from '../utils/selection_utils.js';

import {TestLensOverlayBrowserProxy} from './test_overlay_browser_proxy.js';


suite('ObjectSelection', function() {
  let testBrowserProxy: TestLensOverlayBrowserProxy;
  let selectionOverlayElement: SelectionOverlayElement;
  let callbackRouterRemote: LensPageRemote;
  let objects: OverlayObject[];
  let metrics: MetricsTracker;

  setup(async () => {
    // Resetting the HTML needs to be the first thing we do in setup to
    // guarantee that any singleton instances don't change while any UI is still
    // attached to the DOM.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testBrowserProxy = new TestLensOverlayBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    BrowserProxyImpl.setInstance(testBrowserProxy);

    // Turn off the shimmer. Since the shimmer is resource intensive, turn off
    // to prevent from causing issues in the tests.
    loadTimeData.overrideValues({'enableShimmer': false});

    selectionOverlayElement = document.createElement('lens-selection-overlay');
    document.body.appendChild(selectionOverlayElement);
    // Since the size of the Selection Overlay is based on the screenshot which
    // is not loaded in the test, we need to force the overlay to take up the
    // viewport.
    selectionOverlayElement.$.selectionOverlay.style.width = '100%';
    selectionOverlayElement.$.selectionOverlay.style.height = '100%';
    metrics = fakeMetricsPrivate();
    await flushTasks();
    await addObjects();
  });

  // Normalizes the given values to the size of selection overlay.
  function normalizedBox(box: RectF): RectF {
    const boundingRect = selectionOverlayElement.getBoundingClientRect();
    return {
      x: box.x / boundingRect.width,
      y: box.y / boundingRect.height,
      width: box.width / boundingRect.width,
      height: box.height / boundingRect.height,
    };
  }

  function addObjects() {
    // The object at index 0 will have a segmentation mask.
    objects =
        [
          {x: 20, y: 15, width: 20, height: 10},
          {x: 120, y: 15, width: 30, height: 10},
          {x: 70, y: 35, width: 50, height: 20},
          {x: 320, y: 50, width: 80, height: 30},
          {x: 320, y: 50, width: 40, height: 20},
          {x: 320, y: 50, width: 60, height: 25},
        ]
            .map(
                (rect, i) => createObject(
                    i.toString(), normalizedBox(rect),
                    /*includeSegmentationMask=*/ i === 0));
    callbackRouterRemote.objectsReceived(objects);
    return flushTasks();
  }

  function getRenderedObjects() {
    return selectionOverlayElement.$.objectSelectionLayer
        .getObjectNodesForTesting();
  }

  async function verifyLastReceievedObjectRequest(
      expectedRegion: CenterRotatedBox, expectedIsMaskClick: boolean) {
    await testBrowserProxy.handler.whenCalled('issueLensObjectRequest');
    const requestRegion =
        testBrowserProxy.handler.getArgs('issueLensObjectRequest')[0][0];
    const isMaskClick =
        testBrowserProxy.handler.getArgs('issueLensObjectRequest')[0][1];
    assertBoxesWithinThreshold(expectedRegion, requestRegion);
    assertEquals(expectedIsMaskClick, isMaskClick);
  }

  test('verify that objects render on the page', () => {
    const objectsOnPage = getRenderedObjects();

    assertEquals(6, objectsOnPage.length);
  });

  test(
      `verify that tapping an object issues lens request via mojo`,
      async () => {
        await simulateClick(selectionOverlayElement, {x: 120, y: 15});

        await verifyLastReceievedObjectRequest(
            objects[1]!.geometry.boundingBox, /*expectedIsMaskClick=*/ false);
        assertEquals(1, metrics.count('Lens.Overlay.Overlay.UserAction'));
        const action = await testBrowserProxy.handler.whenCalled(
            'recordUkmAndTaskCompletionForLensOverlayInteraction');
        assertEquals(UserAction.kObjectClick, action);
        assertEquals(
            1,
            metrics.count(
                'Lens.Overlay.Overlay.UserAction', UserAction.kObjectClick));
        assertEquals(
            1,
            metrics.count(
                'Lens.Overlay.Overlay.ByInvocationSource.AppMenu.UserAction',
                UserAction.kObjectClick));
      });

  test(
      `verify that smaller objects have priority over larger objects`,
      async () => {
        await simulateClick(selectionOverlayElement, {x: 320, y: 50});

        await verifyLastReceievedObjectRequest(
            objects[4]!.geometry.boundingBox, /*expectedIsMaskClick=*/ false);
      });

  test(
      `verify that tapping on objects with masks sets the mask click flag`,
      async () => {
        // Tap on the object at index 0, which has a mask.
        await simulateClick(selectionOverlayElement, {x: 21, y: 16});

        await verifyLastReceievedObjectRequest(
            objects[0]!.geometry.boundingBox, /*expectedIsMaskClick=*/ true);
      });

  test(
      'verify that tapping an object calls closePreselectionBubble',
      async () => {
        await simulateClick(selectionOverlayElement, {x: 320, y: 50});
        await testBrowserProxy.handler.whenCalled('closePreselectionBubble');
        assertEquals(
            1,
            testBrowserProxy.handler.getCallCount('closePreselectionBubble'));
      });
});
