// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {LensSidePanelPageRemote} from 'chrome-untrusted://lens/lens.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {SidePanelBrowserProxyImpl} from 'chrome-untrusted://lens/side_panel/side_panel_browser_proxy.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelBrowserProxy} from './test_side_panel_browser_proxy.js';

suite('ErrorPage', () => {
  let testBrowserProxy: TestLensSidePanelBrowserProxy;
  let lensSidePanelElement: LensSidePanelAppElement;
  let callbackRouterRemote: LensSidePanelPageRemote;

  setup(() => {
    testBrowserProxy = new TestLensSidePanelBrowserProxy();
    callbackRouterRemote =
        testBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    SidePanelBrowserProxyImpl.setInstance(testBrowserProxy);

    // Turn off the shimmer. Since the shimmer is resource intensive, turn off
    // to prevent from causing issues in the tests. Also force enable the error
    // page to prevent failures due to changes in the default flag value.
    loadTimeData.overrideValues(
        {'enableShimmer': false, 'enableErrorPage': true});

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    lensSidePanelElement = document.createElement('lens-side-panel-app');
    document.body.appendChild(lensSidePanelElement);
    return waitAfterNextRender(lensSidePanelElement);
  });

  test('ErrorPageIsNotVisibleByDefault', () => {
    assertFalse(isVisible(lensSidePanelElement.$.networkErrorPage));
  });

  test('ErrorPageIsVisibleAndHiddenAfterCallback', async () => {
    callbackRouterRemote.setShowErrorPage(true);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isVisible(lensSidePanelElement.$.networkErrorPage));

    callbackRouterRemote.setShowErrorPage(false);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isVisible(lensSidePanelElement.$.networkErrorPage));
  });

  test('ErrorPageIsNotAffectedByLoadingState', async () => {
    callbackRouterRemote.setIsLoadingResults(true);
    callbackRouterRemote.setShowErrorPage(true);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isVisible(lensSidePanelElement.$.networkErrorPage));

    callbackRouterRemote.setShowErrorPage(false);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isVisible(lensSidePanelElement.$.networkErrorPage));
  });

  test('ErrorPageHiddenWhenDisabled', async () => {
    loadTimeData.overrideValues({'enableErrorPage': false});
    callbackRouterRemote.setShowErrorPage(true);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isVisible(lensSidePanelElement.$.networkErrorPage));
  });
});
