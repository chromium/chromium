// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://lens/side_panel/side_panel_app.js';

import type {LensSidePanelPageRemote} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import {browserProxyFactory, SidePanelResultStatus} from 'chrome-untrusted://lens/lens_side_panel.mojom-webui.js';
import type {LensSidePanelAppElement} from 'chrome-untrusted://lens/side_panel/side_panel_app.js';
import {loadTimeData} from 'chrome-untrusted://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome-untrusted://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome-untrusted://webui-test/test_util.js';

import {TestLensSidePanelPageHandler} from './test_side_panel_browser_proxy.js';

suite('ErrorPage', () => {
  let lensSidePanelElement: LensSidePanelAppElement;
  let callbackRouterRemote: LensSidePanelPageRemote;

  setup(() => {
    const {instance, remote} =
        browserProxyFactory.createForTest(new TestLensSidePanelPageHandler());
    browserProxyFactory.setInstance(instance);
    callbackRouterRemote = remote;

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
    assertFalse(isVisible(lensSidePanelElement.$.errorPage));
  });

  test('ErrorPageIsVisibleAndHiddenAfterCallback', async () => {
    callbackRouterRemote.setShowErrorPage(
        true, SidePanelResultStatus.kErrorPageShownOffline);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isVisible(lensSidePanelElement.$.errorPage));

    callbackRouterRemote.setShowErrorPage(
        false, SidePanelResultStatus.kResultShown);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isVisible(lensSidePanelElement.$.errorPage));
  });

  test('ErrorPageIsNotAffectedByLoadingState', async () => {
    callbackRouterRemote.setIsLoadingResults(true);
    callbackRouterRemote.setShowErrorPage(
        true, SidePanelResultStatus.kErrorPageShownOffline);
    await waitAfterNextRender(lensSidePanelElement);
    assertTrue(isVisible(lensSidePanelElement.$.errorPage));

    callbackRouterRemote.setShowErrorPage(
        false, SidePanelResultStatus.kResultShown);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isVisible(lensSidePanelElement.$.errorPage));
  });

  test('ErrorPageHiddenWhenDisabled', async () => {
    loadTimeData.overrideValues({'enableErrorPage': false});
    callbackRouterRemote.setShowErrorPage(
        true, SidePanelResultStatus.kErrorPageShownOffline);
    await waitAfterNextRender(lensSidePanelElement);
    assertFalse(isVisible(lensSidePanelElement.$.errorPage));
  });
});
