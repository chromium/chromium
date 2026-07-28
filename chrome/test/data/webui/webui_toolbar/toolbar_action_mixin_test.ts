// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {ToolbarActionMixin, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ToolbarActionMixinInterface} from 'chrome://webui-toolbar.top-chrome/app.js';

interface ActionState {
  id: string|null;
  secondaryId?: string;
}

const DefaultActionBase =
    ToolbarActionMixin(CrLitElement, {id: 'default'} as ActionState);

/**
 * Minimal action element using default mixin implementations, used to verify
 * that unoverridden identifier methods return undefined.
 */
class DefaultActionElement extends DefaultActionBase {
  static get is() {
    return 'default-action';
  }
}

customElements.define(DefaultActionElement.is, DefaultActionElement);

const TestActionBase =
    ToolbarActionMixin(CrLitElement, {id: null} as ActionState);

/**
 * Action element that intercepts help bubble registrations and exposes
 * callback options to verify element tracking, animation waiting, aborts,
 * and state updates.
 */
class TestActionElement extends TestActionBase {
  static get is() {
    return 'test-action';
  }

  registerCalls: Array<[string, any, any]> = [];
  unregisterCalls: string[] = [];
  lastRegisterOptions?: any;

  override getElementId(state: ActionState): string|undefined {
    return state.id ?? undefined;
  }

  override getSecondaryElementId(): string|undefined {
    return this.state.secondaryId ?? undefined;
  }

  override registerHelpBubble(nativeId: string, trackable: any, options: any):
      boolean {
    this.registerCalls.push([nativeId, trackable, options]);
    this.lastRegisterOptions = options;
    return super.registerHelpBubble(nativeId, trackable, options);
  }

  override unregisterHelpBubble(nativeId: string): void {
    this.unregisterCalls.push(nativeId);
    super.unregisterHelpBubble(nativeId);
  }

  override render() {
    return html`<div id="button">Action</div>`;
  }
}

customElements.define(TestActionElement.is, TestActionElement);

declare global {
  interface HTMLElementTagNameMap {
    'default-action': DefaultActionElement;
    'test-action': TestActionElement;
  }
}

suite('ToolbarActionMixinTest', function() {
  let element: TestActionElement;
  let startTrackingCalls: Array<[HTMLElement, string]>;
  let stopTrackingCalls: HTMLElement[];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    startTrackingCalls = [];
    stopTrackingCalls = [];

    const mockManager = {
      startTracking: (el: HTMLElement, nativeId: string) => {
        startTrackingCalls.push([el, nativeId]);
      },
      stopTracking: (el: HTMLElement) => {
        stopTrackingCalls.push(el);
      },
    };
    TrackedElementManager.setInstance(mockManager as any);

    element = document.createElement('test-action');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('DefaultMethodsReturnUndefined', function() {
    const defaultEl = document.createElement('default-action');
    assertEquals(undefined, defaultEl.getElementId({id: 'test'}));
    assertEquals(undefined, defaultEl.getSecondaryElementId());
  });

  test('RegisterAndUnregisterOnStateChange', async function() {
    const interfaceCheck: ToolbarActionMixinInterface<ActionState> = element;
    assertTrue(!!interfaceCheck);
    assertEquals(0, element.registerCalls.length);
    assertEquals(0, startTrackingCalls.length);

    element.state = {id: 'action-1', secondaryId: 'sec-1'};
    await microtasksFinished();

    assertEquals(1, element.registerCalls.length);
    assertEquals('action-1', element.registerCalls[0]![0]);
    assertEquals('sec-1', element.registerCalls[0]![2].secondaryId);
    assertEquals(1, startTrackingCalls.length);
    assertEquals(element, startTrackingCalls[0]![0]);
    assertEquals('action-1', startTrackingCalls[0]![1]);

    element.state = {id: 'action-2'};
    await microtasksFinished();

    assertEquals(1, element.unregisterCalls.length);
    assertEquals('action-1', element.unregisterCalls[0]);
    assertEquals(2, element.registerCalls.length);
    assertEquals('action-2', element.registerCalls[1]![0]);
    assertEquals(undefined, element.registerCalls[1]![2].secondaryId);
    assertEquals(1, stopTrackingCalls.length);
    assertEquals(2, startTrackingCalls.length);

    element.state = {id: null};
    await microtasksFinished();

    assertEquals(2, element.unregisterCalls.length);
    assertEquals('action-2', element.unregisterCalls[1]);
  });

  test('UnregistersOnDisconnect', async function() {
    element.state = {id: 'action-1'};
    await microtasksFinished();

    assertEquals(1, element.registerCalls.length);
    element.remove();
    assertEquals(1, stopTrackingCalls.length);
    assertEquals(element, stopTrackingCalls[0]);
  });

  test('WaitsForFiniteAnimationsBeforeRegistering', async function() {
    const animation =
        element.animate([{opacity: 0}, {opacity: 1}], {duration: 500});
    element.state = {id: 'animated-id'};
    await microtasksFinished();

    assertEquals(0, element.registerCalls.length);

    animation.finish();
    await microtasksFinished();

    assertEquals(1, element.registerCalls.length);
    assertEquals('animated-id', element.registerCalls[0]![0]);
  });

  test('IgnoresInfiniteAnimationsWhenRegistering', async function() {
    const animation = element.animate(
        [{opacity: 0}, {opacity: 1}], {duration: 500, iterations: Infinity});
    element.state = {id: 'infinite-id'};
    await microtasksFinished();

    assertEquals(1, element.registerCalls.length);
    assertEquals('infinite-id', element.registerCalls[0]![0]);
    animation.cancel();
  });

  test('AbortsRegistrationIfDisconnectedDuringAnimation', async function() {
    const animation =
        element.animate([{opacity: 0}, {opacity: 1}], {duration: 500});
    element.state = {id: 'aborted-id'};
    await microtasksFinished();
    assertEquals(0, element.registerCalls.length);

    // Disconnecting the element should abort registration.
    element.remove();

    animation.finish();
    await microtasksFinished();

    assertEquals(0, element.registerCalls.length);
  });

  test('AbortsRegistrationIfStateChangesDuringAnimation', async function() {
    const animation =
        element.animate([{opacity: 0}, {opacity: 1}], {duration: 500});

    // This registration will be aborted because it changes before animations
    // complete.
    element.state = {id: 'id-1'};
    await microtasksFinished();
    assertEquals(0, element.registerCalls.length);

    element.state = {id: 'id-2'};
    await microtasksFinished();
    assertEquals(0, element.registerCalls.length);

    animation.finish();
    await microtasksFinished();

    assertEquals(1, element.registerCalls.length);
    assertEquals('id-2', element.registerCalls[0]![0]);
  });

  test('HelpBubbleCallbacksAndUpdateState', async function() {
    element.state = {id: 'action-1'};
    await microtasksFinished();

    const options = element.lastRegisterOptions;
    assertTrue(!!options);

    assertFalse(element.trackedHighlighted);
    options.onHighlightChanged(true);
    assertTrue(element.trackedHighlighted);
    options.onHighlightChanged(false);
    assertFalse(element.trackedHighlighted);

    assertFalse(element.hasHelpBubble);
    assertEquals(
        'Original Tooltip',
        element.adjustTooltipForHelpBubble('Original Tooltip'));

    options.onHelpBubbleShown();
    assertTrue(element.hasHelpBubble);
    // No tooltip when help bubble shown.
    assertEquals('', element.adjustTooltipForHelpBubble('Original Tooltip'));

    options.onHelpBubbleHidden();
    assertFalse(element.hasHelpBubble);
    assertEquals(
        'Original Tooltip',
        element.adjustTooltipForHelpBubble('Original Tooltip'));
  });
});
