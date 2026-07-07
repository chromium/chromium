// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/lens_search_tooltip.js';

import type {ContextualTasksLensSearchTooltipElement} from 'chrome://contextual-tasks/lens_search_tooltip.js';
import {BrowserProxyImpl} from 'chrome://contextual-tasks/contextual_tasks_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';


import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestContextualTasksBrowserProxy} from './test_contextual_tasks_browser_proxy.js';

suite('LensSearchTooltipTest', () => {
  let tooltipElement: ContextualTasksLensSearchTooltipElement;
  let container: HTMLDivElement;
  let target: HTMLDivElement;
  let browserProxy: TestContextualTasksBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestContextualTasksBrowserProxy('http://example.com');

    BrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.resetForTesting({
      isLensSearchTooltipDismissCountBelowCap: true,
      isOnboardingTooltipDismissCountBelowCap: false,
      lensSearchTooltipSessionImpressionCap: 1,
    });



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

    tooltipElement = document.createElement('contextual-tasks-lens-search-tooltip');
    container.appendChild(tooltipElement);
    await microtasksFinished();
  });

  test('shows and positions correctly', async () => {
    tooltipElement.target = target;
    tooltipElement.show(container, target); // Target as composebox for simplicity
    await microtasksFinished();

    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;

    assertFalse(crTooltip.$.tooltip.hidden);

    assertEquals('auto', crTooltip.style.bottom);
    assertTrue(crTooltip.style.top !== '');
    assertTrue(crTooltip.style.top !== 'auto');
  });

  test('clicking accept button calls proxy', async () => {
    tooltipElement.target = target;
    tooltipElement.show(container, target);
    await microtasksFinished();

    const button = tooltipElement.shadowRoot.querySelector('cr-button')!;

    button.click();

    await browserProxy.handler.whenCalled('lensSearchTooltipDismissed');
    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;

    assertTrue(crTooltip.$.tooltip.hidden);

  });

  test('shows and positions correctly via updateTooltipVisibility', async () => {
    const mockComposebox = document.createElement('div') as any;
    mockComposebox.getLensButtonElement = () => target;

    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();

    assertTrue(tooltipElement.shouldShow);

    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    assertFalse(crTooltip.$.tooltip.hidden);
    assertEquals('auto', crTooltip.style.bottom);
    assertTrue(crTooltip.style.top !== '');
    assertTrue(crTooltip.style.top !== 'auto');
  });

  test('hides tooltip when lens button is removed', async () => {
    let hasButton = true;
    const mockComposebox = document.createElement('div') as any;
    mockComposebox.getLensButtonElement = () => hasButton ? target : null;

    // Show tooltip.
    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();
    assertTrue(tooltipElement.shouldShow);

    // Remove button.
    hasButton = false;
    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();

    assertFalse(tooltipElement.shouldShow);
    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    assertTrue(crTooltip.$.tooltip.hidden);
  });

  test('does not show if lens tooltip is capped', async () => {
    loadTimeData.resetForTesting({
      isLensSearchTooltipDismissCountBelowCap: false,
      isOnboardingTooltipDismissCountBelowCap: false,
      lensSearchTooltipSessionImpressionCap: 1,
    });


    const mockComposebox = document.createElement('div') as any;
    mockComposebox.getLensButtonElement = () => target;

    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();

    assertFalse(tooltipElement.shouldShow);
  });

  test('does not show if onboarding tooltip is active', async () => {
    loadTimeData.resetForTesting({
      isLensSearchTooltipDismissCountBelowCap: true,
      isOnboardingTooltipDismissCountBelowCap: true,
      lensSearchTooltipSessionImpressionCap: 1,
    });


    const mockComposebox = document.createElement('div') as any;
    mockComposebox.getLensButtonElement = () => target;

    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();

    assertFalse(tooltipElement.shouldShow);
  });

  test('does not show again in same session if cap is reached', async () => {
    // Cap is 1 (from setup)
    const mockComposebox = document.createElement('div') as any;
    mockComposebox.getLensButtonElement = () => target;

    // First show
    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();
    assertTrue(tooltipElement.shouldShow);

    // Hide it (e.g. target lost)
    mockComposebox.getLensButtonElement = () => null;
    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();
    assertFalse(tooltipElement.shouldShow);

    // Try to show again (target back)
    mockComposebox.getLensButtonElement = () => target;
    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();

    // Should NOT show because session cap of 1 is reached
    assertFalse(tooltipElement.shouldShow);
  });

  test('dismissal prevents showing again even if session cap is not reached', async () => {
    // Set cap to 3
    loadTimeData.resetForTesting({
      isLensSearchTooltipDismissCountBelowCap: true,
      isOnboardingTooltipDismissCountBelowCap: false,
      lensSearchTooltipSessionImpressionCap: 3,
    });

    const mockComposebox = document.createElement('div') as any;
    mockComposebox.getLensButtonElement = () => target;

    // First show (imp 1)
    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();
    assertTrue(tooltipElement.shouldShow);

    // Simulate dismissal
    const button = tooltipElement.shadowRoot.querySelector('cr-button')!;
    button.click();
    await microtasksFinished();
    assertFalse(tooltipElement.shouldShow);

    // Try to show again (target is still there)
    tooltipElement.updateTooltipVisibility(container, mockComposebox);
    await microtasksFinished();

    // Should NOT show because it was dismissed, even though showCount (1) < cap (3)
    assertFalse(tooltipElement.shouldShow);
  });
});


