// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {LensSidePanelPageRemote} from 'chrome-untrusted://lens-overlay/lens_side_panel.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

suite('FeedbackToast', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let lensSidePanelElement: LensSidePanelAppElement;
  let callbackRouterRemote: LensSidePanelPageRemote;

  function isRendered(el: HTMLElement) {
    // isVisible only checks if the bounding client rect is not empty and
    // zeroed.
    return isVisible(el) && getComputedStyle(el).visibility !== 'hidden';
  }

  setup(() => {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    // Enable the new feedback feature.
    loadTimeData.overrideValues({'newFeedbackEnabled': true});

    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);
  });

  test('FeedbackToastDoesNotShowWhenDisabled', async () => {
    loadTimeData.overrideValues({'newFeedbackEnabled': false});
    callbackRouterRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);

    assertFalse(isRendered(lensSidePanelElement.$.feedbackToast));
  });

  test('FeedbackToastNotVisbleOnInitialization', () => {
    assertFalse(isRendered(lensSidePanelElement.$.feedbackToast));
  });

  test('ShowFeedbackToastOnLoadFinished', async () => {
    callbackRouterRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);

    assertTrue(isRendered(lensSidePanelElement.$.feedbackToast));
  });


  test('HideFeedbackToastOnNewLoad', async () => {
    callbackRouterRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);

    assertTrue(isRendered(lensSidePanelElement.$.feedbackToast));

    callbackRouterRemote.setIsLoadingResults(true);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isRendered(lensSidePanelElement.$.feedbackToast));
  });

  test('HideFeedbackToastOnCloseButtonClick', async () => {
    callbackRouterRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isRendered(lensSidePanelElement.$.feedbackToast));

    lensSidePanelElement.$.closeFeedbackToastButton.click();

    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isRendered(lensSidePanelElement.$.feedbackToast));
  });
});
