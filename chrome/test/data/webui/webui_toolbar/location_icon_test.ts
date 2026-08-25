// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, IconTable, IconType, LhsChipIdentifier, PointerProxyImpl, SecurityChipRole} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {IconFromTableElement, LocationIconElement, PointerProxy} from 'chrome://webui-toolbar.top-chrome/app.js';

class TestToolbarUiHandler extends TestBrowserProxy {
  constructor() {
    super(['onLhsChipMousePressed', 'onLhsChipClicked']);
  }

  onLhsChipMousePressed(id: LhsChipIdentifier, isMiddleClick: boolean) {
    this.methodCalled('onLhsChipMousePressed', [id, isMiddleClick]);
  }

  onLhsChipClicked(id: LhsChipIdentifier, isMouseInteraction: boolean) {
    this.methodCalled('onLhsChipClicked', [id, isMouseInteraction]);
  }
}

class TestPointerProxy extends TestBrowserProxy implements PointerProxy {
  constructor() {
    super(['setPointerCapture', 'releasePointerCapture']);
  }

  setPointerCapture(el: Element, pointerId: number) {
    this.methodCalled('setPointerCapture', [el, pointerId]);
  }

  releasePointerCapture(el: Element, pointerId: number) {
    this.methodCalled('releasePointerCapture', [el, pointerId]);
  }
}

suite('LocationIconTest', function() {
  let locationIcon: LocationIconElement;
  let toolbarUiHandler: TestToolbarUiHandler;
  let pointerProxy: TestPointerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    toolbarUiHandler = new TestToolbarUiHandler();
    BrowserProxyImpl.setInstance({toolbarUIHandler: toolbarUiHandler} as any);

    pointerProxy = new TestPointerProxy();
    PointerProxyImpl.setInstance(pointerProxy);

    locationIcon = document.createElement('location-icon');
    document.body.appendChild(locationIcon);
  });

  test('Render text', async function() {
    locationIcon.state = {
      icon: {handleId: 10n},
      securityLevel: 0,
      text: 'Not secure',
      tooltip: '',
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: '',
        description: '',
      },
    };
    await microtasksFinished();

    const textEl = locationIcon.shadowRoot.querySelector('#text');
    assertTrue(!!textEl);
    assertEquals('Not secure', textEl.textContent);
    assertTrue(locationIcon.hasAttribute('clickable'));
    assertFalse(locationIcon.hasAttribute('is-text-dangerous'));

    const iconContainer =
        locationIcon.shadowRoot.querySelector<IconFromTableElement>(
            'icon-from-table');
    assertTrue(!!iconContainer);
    assertEquals(10n, iconContainer.iconHandle.handleId);
  });

  test('Tooltip rendering', async function() {
    locationIcon.state = {
      icon: {handleId: 0n},
      securityLevel: 0,
      text: '',
      tooltip: 'View site information',
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: '',
        description: '',
      },
    };
    await microtasksFinished();

    const container = locationIcon.$.container;
    assertEquals('View site information', container.title);
  });

  test('Dangerous text', async function() {
    locationIcon.style.setProperty(
        '--color-omnibox-security-chip-dangerous-background', 'rgb(0, 0, 255)');
    locationIcon.style.setProperty(
        '--color-omnibox-security-chip-text', 'rgb(0, 255, 0)');

    locationIcon.state = {
      icon: {handleId: 0n},
      securityLevel: 3,  // DANGEROUS
      text: 'Dangerous',
      tooltip: '',
      isClickable: true,
      isTextDangerous: true,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: '',
        description: '',
      },
    };
    await microtasksFinished();

    assertTrue(locationIcon.hasAttribute('is-text-dangerous'));
    assertTrue(locationIcon.hasAttribute('is-dangerous'));

    const container = locationIcon.$.container;
    assertTrue(hasStyle(container, 'background-color', 'rgb(0, 0, 255)'));
    assertTrue(hasStyle(container, 'color', 'rgb(0, 255, 0)'));
  });

  test('Dangerous level, Not secure text', async function() {
    locationIcon.style.setProperty(
        '--color-omnibox-security-chip-dangerous', 'rgb(255, 0, 0)');

    locationIcon.state = {
      icon: {handleId: 0n},
      securityLevel: 3,  // DANGEROUS
      text: 'Not secure',
      tooltip: '',
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: '',
        description: '',
      },
    };
    await microtasksFinished();

    assertFalse(locationIcon.hasAttribute('is-text-dangerous'));
    assertTrue(locationIcon.hasAttribute('is-dangerous'));

    const container = locationIcon.$.container;
    assertTrue(hasStyle(container, 'color', 'rgb(255, 0, 0)'));
  });

  test('Warning text', async function() {
    locationIcon.state = {
      icon: {handleId: 0n},
      securityLevel: 4,  // WARNING
      text: 'Not secure',
      tooltip: '',
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: '',
        description: '',
      },
    };
    await microtasksFinished();

    assertFalse(locationIcon.hasAttribute('is-text-dangerous'));
    assertFalse(locationIcon.hasAttribute('is-dangerous'));
  });

  test('Unclickable state', async function() {
    locationIcon.state = {
      icon: {handleId: 0n},
      securityLevel: 0,
      text: '',
      tooltip: '',
      isClickable: false,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: '',
        description: '',
      },
    };
    await microtasksFinished();

    assertFalse(locationIcon.hasAttribute('clickable'));

    const container = locationIcon.$.container;
    container.dispatchEvent(new PointerEvent('pointerdown'));
    assertEquals(0, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));

    container.click();
    assertEquals(0, toolbarUiHandler.getCallCount('onLhsChipClicked'));
  });

  test('Click events', async function() {
    locationIcon.state = {
      icon: {handleId: 0n},
      securityLevel: 0,
      text: '',
      tooltip: '',
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: '',
        description: '',
      },
    };
    await microtasksFinished();

    const container = locationIcon.$.container;

    // Simulate normal click pointerdown
    container.dispatchEvent(new PointerEvent('pointerdown', {button: 0}));
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));
    assertEquals(
        LhsChipIdentifier.kLocationIcon,
        toolbarUiHandler.getArgs('onLhsChipMousePressed')[0][0]);
    assertFalse(toolbarUiHandler.getArgs('onLhsChipMousePressed')[0][1]);
    container.dispatchEvent(new PointerEvent('pointerup'));

    // Simulate right click pointerdown
    container.dispatchEvent(new PointerEvent('pointerdown', {button: 2}));
    assertEquals(2, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));
    assertFalse(toolbarUiHandler.getArgs('onLhsChipMousePressed')[1][1]);
    container.dispatchEvent(new PointerEvent('pointerup'));

    // Simulate middle click pointerdown with e.buttons = 4
    container.dispatchEvent(
        new PointerEvent('pointerdown', {button: 1, buttons: 4}));
    assertEquals(3, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));
    assertEquals(
        LhsChipIdentifier.kLocationIcon,
        toolbarUiHandler.getArgs('onLhsChipMousePressed')[2][0]);
    assertTrue(toolbarUiHandler.getArgs('onLhsChipMousePressed')[2][1]);
    container.dispatchEvent(new PointerEvent('pointerup'));

    container.click();
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipClicked'));
    assertEquals(
        LhsChipIdentifier.kLocationIcon,
        toolbarUiHandler.getArgs('onLhsChipClicked')[0][0]);
    assertFalse(toolbarUiHandler.getArgs('onLhsChipClicked')[0][1]);

    // Simulate mouse interaction
    const clickEvent = new PointerEvent('click', {pointerType: 'mouse'});
    container.dispatchEvent(clickEvent);
    assertEquals(2, toolbarUiHandler.getCallCount('onLhsChipClicked'));
    assertEquals(
        LhsChipIdentifier.kLocationIcon,
        toolbarUiHandler.getArgs('onLhsChipClicked')[1][0]);
    assertTrue(toolbarUiHandler.getArgs('onLhsChipClicked')[1][1]);
  });

  test('Multi-touch scenario', async function() {
    locationIcon.state = {
      icon: {handleId: 0n},
      securityLevel: 0,
      text: '',
      tooltip: '',
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: '',
        description: '',
      },
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
    };
    await microtasksFinished();

    const container = locationIcon.$.container;

    // Initial touch
    container.dispatchEvent(
        new PointerEvent('pointerdown', {pointerId: 1, button: 0}));
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));

    // Second touch while first is still active
    container.dispatchEvent(
        new PointerEvent('pointerdown', {pointerId: 2, button: 0}));
    // Should NOT trigger another mouse pressed event
    assertEquals(1, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));

    // Release first touch
    container.dispatchEvent(new PointerEvent('pointerup', {pointerId: 1}));

    // Now a new touch should work
    container.dispatchEvent(
        new PointerEvent('pointerdown', {pointerId: 3, button: 0}));
    assertEquals(2, toolbarUiHandler.getCallCount('onLhsChipMousePressed'));
  });

  test('GlowUp animation', async function() {
    IconTable.getInstance().applyUpdates([{
      handleId: 10n,
      iconUrlOrName: 'webui-toolbar:page_info_custom',
      iconType: IconType.kIconSet,
      color: null,
    }]);
    // Disable by default
    locationIcon.glowUpEnabled = false;
    locationIcon.state = {
      icon: {handleId: 10n},
      securityLevel: 2,  // kSecure
      text: '',
      tooltip: '',
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState:
          {role: SecurityChipRole.kButton, label: '', description: ''},
    };
    await microtasksFinished();

    // Should use icon-from-table
    assertTrue(!!locationIcon.shadowRoot.querySelector('icon-from-table'));
    assertFalse(!!locationIcon.shadowRoot.querySelector('cr-icon'));
    assertFalse(locationIcon.hasAttribute('glow-up-active'));

    // Enable GlowUp
    locationIcon.glowUpEnabled = true;

    // Mock setTimeout to capture the callback
    const originalSetTimeout = window.setTimeout;
    let timeoutCallback: (() => void)|null = null;
    (window as any).setTimeout = (cb: any, ms: number) => {
      if (ms === 150) {
        timeoutCallback = cb;
        return 123;  // Fake timer ID
      }
      return originalSetTimeout(cb, ms);
    };

    // Simulate bubble opening
    locationIcon.state = {
      ...locationIcon.state,
      isContextMenuVisible: true,
    };
    await microtasksFinished();

    // Should now use cr-icon with the animated forward icon
    assertFalse(!!locationIcon.shadowRoot.querySelector('icon-from-table'));
    let crIcon = locationIcon.shadowRoot.querySelector('cr-icon');
    assertTrue(!!crIcon);
    assertEquals(
        'webui-toolbar:info_glow_up_forward', crIcon.getAttribute('icon'));
    assertTrue(locationIcon.hasAttribute('glow-up-active'));

    // Trigger the timeout to finish opening animation
    assertTrue(!!timeoutCallback);
    (timeoutCallback as any)();
    timeoutCallback = null;
    await microtasksFinished();

    // After animation finishes, it should still be active because bubble is
    // showing
    assertTrue(locationIcon.hasAttribute('glow-up-active'));
    crIcon = locationIcon.shadowRoot.querySelector('cr-icon');
    assertTrue(!!crIcon);
    assertEquals(
        'webui-toolbar:info_glow_up_forward', crIcon.getAttribute('icon'));

    // Simulate bubble closing
    locationIcon.state = {
      ...locationIcon.state,
      isContextMenuVisible: false,
    };
    await microtasksFinished();

    // During closing animation, it should still be active but use reverse icon
    assertTrue(locationIcon.hasAttribute('glow-up-active'));
    crIcon = locationIcon.shadowRoot.querySelector('cr-icon');
    assertTrue(!!crIcon);
    assertEquals(
        'webui-toolbar:info_glow_up_reverse', crIcon.getAttribute('icon'));

    // Trigger the timeout to finish closing animation
    assertTrue(!!timeoutCallback);
    (timeoutCallback as any)();
    await microtasksFinished();

    // After closing animation finishes, it should go back to static icon
    assertFalse(locationIcon.hasAttribute('glow-up-active'));
    assertTrue(!!locationIcon.shadowRoot.querySelector('icon-from-table'));
    assertFalse(!!locationIcon.shadowRoot.querySelector('cr-icon'));

    // Restore
    (window as any).setTimeout = originalSetTimeout;
  });

  test('Clear timer on disconnect', async function() {
    IconTable.getInstance().applyUpdates([{
      handleId: 10n,
      iconUrlOrName: 'webui-toolbar:page_info_custom',
      iconType: IconType.kIconSet,
      color: null,
    }]);
    locationIcon.glowUpEnabled = true;
    locationIcon.state = {
      icon: {handleId: 10n},
      securityLevel: 2,  // kSecure
      text: '',
      tooltip: '',
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState:
          {role: SecurityChipRole.kButton, label: '', description: ''},
    };
    await microtasksFinished();

    const originalClearTimeout = window.clearTimeout;
    let clearTimeoutCalled = false;
    (window as any).clearTimeout = (id: any) => {
      clearTimeoutCalled = true;
      originalClearTimeout(id);
    };

    // Simulate bubble opening to start timer
    locationIcon.state = {
      ...locationIcon.state,
      isContextMenuVisible: true,
    };
    await microtasksFinished();

    // Remove from DOM to trigger disconnectedCallback
    locationIcon.remove();

    assertTrue(clearTimeoutCalled);

    // Restore
    (window as any).clearTimeout = originalClearTimeout;
  });

  test('Accessibility state properties', async function() {
    locationIcon.state = {
      icon: {handleId: 10n},
      securityLevel: 0,
      text: '',
      tooltip: '',
      isClickable: true,
      isTextDangerous: false,
      isVisible: true,
      isContextMenuVisible: false,
      accessibilityState: {
        role: SecurityChipRole.kImage,
        label: 'Search icon',
        description: 'Context description',
      },
    };
    await microtasksFinished();

    const container = locationIcon.$.container;
    assertEquals('img', container.getAttribute('role'));
    assertEquals('Search icon', container.getAttribute('aria-label'));
    assertEquals(
        'Context description', container.getAttribute('aria-description'));

    // Verify the default button role updates correctly.
    locationIcon.state = Object.assign({}, locationIcon.state, {
      accessibilityState: {
        role: SecurityChipRole.kButton,
        label: 'A label',
        description: 'A description',
      },
    });
    await microtasksFinished();
    assertEquals('button', container.getAttribute('role'));
  });
});
