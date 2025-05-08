// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {LensSidePanelPageRemote} from 'chrome-untrusted://lens-overlay/lens_side_panel.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import type {CrButtonElement} from 'chrome-untrusted://resources/cr_elements/cr_button/cr_button.js';
import type {CrToastElement} from 'chrome-untrusted://resources/cr_elements/cr_toast/cr_toast.js';
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

  function getFeedbackToast(): CrToastElement {
    return lensSidePanelElement.$.feedbackToast.shadowRoot.querySelector(
        'cr-toast')!;
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

    assertFalse(isRendered(getFeedbackToast()));
  });

  test('FeedbackToastNotVisbleOnInitialization', () => {
    assertFalse(isRendered(getFeedbackToast()));
  });

  test('ShowFeedbackToastOnLoadFinished', async () => {
    callbackRouterRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);

    assertTrue(isRendered(getFeedbackToast()));
  });


  test('HideFeedbackToastOnNewLoad', async () => {
    callbackRouterRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);

    assertTrue(isRendered(getFeedbackToast()));

    callbackRouterRemote.setIsLoadingResults(true);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isRendered(getFeedbackToast()));
  });

  test('HideFeedbackToastOnCloseButtonClick', async () => {
    callbackRouterRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isRendered(getFeedbackToast()));

    // Click the close button, which should hide the feedback toast.
    const closeButton =
        lensSidePanelElement.$.feedbackToast.shadowRoot.querySelector(
            'cr-icon-button');
    assertTrue(closeButton !== null);
    closeButton.click();

    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isRendered(getFeedbackToast()));
  });

  test('SendFeedbackButtonClickCallsHandler', async () => {
    // Show the toast first.
    callbackRouterRemote.setIsLoadingResults(false);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isRendered(getFeedbackToast()));

    // Click the send feedback button.
    const sendFeedbackButton =
        lensSidePanelElement.$.feedbackToast.shadowRoot
            .querySelector<CrButtonElement>('#sendFeedbackButton');
    assertTrue(sendFeedbackButton !== null);
    sendFeedbackButton.click();

    // Verify the handler method was called and toast was hidden.
    await testBrowserProxy.handler.whenCalled('requestSendFeedback');
    assertFalse(isRendered(getFeedbackToast()));
  });
});
