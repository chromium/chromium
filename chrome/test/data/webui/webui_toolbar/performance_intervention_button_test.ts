// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl} from 'chrome://webui-toolbar.top-chrome/app.js';

class TestToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super(['onPerformanceInterventionButtonClicked']);
  }

  onPerformanceInterventionButtonClicked() {
    this.methodCalled('onPerformanceInterventionButtonClicked');
  }
}

class TestPerformanceInterventionBrowserProxy extends TestBrowserProxy {
  toolbarUIHandler: TestToolbarUiHandler;

  constructor() {
    super([]);
    this.toolbarUIHandler = new TestToolbarUiHandler();
  }
}

suite('PerformanceInterventionButton', function() {
  let button: any;
  let browserProxy: TestPerformanceInterventionBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestPerformanceInterventionBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy as any);

    button = document.createElement('performance-intervention-button');
    document.body.appendChild(button);
    await button.updateComplete;
    await microtasksFinished();
  });

  test('ClickTriggersProxy', async () => {
    assertEquals(
        0,
        browserProxy.toolbarUIHandler.getCallCount(
            'onPerformanceInterventionButtonClicked'));

    assertTrue(!!button.shadowRoot, 'shadowRoot should not be null');
    const crIconButton = button.shadowRoot?.querySelector('cr-icon-button');
    assertTrue(!!crIconButton, 'cr-icon-button should not be null');

    // Simulate click
    crIconButton.click();

    await browserProxy.toolbarUIHandler.whenCalled(
        'onPerformanceInterventionButtonClicked');
    assertEquals(
        1,
        browserProxy.toolbarUIHandler.getCallCount(
            'onPerformanceInterventionButtonClicked'));
  });

  test('ShowsTooltip', () => {
    const crIconButton = button.shadowRoot?.querySelector('cr-icon-button');
    assertTrue(!!crIconButton);
    // Tooltip and aria-label strings should be set.
    assertEquals('Performance issue alert', crIconButton.title);
    assertEquals(
        'Performance issue alert', crIconButton.getAttribute('aria-label'));
  });
});
