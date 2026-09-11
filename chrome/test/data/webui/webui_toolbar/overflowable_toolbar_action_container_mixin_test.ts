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
      enabled: {type: Boolean},
    };
  }

  accessor action: PinnedToolbarAction = PinnedToolbarAction.kPrint;
  accessor enabled: boolean = true;

  isDivider(): boolean {
    return false;
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
});
