// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/app.js';
import 'chrome://contextual-tasks/onboarding_tooltip.js';

import type {ContextualTasksOnboardingTooltipElement} from 'chrome://contextual-tasks/onboarding_tooltip.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('OnboardingTooltipTest', () => {
  let tooltipElement: ContextualTasksOnboardingTooltipElement;
  let container: HTMLDivElement;
  let target: HTMLDivElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({
      composeboxShowOnboardingTooltipSessionImpressionCap: 3,
      isOnboardingTooltipDismissCountBelowCap: true,
      composeboxShowOnboardingTooltipImpressionDelay: 0,
      showOnboardingTooltip: true,
      tabFaviconChipsToCoinsEnabled: false,
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

    tooltipElement = document.createElement('contextual-tasks-onboarding-tooltip');
    container.appendChild(tooltipElement);
    await microtasksFinished();
  });

  test('shows and positions correctly', async () => {

    tooltipElement.updateTooltipVisibility(true, target, container);
    await microtasksFinished();

    assertTrue(tooltipElement.shouldShow);

    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    assertEquals('auto', crTooltip.style.bottom);
    assertTrue(crTooltip.style.top !== '');
    assertTrue(crTooltip.style.top !== 'auto');
  });

  test('hides tooltip when auto tab chip is removed', async () => {

    // Show tooltip.
    tooltipElement.updateTooltipVisibility(true, target, container);
    await microtasksFinished();
    assertTrue(tooltipElement.shouldShow);

    // Remove auto tab chip.
    tooltipElement.updateTooltipVisibility(false, target, container);
    await microtasksFinished();

    assertTrue(!tooltipElement.shouldShow);
  });



  test('positions correctly and resets bottom', async () => {
    tooltipElement.target = target;
    await microtasksFinished();

    // Manually set a bottom style to simulate the buggy state
    // (where CrTooltip fit-to-visible-bounds might have set bottom).
    const crTooltip = tooltipElement.shadowRoot.querySelector('cr-tooltip')!;
    crTooltip.style.bottom = '10px';

    // Call updatePosition which should reset bottom and set top.
    tooltipElement.updatePosition();
    await microtasksFinished();

    // Verify bottom is reset to auto.
    assertEquals('auto', crTooltip.style.bottom);

    // Verify top is set (should be positive/negative depending on layout,
    // but not empty and not 'auto').
    assertTrue(crTooltip.style.top !== '');
    assertTrue(crTooltip.style.top !== 'auto');
  });
});
