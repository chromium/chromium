// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/info_tooltip.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {ContextualTasksInfoTooltipElement} from 'chrome://contextual-tasks/info_tooltip.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {WindowOpenDisposition} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('InfoTooltipTest', () => {
  let tooltipElement: ContextualTasksInfoTooltipElement;
  let container: HTMLDivElement;
  let target: HTMLDivElement;
  let testProxy: TestContextualTasksBrowserProxy;

  setup(async () => {
    loadTimeData.resetForTesting({
      tabFaviconChipsToCoinsEnabled: false,
    });

    testProxy = new TestContextualTasksBrowserProxy('about:blank');
    BrowserProxyImpl.setInstance(testProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Create a positioned container.
    container = document.createElement('div');
    container.style.position = 'relative';
    container.style.width = '400px';
    container.style.height = '400px';
    document.body.appendChild(container);

    // Create a target element inside the container.
    target = document.createElement('div');
    target.style.position = 'absolute';
    target.style.bottom = '50px';
    target.style.left = '100px';
    target.style.width = '50px';
    target.style.height = '20px';
    container.appendChild(target);

    tooltipElement = document.createElement('contextual-tasks-info-tooltip');
    tooltipElement.bodyText = 'Test body content';
    container.appendChild(tooltipElement);
    await microtasksFinished();
  });

  test('shows and positions correctly left-aligned', async () => {
    tooltipElement.target = target;
    tooltipElement.container = container;
    tooltipElement.horizontalAlign = 'left';
    tooltipElement.show();
    await microtasksFinished();

    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    assertFalse(crTooltip.$.tooltip.hidden);

    assertEquals('auto', crTooltip.style.bottom);
    assertTrue(crTooltip.style.top !== '');
    assertTrue(crTooltip.style.top !== 'auto');

    // Left offset should match the target's left offset.
    const parentRect = container.getBoundingClientRect();
    const targetRect = target.getBoundingClientRect();
    const expectedLeft = targetRect.left - parentRect.left;
    assertEquals(`${expectedLeft}px`, crTooltip.style.left);
  });

  test('shows and positions correctly right-aligned', async () => {
    tooltipElement.target = target;
    tooltipElement.container = container;
    tooltipElement.horizontalAlign = 'right';
    tooltipElement.show();
    await microtasksFinished();

    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    assertFalse(crTooltip.$.tooltip.hidden);

    assertEquals('auto', crTooltip.style.bottom);
    assertTrue(crTooltip.style.top !== '');
    assertTrue(crTooltip.style.top !== 'auto');

    // Right offset calculation check.
    const parentRect = container.getBoundingClientRect();
    const targetRect = target.getBoundingClientRect();
    const expectedRight = parentRect.right - targetRect.right;
    assertEquals(`${expectedRight}px`, crTooltip.style.right);
  });

  test('renders text button and fires event on click', async () => {
    tooltipElement.titleText = 'Title';
    tooltipElement.bodyText = 'Body content';
    tooltipElement.closeButtonType = 'text';
    tooltipElement.buttonText = 'Accept';
    tooltipElement.target = target;
    tooltipElement.show();
    await microtasksFinished();

    const titleEl = tooltipElement.shadowRoot.querySelector('.tooltip-title')!;
    assertEquals('Title', titleEl.textContent.trim());

    const bodyEl = tooltipElement.shadowRoot.querySelector('.tooltip-body')!;
    assertEquals('Body content', bodyEl.textContent.trim());

    const button = tooltipElement.shadowRoot.querySelector('cr-button')!;
    assertEquals('Accept', button.textContent.trim());

    let eventFired = false;
    tooltipElement.addEventListener('tooltip-dismissed', () => {
      eventFired = true;
    });

    button.click();
    await microtasksFinished();

    assertTrue(eventFired);
    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    assertTrue(crTooltip.$.tooltip.hidden);
  });

  test('renders icon button and fires event on click', async () => {
    tooltipElement.bodyText = 'Body content';
    tooltipElement.closeButtonType = 'icon';
    tooltipElement.target = target;
    tooltipElement.show();
    await microtasksFinished();

    // Title should not render since it is empty.
    const titleEl = tooltipElement.shadowRoot.querySelector('.tooltip-title');
    assertEquals(null, titleEl);

    // Icon button should exist.
    const closeBtn = tooltipElement.shadowRoot.querySelector('#closeBtn')!;
    assertTrue(closeBtn !== null);

    let eventFired = false;
    tooltipElement.addEventListener('tooltip-dismissed', () => {
      eventFired = true;
    });

    (closeBtn as HTMLElement).click();
    await microtasksFinished();

    assertTrue(eventFired);
  });

  test('renders link and calls browser proxy on click', async () => {
    tooltipElement.bodyText = 'Body content';
    tooltipElement.linkUrl = 'https://example.com/';
    tooltipElement.linkText = 'Learn More';
    tooltipElement.target = target;
    tooltipElement.show();
    await microtasksFinished();

    const link = tooltipElement.shadowRoot.querySelector('a')!;
    assertEquals('Learn More', link.textContent.trim());
    assertEquals('https://example.com/', link.getAttribute('href'));

    link.click();
    await microtasksFinished();

    const [url, disposition] = await testProxy.handler.whenCalled('openUrl');
    assertEquals('https://example.com/', url);
    assertEquals(WindowOpenDisposition.NEW_FOREGROUND_TAB, disposition);
  });
});
