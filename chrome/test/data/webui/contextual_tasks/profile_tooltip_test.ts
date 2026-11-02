// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://contextual-tasks/profile_tooltip.js';

import type {ProfileTooltipElement} from 'chrome://contextual-tasks/profile_tooltip.js';
import type {UnboundedElement} from 'chrome://contextual-tasks/utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('ProfileTooltipTest', () => {
  let tooltip: ProfileTooltipElement;
  let anchor: HTMLElement;

  function getTooltipDiv(): UnboundedElement {
    return tooltip.$.tooltip;
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.resetForTesting({
      googleAccount: 'Google Account',
      contextualTasksUnboundedMenuEnabled: false,
    });

    anchor = document.createElement('div');
    anchor.style.cssText =
        'position: fixed; left: 100px; top: 10px; width: 20px; height: 20px;';
    document.body.appendChild(anchor);

    tooltip = document.createElement('contextual-tasks-profile-tooltip');
    tooltip.fullName = 'Jane Doe';
    tooltip.email = 'janedoe@gmail.com';
    document.body.appendChild(tooltip);
    await microtasksFinished();
  });

  test('renders header, full name, and email', () => {
    const div = getTooltipDiv();
    assertEquals('tooltip', div.getAttribute('role'));
    const title = div.querySelector<HTMLElement>('.title');
    assertTrue(!!title);
    assertEquals('Google Account', title.textContent);
    const lines = div.querySelectorAll<HTMLElement>('.text');
    assertEquals(2, lines.length);
    assertEquals('Jane Doe', lines[0]!.textContent);
    assertEquals('janedoe@gmail.com', lines[1]!.textContent);
  });

  test('omits name line when full name is empty', async () => {
    tooltip.fullName = '';
    await microtasksFinished();
    const lines = getTooltipDiv().querySelectorAll<HTMLElement>('.text');
    assertEquals(1, lines.length);
    assertEquals('janedoe@gmail.com', lines[0]!.textContent);
  });

  test('show and hide toggle visibility', async () => {
    const div = getTooltipDiv();
    assertFalse(tooltip.isOpen);
    assertEquals('hidden', getComputedStyle(div).visibility);

    await tooltip.show(anchor);
    assertTrue(tooltip.isOpen);
    assertTrue(div.hasAttribute('open'));
    assertEquals('visible', getComputedStyle(div).visibility);

    tooltip.hide();
    await microtasksFinished();
    assertFalse(tooltip.isOpen);
    assertFalse(div.hasAttribute('open'));
    assertEquals('hidden', getComputedStyle(div).visibility);
  });

  test('waits 500ms before showing on hover', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    tooltip.scheduleShow(anchor);
    mockTimer.tick(499);
    assertFalse(tooltip.isOpen);

    mockTimer.tick(1);
    mockTimer.uninstall();
    await microtasksFinished();
    assertTrue(tooltip.isOpen);
  });

  test('leaving before the show delay cancels the tooltip', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    tooltip.scheduleShow(anchor);
    mockTimer.tick(300);
    tooltip.scheduleHide();
    mockTimer.tick(500);
    mockTimer.uninstall();
    await microtasksFinished();
    assertFalse(tooltip.isOpen);
  });

  test('waits 100ms before hiding and re-entering keeps it open', async () => {
    await tooltip.show(anchor);
    const mockTimer = new MockTimer();
    mockTimer.install();

    tooltip.scheduleHide();
    mockTimer.tick(99);
    assertTrue(tooltip.isOpen);
    tooltip.scheduleShow(anchor);
    mockTimer.tick(500);
    assertTrue(tooltip.isOpen);

    tooltip.scheduleHide();
    mockTimer.tick(100);
    mockTimer.uninstall();
    await microtasksFinished();
    assertFalse(tooltip.isOpen);
  });

  test('hide cancels a pending show', async () => {
    const mockTimer = new MockTimer();
    mockTimer.install();
    tooltip.scheduleShow(anchor);
    tooltip.hide();
    mockTimer.tick(500);
    mockTimer.uninstall();
    await microtasksFinished();
    assertFalse(tooltip.isOpen);
  });

  test('positions tooltip centered below the anchor', async () => {
    await tooltip.show(anchor);
    const div = getTooltipDiv();
    const anchorRect = anchor.getBoundingClientRect();
    const tooltipRect = div.getBoundingClientRect();
    // Allow sub-pixel differences from device scale factors (e.g. Android).
    assertTrue(Math.abs(anchorRect.bottom + 4 - tooltipRect.top) < 1);
    const expectedLeft = Math.max(
        8, anchorRect.left + (anchorRect.width - tooltipRect.width) / 2);
    assertTrue(Math.abs(expectedLeft - tooltipRect.left) < 1);
  });

  test('uses unbounded element when flag is enabled', async () => {
    loadTimeData.overrideValues({contextualTasksUnboundedMenuEnabled: true});
    const div = getTooltipDiv();
    let showCalled = false;
    let hideCalled = false;
    div.showUnboundedElement = () => {
      showCalled = true;
      return Promise.resolve();
    };
    div.hideUnboundedElement = () => {
      hideCalled = true;
      return Promise.resolve();
    };

    await tooltip.show(anchor);
    assertTrue(showCalled);
    assertTrue(div.hasAttribute('unbounded'));

    tooltip.hide();
    await microtasksFinished();
    assertTrue(hideCalled);
    assertFalse(div.hasAttribute('open'));
  });

  test('does not use unbounded element when flag is disabled', async () => {
    const div = getTooltipDiv();
    let showCalled = false;
    div.showUnboundedElement = () => {
      showCalled = true;
      return Promise.resolve();
    };

    await tooltip.show(anchor);
    assertFalse(showCalled);
    assertFalse(div.hasAttribute('unbounded'));
    assertTrue(div.hasAttribute('open'));
  });

  test('closes when the unbounded element is dismissed', async () => {
    loadTimeData.overrideValues({contextualTasksUnboundedMenuEnabled: true});
    const div = getTooltipDiv();
    let hideCalled = false;
    div.showUnboundedElement = () => Promise.resolve();
    div.hideUnboundedElement = () => {
      hideCalled = true;
      return Promise.resolve();
    };
    await tooltip.show(anchor);

    const toggleEvent = new Event('beforetoggle');
    Object.assign(toggleEvent, {newState: 'closed'});
    div.dispatchEvent(toggleEvent);
    await microtasksFinished();

    assertFalse(tooltip.isOpen);
    // The platform already dismissed it, so it must not be hidden again.
    assertFalse(hideCalled);
  });
});
