// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertNotReached} from '//resources/js/assert.js';
import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {OverflowableToolbarActionContainerMixin} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {OverflowableToolbarAction} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {OverflowMenuItem} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api.mojom-webui.js';
import {PinnedToolbarAction} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api_data_model.mojom-webui.js';

class DummyIconElement extends CrLitElement implements
    OverflowableToolbarAction {
  static get is() {
    return 'dummy-icon';
  }

  static override get properties() {
    return {
      action: {type: Number},
      shouldPreventOverflow: {type: Boolean},
      enabled: {type: Boolean},
    };
  }

  accessor action: PinnedToolbarAction = PinnedToolbarAction.kPrint;
  accessor shouldPreventOverflow: boolean = false;
  accessor enabled: boolean = true;

  isDivider(): boolean {
    return false;
  }

  preventOverflow(): boolean {
    return this.shouldPreventOverflow;
  }

  getOverflowMenuItem(): OverflowMenuItem {
    return {
      id: {
        pinnedAction: this.action,
      },
      isEnabled: this.enabled,
    };
  }
}
customElements.define(DummyIconElement.is, DummyIconElement);

class DummyDividerElement extends CrLitElement implements
    OverflowableToolbarAction {
  static get is() {
    return 'dummy-divider';
  }

  isDivider(): boolean {
    return true;
  }

  preventOverflow(): boolean {
    return false;
  }

  getOverflowMenuItem(): OverflowMenuItem {
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

  override getActions(): Array<CrLitElement&OverflowableToolbarAction> {
    return Array.from(
        this.shadowRoot
            .querySelectorAll<CrLitElement&OverflowableToolbarAction>(
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

  override getActions(): Array<CrLitElement&OverflowableToolbarAction> {
    return Array.from(
        this.shadowRoot
            .querySelectorAll<CrLitElement&OverflowableToolbarAction>(
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
    icon3.shouldPreventOverflow = true;
    control.setToMinWidth();
    assertTrue(actions[0]!.classList.contains('overflow-display-none'));
    assertTrue(actions[1]!.classList.contains('overflow-display-none'));
    assertTrue(actions[2]!.classList.contains('overflow-display-none'));
    assertFalse(actions[3]!.classList.contains('overflow-display-none'));

    // If `icon2` is always visible, the divider after it should be visible as
    // well.
    icon2.shouldPreventOverflow = true;
    icon3.shouldPreventOverflow = false;
    control.setToMinWidth();
    assertTrue(actions[0]!.classList.contains('overflow-display-none'));
    assertFalse(actions[1]!.classList.contains('overflow-display-none'));
    assertFalse(actions[2]!.classList.contains('overflow-display-none'));
    assertTrue(actions[3]!.classList.contains('overflow-display-none'));

    // For the sake of completeness, check the case where only `icon1` is
    // visible.
    icon1.shouldPreventOverflow = true;
    icon2.shouldPreventOverflow = false;
    control.setToMinWidth();
    assertFalse(actions[0]!.classList.contains('overflow-display-none'));
    assertTrue(actions[1]!.classList.contains('overflow-display-none'));
    assertTrue(actions[2]!.classList.contains('overflow-display-none'));
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
    icon2.shouldPreventOverflow = true;

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

  // Verifies whether a layout is requested or not on updated(), given the
  // initial overflow state, visibility state, and new visibility state.
  async function runshouldPreventOverflowLayoutTestCase(
      initiallyOverflowed: boolean, oldshouldPreventOverflow: boolean,
      newshouldPreventOverflow: boolean, expectLayout: boolean) {
    const actions = control.getActions();
    const icon1 = actions[0]! as DummyIconElement;

    // Set initial overflow and shouldPreventOverflow states.
    icon1.shouldPreventOverflow = oldshouldPreventOverflow;
    if (initiallyOverflowed) {
      control.setToMinWidth();
    } else {
      control.setToPreferredWidth();
    }
    assertEquals(
        initiallyOverflowed, icon1.classList.contains('overflow-display-none'));

    // Run initial update to populate WeakMap and state in control.
    control.requestUpdate();
    await microtasksFinished();

    let layoutFiredCount = 0;
    const listener = () => {
      layoutFiredCount++;
    };
    control.addEventListener('request-layout', listener);

    // Transition to newshouldPreventOverflow state.
    icon1.shouldPreventOverflow = newshouldPreventOverflow;
    control.requestUpdate();
    await microtasksFinished();

    control.removeEventListener('request-layout', listener);

    assertEquals(
        expectLayout ? 1 : 0, layoutFiredCount,
        `Scenario failed for initiallyOverflowed=${initiallyOverflowed}, ` +
            `oldshouldPreventOverflow=${oldshouldPreventOverflow}, ` +
            `newshouldPreventOverflow=${newshouldPreventOverflow}`);
  }

  test('request-layout shouldPreventOverflow scenarios', async () => {
    // 1. `shouldPreventOverflow` false -> true while overflowed => layout
    // expected.
    await runshouldPreventOverflowLayoutTestCase(
        /*initiallyOverflowed=*/ true,
        /*oldshouldPreventOverflow=*/ false,
        /*newshouldPreventOverflow=*/ true,
        /*expectLayout=*/ true);

    // 2. `shouldPreventOverflow` false -> true while visible => no layout
    // expected.
    await runshouldPreventOverflowLayoutTestCase(
        /*initiallyOverflowed=*/ false,
        /*oldshouldPreventOverflow=*/ false,
        /*newshouldPreventOverflow=*/ true,
        /*expectLayout=*/ false);

    // 3. `shouldPreventOverflow` true -> false while visible => layout
    // expected.
    await runshouldPreventOverflowLayoutTestCase(
        /*initiallyOverflowed=*/ false,
        /*oldshouldPreventOverflow=*/ true,
        /*newshouldPreventOverflow=*/ false,
        /*expectLayout=*/ true);

    // 4. `shouldPreventOverflow` true -> true while visible => no layout
    // expected.
    await runshouldPreventOverflowLayoutTestCase(
        /*initiallyOverflowed=*/ false,
        /*oldshouldPreventOverflow=*/ true,
        /*newshouldPreventOverflow=*/ true,
        /*expectLayout=*/ false);

    // 5. `shouldPreventOverflow` false -> false while overflowed => no layout
    // expected.
    await runshouldPreventOverflowLayoutTestCase(
        /*initiallyOverflowed=*/ true,
        /*oldshouldPreventOverflow=*/ false,
        /*newshouldPreventOverflow=*/ false,
        /*expectLayout=*/ false);

    // 6. `shouldPreventOverflow` false -> false while visible => no layout
    // expected.
    await runshouldPreventOverflowLayoutTestCase(
        /*initiallyOverflowed=*/ false,
        /*oldshouldPreventOverflow=*/ false,
        /*newshouldPreventOverflow=*/ false,
        /*expectLayout=*/ false);
  });
});
