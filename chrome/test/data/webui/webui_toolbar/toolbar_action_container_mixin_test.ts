// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {AnimationTracker, ToolbarActionContainerMixin} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {KeyedActionState, ToolbarActionContainerMixinInterface} from 'chrome://webui-toolbar.top-chrome/app.js';

interface TestState {
  id: string;
  name: string;
}

const DefaultContainerBase =
    ToolbarActionContainerMixin(CrLitElement, [] as TestState[]);

/**
 * Basic container element without mixin overrides, used to verify default
 * method assertions and fallback behavior.
 */
class DefaultContainerElement extends DefaultContainerBase {
  static get is() {
    return 'default-container';
  }
}

customElements.define(DefaultContainerElement.is, DefaultContainerElement);

const TestContainerBase =
    ToolbarActionContainerMixin(CrLitElement, [] as TestState[]);

/**
 * Customizable container element that overrides getKey and implements a Lit
 * template to test state reconciliation, slide-out transition lifecycle, and
 * zero-width removal handling.
 */
class TestContainerElement extends TestContainerBase {
  static get is() {
    return 'test-container';
  }

  override render() {
    return html`
      <div id="container">
        ${
        this.keyedStates.map(
            s => html`
          <div class="item ${s.exiting ? 'exiting' : ''} ${
                s.animateIn ? 'animate-in' : ''}"
               style="${
                s.exiting && this.forceZeroWidthExiting ?
                    'width: 0px; display: inline-block;' :
                    'width: 100px; display: inline-block;'}"
               data-key="${s.key}">
            ${s.state.name}
          </div>
        `)}
      </div>
    `;
  }

  static override get properties() {
    return {
      customIsInitial: {type: Boolean},
      forceZeroWidthExiting: {type: Boolean},
    };
  }

  // When true, modifies isInitialUpdate() to only return true when the incoming
  // newStates array is empty, allowing tests to test non-initial update
  // behavior.
  accessor customIsInitial: boolean = false;

  // When true, renders exiting items with a width of 0px so that
  // reconcileKeys() immediately removes them without waiting for CSS
  // transitions to finish.
  accessor forceZeroWidthExiting: boolean = false;

  override getKey(state: TestState): string {
    return state.id;
  }

  override isInitialUpdate(newStates: TestState[]): boolean {
    if (this.customIsInitial) {
      return newStates.length === 0;
    }
    return super.isInitialUpdate(newStates);
  }

  override moveItem(_id: string, _index: number) {}
  override moveItemBy(_id: string, _delta: number) {}
  override getMimeType(): string {
    return 'application/x-test';
  }
  override getBroadcastChannelName(): string {
    return 'test-channel';
  }
  override get childTagName(): string {
    return 'div';
  }
  override isDivider(_key: string): boolean {
    return false;
  }
}

customElements.define(TestContainerElement.is, TestContainerElement);

declare global {
  interface HTMLElementTagNameMap {
    'default-container': DefaultContainerElement;
    'test-container': TestContainerElement;
  }
}

suite('ToolbarActionContainerMixinTest', function() {
  let element: TestContainerElement;

  teardown(() => {
    AnimationTracker.resetForTesting();
  });

  setup(async () => {
    AnimationTracker.showAnimations = true;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('test-container');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('DefaultGetKeyThrows', function() {
    const defaultEl = document.createElement('default-container');
    assertThrows(() => {
      defaultEl.getKey({id: 'test', name: 'Test'});
    });
  });

  test('InitialUpdateAndVisibility', async function() {
    const interfaceCheck: ToolbarActionContainerMixinInterface<TestState> =
        element;
    const stateCheck: Array<KeyedActionState<TestState>> = element.keyedStates;
    assertTrue(!!interfaceCheck && !!stateCheck);

    assertTrue(element.hidden);
    assertTrue(element.isInitialUpdate(element.states));

    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    assertFalse(element.hidden);
    assertEquals(2, element.keyedStates.length);
    assertEquals(false, element.keyedStates[0]!.animateIn);
    assertEquals(false, element.keyedStates[1]!.animateIn);
    assertFalse(element.allExiting());
    assertFalse(element.animateInDivider());
  });

  test('AnimateInForNewItems', async function() {
    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();

    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    assertEquals(2, element.keyedStates.length);
    assertEquals(false, element.keyedStates[0]!.animateIn);
    assertEquals(true, element.keyedStates[1]!.animateIn);
    assertFalse(element.animateInDivider());

    element.states = [{id: 'c', name: 'Item C'}];
    await microtasksFinished();
    const activeStates = element.keyedStates.filter(s => !s.exiting);
    assertEquals(1, activeStates.length);
    assertEquals('c', activeStates[0]!.key);
    assertEquals(true, activeStates[0]!.animateIn);
  });

  test('AnimateInDividerAndAllExiting', async function() {
    AnimationTracker.showAnimations = false;
    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();

    element.states = [{id: 'b', name: 'Item B'}];
    await microtasksFinished();

    assertEquals(1, element.keyedStates.length);
    assertEquals('b', element.keyedStates[0]!.key);
    assertTrue(element.animateInDivider());
    assertFalse(element.allExiting());

    AnimationTracker.showAnimations = true;
    element.states = [];
    await microtasksFinished();

    assertEquals(1, element.keyedStates.length);
    assertEquals('b', element.keyedStates[0]!.key);
    assertEquals(true, element.keyedStates[0]!.exiting);
    assertTrue(element.allExiting());
  });

  test('ExitingItemsRemovedWhenAnimationsDisabled', async function() {
    AnimationTracker.showAnimations = false;
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();

    assertEquals(1, element.keyedStates.length);
    assertEquals('a', element.keyedStates[0]!.key);
  });

  test('ExitingItemsRemovedOnTransitionDone', async function() {
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    element.states = [{id: 'b', name: 'Item B'}];
    await microtasksFinished();

    assertEquals(2, element.keyedStates.length);
    const itemA = element.keyedStates[0]!;
    assertEquals('a', itemA.key);
    assertEquals(true, itemA.exiting);

    const domItem = element.shadowRoot.querySelector('[data-key="a"]');
    assertTrue(!!domItem);
    assertTrue(domItem.classList.contains('exiting'));

    domItem.dispatchEvent(new TransitionEvent(
        'transitionend', {propertyName: 'height', bubbles: true}));
    assertEquals(2, element.keyedStates.length);

    const container = element.shadowRoot.querySelector('#container');
    assertTrue(!!container);
    container.dispatchEvent(new TransitionEvent(
        'transitionend', {propertyName: 'width', bubbles: true}));
    assertEquals(2, element.keyedStates.length);

    domItem.dispatchEvent(new TransitionEvent(
        'transitionend', {propertyName: 'width', bubbles: true}));
    assertEquals(1, element.keyedStates.length);
    assertEquals('b', element.keyedStates[0]!.key);
  });

  test('ExitingItemsRemovedOnTransitionCancel', async function() {
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();

    assertEquals(2, element.keyedStates.length);
    const domItem = element.shadowRoot.querySelector('[data-key="b"]');
    assertTrue(!!domItem);

    domItem.dispatchEvent(new TransitionEvent(
        'transitioncancel', {propertyName: 'width', bubbles: true}));
    assertEquals(1, element.keyedStates.length);
    assertEquals('a', element.keyedStates[0]!.key);
  });

  test('ExitingItemsWithZeroWidthRemovedImmediately', async function() {
    element.forceZeroWidthExiting = true;

    // Add two actions.
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();
    assertEquals(2, element.keyedStates.length);

    // Get rid of 'b' action, `forceZeroWidthExiting` should make it 0 width.
    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();

    // 'b' should instantly go away as it had zero length (i.e. didn't animate
    // away).
    assertEquals(1, element.keyedStates.length);
    assertEquals('a', element.keyedStates[0]!.key);
  });

  test('CustomIsInitialOverride', async function() {
    element.customIsInitial = true;
    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();

    assertFalse(element.isInitialUpdate(element.states));
    assertEquals(1, element.keyedStates.length);
    assertEquals('a', element.keyedStates[0]!.key);
    assertEquals(true, element.keyedStates[0]!.animateIn);
  });
});
