// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertNotReached} from '//resources/js/assert.js';
import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {OverflowableToolbarActionContainerMixin, OverflowableToolbarActionMixin} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {OverflowableToolbarActionElement} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {OverflowMenuItem} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api.mojom-webui.js';
import {PinnedToolbarAction} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api_data_model.mojom-webui.js';

const DummyActionElementBase = OverflowableToolbarActionMixin(CrLitElement);

class DummyIconElement extends DummyActionElementBase {
  static get is() {
    return 'dummy-icon';
  }

  static override get properties() {
    return {
      action: {type: Number},
      enabled: {type: Boolean},
    };
  }

  accessor action: PinnedToolbarAction = PinnedToolbarAction.kPrint;
  accessor enabled: boolean = true;

  override getOverflowMenuItem(): OverflowMenuItem {
    return {
      id: {
        pinnedAction: this.action,
      },
      isEnabled: this.enabled,
    };
  }
}
customElements.define(DummyIconElement.is, DummyIconElement);

class DummyDividerElement extends DummyActionElementBase {
  static get is() {
    return 'dummy-divider';
  }

  override isDivider: boolean = true;

  override getOverflowMenuItem(): OverflowMenuItem {
    assertNotReached('Divider does not have overflow menu item');
  }
}
customElements.define(DummyDividerElement.is, DummyDividerElement);

const TestOverflowableToolbarActionContainerBase =
    OverflowableToolbarActionContainerMixin(CrLitElement);

class TestOverflowableToolbarActionContainerElement extends
    TestOverflowableToolbarActionContainerBase {
  static get is() {
    return 'test-overflowable-toolbar-action-container';
  }

  override render() {
    return html`
      <dummy-icon id="icon1" .action=${PinnedToolbarAction.kPrint}></dummy-icon>
      <dummy-icon id="icon2" .action=${
        PinnedToolbarAction.kClearBrowsingData} .enabled=${false}></dummy-icon>
      <dummy-divider id="divider"></dummy-divider>
      <dummy-icon id="icon3" .action=${
        PinnedToolbarAction.kShowDownloads}></dummy-icon>
    `;
  }

  override getActions(): OverflowableToolbarActionElement[] {
    return Array.from(
        this.shadowRoot.querySelectorAll<OverflowableToolbarActionElement>(
            'dummy-icon, dummy-divider'));
  }
}
customElements.define(
    TestOverflowableToolbarActionContainerElement.is,
    TestOverflowableToolbarActionContainerElement);

class TestDynamicOverflowableToolbarActionContainerElement extends
    TestOverflowableToolbarActionContainerBase {
  static get is() {
    return 'test-dynamic-overflowable-toolbar-action-container';
  }

  override render() {
    return html`
      ${
        this.items.map(
            item => item === 'divider' ?
                html`<dummy-divider id="divider"></dummy-divider>` :
                html`<dummy-icon id="${item}"></dummy-icon>`)}
    `;
  }

  static override get properties() {
    return {
      items: {type: Array},
    };
  }

  accessor items: string[] = ['icon1', 'icon2', 'divider', 'icon3'];

  override getActions(): OverflowableToolbarActionElement[] {
    return Array.from(
        this.shadowRoot.querySelectorAll<OverflowableToolbarActionElement>(
            'dummy-icon, dummy-divider'));
  }
}
customElements.define(
    TestDynamicOverflowableToolbarActionContainerElement.is,
    TestDynamicOverflowableToolbarActionContainerElement);

suite('OverflowableToolbarActionContainerMixinTest', () => {
  let control: TestOverflowableToolbarActionContainerElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    control =
        document.createElement('test-overflowable-toolbar-action-container') as
        TestOverflowableToolbarActionContainerElement;
    document.body.appendChild(control);
    await microtasksFinished();
  });

  test('getActions', () => {
    const actions = control.getActions();
    assertEquals(4, actions.length);
    assertEquals('icon1', actions[0]!.id);
    assertEquals('icon2', actions[1]!.id);
    assertEquals('divider', actions[2]!.id);
    assertEquals('icon3', actions[3]!.id);
  });

  test('setToMinWidth', () => {
    control.setToMinWidth();
    for (const el of control.getActions()) {
      assertTrue(el.classList.contains('overflow-display-none'));
    }
  });

  test('setToMinWidth with preventOverflow', () => {
    const actions = control.getActions();
    const icon1 = actions[0]! as DummyIconElement;
    const icon2 = actions[1]! as DummyIconElement;
    const icon3 = actions[3]! as DummyIconElement;

    // If `icon3` is always visible, only `icon3` should be visible.
    icon3.preventOverflow = true;
    control.setToMinWidth();
    assertTrue(actions[0]!.classList.contains('overflow-display-none'));
    assertTrue(actions[1]!.classList.contains('overflow-display-none'));
    assertTrue(actions[2]!.classList.contains('overflow-display-none'));
    assertFalse(actions[3]!.classList.contains('overflow-display-none'));

    // If `icon2` is always visible, the divider after it should be visible as
    // well.
    icon2.preventOverflow = true;
    icon3.preventOverflow = false;
    control.setToMinWidth();
    assertTrue(actions[0]!.classList.contains('overflow-display-none'));
    assertFalse(actions[1]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertTrue(actions[3]!.classList.contains('overflow-display-none'));

    // If only `icon1` is always visible, the divider should also be visible,
    // since there's a prevent-overflow action to its left, with no divider in
    // between.
    icon1.preventOverflow = true;
    icon2.preventOverflow = false;
    control.setToMinWidth();
    assertFalse(actions[0]!.classList.contains('overflow-display-none'));
    assertTrue(actions[1]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertTrue(actions[3]!.classList.contains('overflow-display-none'));
  });

  test('setToPreferredWidth with enough space', () => {
    control.setToMinWidth();
    control.setToPreferredWidth();
    for (const el of control.getActions()) {
      assertFalse(el.classList.contains('overflow-display-none'));
    }
  });

  test('expandUpToPreferredWidth with limited space', () => {
    // For the first part of this test, run out of space when hiding element 1,
    // which is the third from the back. Since element 2 is the divider, and
    // thus grouped with element 1, only element 3 (the last) will be hidden.

    let mockHost = {
      getAvailableWidth: () => {
        const actions = control.getActions();
        return actions[1]!.classList.contains('overflow-display-none') ? 100 :
                                                                         -10;
      },
    };
    Object.defineProperty(control, 'getRootNode', {
      value: () => ({host: mockHost}),
      configurable: true,
    });

    control.setToMinWidth();
    control.expandUpToPreferredWidth();

    const actions = control.getActions();
    assertEquals('icon1', actions[0]!.id);
    assertEquals('icon2', actions[1]!.id);
    assertEquals('divider', actions[2]!.id);
    assertEquals('icon3', actions[3]!.id);

    assertFalse(actions[3]!.classList.contains('overflow-display-none'));
    assertTrue(actions[2]!.classList.contains('overflow-display-none'));
    assertTrue(actions[1]!.classList.contains('overflow-display-none'));
    assertTrue(actions[0]!.classList.contains('overflow-display-none'));

    // For the second part of this test, only run out of space when trying to
    // show element 0.
    // This time, elements 1 through 3 should all be shown.

    mockHost = {
      getAvailableWidth: () => {
        const currentActions = control.getActions();
        return currentActions[0]!.classList.contains('overflow-display-none') ?
            100 :
            -10;
      },
    };

    control.setToMinWidth();
    control.expandUpToPreferredWidth();

    assertFalse(actions[3]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertFalse(actions[1]!.classList.contains('overflow-display-none'));
    assertTrue(actions[0]!.classList.contains('overflow-display-none'));
  });

  test('expandUpToPreferredWidth with preventOverflow', () => {
    const actions = control.getActions();
    const icon2 = actions[1]! as DummyIconElement;
    icon2.preventOverflow = true;

    // Simulate no available space to expand the pinned actions.
    let mockHost = {
      getAvailableWidth: () => -10,
    };
    Object.defineProperty(control, 'getRootNode', {
      value: () => ({host: mockHost}),
      configurable: true,
    });

    control.setToMinWidth();

    // At min width, `icon2` (`actions[1]`) and its preceding divider
    // (`actions[2]`) should be visible.
    assertTrue(actions[0]!.classList.contains('overflow-display-none'));
    assertFalse(actions[1]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertTrue(actions[3]!.classList.contains('overflow-display-none'));

    control.expandUpToPreferredWidth();

    // With no available space to expand, exactly the same elements as before
    // should be visible. No elements should be hidden, no new elements should
    // be shown.
    assertTrue(actions[0]!.classList.contains('overflow-display-none'));
    assertFalse(actions[1]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertTrue(actions[3]!.classList.contains('overflow-display-none'));

    // Simulate ample available space to expand all controls.
    mockHost = {
      getAvailableWidth: () => 100,
    };

    control.expandUpToPreferredWidth();

    // With sufficient space, all elements should be visible.
    assertFalse(actions[0]!.classList.contains('overflow-display-none'));
    assertFalse(actions[1]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertFalse(actions[3]!.classList.contains('overflow-display-none'));
  });

  test('expandUpToPreferredWidth skips visible divider', () => {
    const actions = control.getActions();
    const icon1 = actions[0]! as DummyIconElement;
    icon1.preventOverflow = true;

    // Only allow one additional element to be shown beyond those shown at
    // min width (`icon1` and the divider).
    const mockHost = {
      getAvailableWidth: () => {
        const hiddenCount =
            control.getActions()
                .filter(a => a.classList.contains('overflow-display-none'))
                .length;
        return hiddenCount >= 1 ? 100 : -10;
      },
    };
    Object.defineProperty(control, 'getRootNode', {
      value: () => ({host: mockHost}),
      configurable: true,
    });

    control.setToMinWidth();
    // `icon1` and the divider after it are visible.
    assertFalse(actions[0]!.classList.contains('overflow-display-none'));
    assertTrue(actions[1]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertTrue(actions[3]!.classList.contains('overflow-display-none'));

    control.expandUpToPreferredWidth();
    // `icon3` should be shown. The already-visible divider should be skipped,
    // and `icon2` doesn't fit.
    assertFalse(actions[0]!.classList.contains('overflow-display-none'));
    assertTrue(actions[1]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertFalse(actions[3]!.classList.contains('overflow-display-none'));
  });

  test('setToMinWidth only shows nearest divider', async () => {
    const dynamicControl =
        document.createElement(
            'test-dynamic-overflowable-toolbar-action-container') as
        TestDynamicOverflowableToolbarActionContainerElement;
    document.body.appendChild(dynamicControl);
    dynamicControl.items = ['icon1', 'divider', 'icon2', 'divider', 'icon3'];
    await microtasksFinished();

    const actions = dynamicControl.getActions();
    assertEquals(5, actions.length);
    (actions[0]! as DummyIconElement).preventOverflow = true;

    dynamicControl.setToMinWidth();
    // Only the first divider has a prevent-overflow action to its left without
    // an intervening divider.
    assertFalse(actions[0]!.classList.contains('overflow-display-none'));
    assertFalse(actions[1]!.classList.contains('overflow-display-none'));
    assertTrue(actions[2]!.classList.contains('overflow-display-none'));
    assertTrue(actions[3]!.classList.contains('overflow-display-none'));
    assertTrue(actions[4]!.classList.contains('overflow-display-none'));
  });

  test('controlsToAddToOverflowMenu', () => {
    // Hide all elements.
    control.setToMinWidth();

    // Make one icon visible on the toolbar, leaving 2 icons overflowed
    const actions = control.getActions();
    actions[3]!.classList.remove('overflow-display-none');

    // The other two elements should be returned by
    // controlsToAddToOverflowMenu().
    const items = control.controlsToAddToOverflowMenu();
    assertEquals(2, items.length);
    assertEquals(PinnedToolbarAction.kPrint, items[0]!.id.pinnedAction);
    assertTrue(items[0]!.isEnabled);
    assertEquals(
        PinnedToolbarAction.kClearBrowsingData, items[1]!.id.pinnedAction);
    assertFalse(items[1]!.isEnabled);

    // Show all elements. No elements should be returned by
    // controlsToAddToOverflowMenu().
    actions[2]!.classList.remove('overflow-display-none');
    actions[1]!.classList.remove('overflow-display-none');
    actions[0]!.classList.remove('overflow-display-none');
    assertEquals(0, control.controlsToAddToOverflowMenu().length);
  });

  test('request-layout tests', async () => {
    const dynamicControl =
        document.createElement(
            'test-dynamic-overflowable-toolbar-action-container') as
        TestDynamicOverflowableToolbarActionContainerElement;
    document.body.appendChild(dynamicControl);
    await microtasksFinished();

    let layoutFiredCount = 0;
    dynamicControl.addEventListener('request-layout', () => {
      layoutFiredCount++;
    });

    // Trigger a re-render without changing elements or their order. No layout
    // should be requested.
    dynamicControl.requestUpdate();
    await microtasksFinished();
    assertEquals(0, layoutFiredCount);

    // Change order of divider/icon elements. A layout should be requested.
    layoutFiredCount = 0;
    dynamicControl.items = ['icon1', 'divider', 'icon2', 'icon3'];
    await microtasksFinished();
    assertEquals(1, layoutFiredCount);

    // Change order of elements, swapping two icons. No layout should be
    // requested, since elements are reused. This is fine, since all icons are
    // the same size.
    layoutFiredCount = 0;
    dynamicControl.items = ['icon3', 'divider', 'icon2', 'icon1'];
    await microtasksFinished();
    assertEquals(0, layoutFiredCount);

    // Removing one element should trigger a layout.
    layoutFiredCount = 0;
    dynamicControl.items = ['icon3', 'divider', 'icon2'];
    await microtasksFinished();
    assertEquals(1, layoutFiredCount);
  });

  test('request-layout on preventOverflow change', async () => {
    const icon1 = control.getActions()[0]! as DummyIconElement;
    assertFalse(icon1.preventOverflow);

    let layoutFiredCount = 0;
    control.addEventListener('request-layout', () => {
      layoutFiredCount++;
    });

    // Setting `preventOverflow` to its current value should not request a
    // layout.
    icon1.preventOverflow = false;
    await microtasksFinished();
    assertEquals(0, layoutFiredCount);

    // Changing `preventOverflow` from false to true should request a layout.
    icon1.preventOverflow = true;
    await microtasksFinished();
    assertEquals(1, layoutFiredCount);

    // Changing `preventOverflow` from true to false should request a layout.
    layoutFiredCount = 0;
    icon1.preventOverflow = false;
    await microtasksFinished();
    assertEquals(1, layoutFiredCount);
  });
});
