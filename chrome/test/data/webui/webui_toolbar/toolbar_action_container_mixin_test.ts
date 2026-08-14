// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {CrLitElement, html} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
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
      disableTransitions: {type: Boolean},
    };
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    // Mock getBoundingClientRect to simulate transition state.
    for (const el of this.shadowRoot.querySelectorAll<HTMLElement>('.item')) {
      if (!el.hasOwnProperty('getBoundingClientRect')) {
        const original = el.getBoundingClientRect.bind(el);
        el.getBoundingClientRect = () => {
          if (el.classList.contains('exiting')) {
            // If transitions are disabled, it snaps to 0 width immediately.
            // Otherwise, it simulates a running transition by returning >0
            // width.
            return {
              width: this.disableTransitions ? 0 : 100,
              x: 0,
              y: 0,
              top: 0,
              bottom: 0,
              left: 0,
              right: 0,
              toJSON: () => {},
            } as DOMRect;
          }
          return original();
        };
      }
    }
  }

  // When true, modifies isInitialUpdate() to only return true when the incoming
  // newStates array is empty, allowing tests to test non-initial update
  // behavior.
  accessor customIsInitial: boolean = false;

  // When true, simulates that CSS transitions are disabled (width snaps to 0).
  accessor disableTransitions: boolean = false;

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
  override isDraggable(state: TestState, index: number): boolean {
    if (state.id === 'divider') {
      return false;
    }
    const dividerIdx = this.keyedStates.findIndex(s => s.key === 'divider');
    if (dividerIdx !== -1 && index > dividerIdx) {
      return false;
    }
    return true;
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
    assertTrue(element.classList.contains('initial-load'));

    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    assertFalse(element.hidden);
    assertEquals(2, element.keyedStates.length);
    assertEquals(false, element.keyedStates[0]!.animateIn);
    assertEquals(false, element.keyedStates[1]!.animateIn);
    assertTrue(element.classList.contains('initial-load'));
    assertFalse(element.allExiting());
    assertFalse(element.animateInDivider());
  });

  test('AnimateInForNewItems', async function() {
    // First update (initial)
    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();
    assertTrue(element.classList.contains('initial-load'));
    assertEquals(1, element.keyedStates.length);
    assertEquals(false, element.keyedStates[0]!.animateIn);

    // Second update (non-initial)
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();
    assertFalse(element.classList.contains('initial-load'));
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
    assertFalse(element.animateInDivider());
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

    // Simulate that transition finished and width is now 0.
    element.disableTransitions = true;

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

    // Simulate that transitions are now disabled/cancelled and it snapped to 0.
    element.disableTransitions = true;

    domItem.dispatchEvent(new TransitionEvent(
        'transitioncancel', {propertyName: 'width', bubbles: true}));
    assertEquals(1, element.keyedStates.length);
    assertEquals('a', element.keyedStates[0]!.key);
  });

  test('ExitingItemsWithZeroWidthRemovedImmediately', async function() {
    element.disableTransitions = true;

    // Add two actions.
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();
    assertEquals(2, element.keyedStates.length);

    // Get rid of 'b' action. Since disableTransitions is true, it should snap
    // to 0.
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
    assertFalse(element.classList.contains('initial-load'));
  });

  test('AnimateInResetAfterTransitionDone', async function() {
    element.states = [
      {id: 'a', name: 'Item A'},
    ];
    await microtasksFinished();

    // Initial update doesn't animate.
    assertFalse(element.keyedStates[0]!.animateIn === true);

    // Add new item.
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    // New item should have animateIn = true.
    assertTrue(element.keyedStates[1]!.animateIn === true);

    // Simulate transitionend event for width.
    const childElB = element.shadowRoot.querySelector('[data-key="b"]');
    assertTrue(!!childElB);
    childElB.dispatchEvent(new TransitionEvent('transitionend', {
      propertyName: 'width',
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // animateIn should be reset to false.
    assertFalse(element.keyedStates[1]!.animateIn === true);
  });

  test('ExitingItemsRemovedOnTransitionDoneMidSlideIn', async function() {
    // Add 'a' as initial.
    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();

    // Add 'b'. It should start sliding in.
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    // Remove 'b' immediately (mid-slide-in).
    element.states = [{id: 'a', name: 'Item A'}];
    await microtasksFinished();

    // 'b' should now be exiting, but NOT removed yet because its width in test
    // is 100px (disableTransitions is false), so it simulates transitioning.
    assertEquals(2, element.keyedStates.length);
    const itemB = element.keyedStates[1]!;
    assertEquals('b', itemB.key);
    assertEquals(true, itemB.exiting);

    const domItem = element.shadowRoot.querySelector('[data-key="b"]');
    assertTrue(!!domItem);
    assertTrue(domItem.classList.contains('exiting'));

    // Simulate that transition finished and width is now 0.
    element.disableTransitions = true;

    // Trigger transitionend to remove it.
    domItem.dispatchEvent(new TransitionEvent(
        'transitionend', {propertyName: 'width', bubbles: true}));

    assertEquals(1, element.keyedStates.length);
    assertEquals('a', element.keyedStates[0]!.key);
  });

  test('DragLeaveGuardedByRelatedTarget', async function() {
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    // Start drag.
    element.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: 'a'},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Dispatch dragenter to simulate entering the host from outside.
    const dragEnterEvent = new DragEvent('dragenter', {
      relatedTarget: null,
      bubbles: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {
        types: ['application/x-test'],
      },
    });
    element.dispatchEvent(dragEnterEvent);
    await microtasksFinished();

    // Verify element 'a' has dragPlaceholder.
    assertTrue(element.keyedStates[0]!.dragPlaceholder === true);

    const childElB = element.shadowRoot.querySelector('[data-key="b"]');
    assertTrue(!!childElB);

    // Simulate dragover 'b' to reorder.
    const dragOverEvent = {
      preventDefault: () => {},
      dataTransfer: {
        types: ['application/x-test'],
        dropEffect: 'none',
      },
      currentTarget: childElB,
    } as unknown as DragEvent;
    element.onActionDragover(dragOverEvent);
    await microtasksFinished();

    // Verify reordered: 'b' then 'a'. 'a' is still placeholder.
    assertEquals('b', element.keyedStates[0]!.key);
    assertEquals('a', element.keyedStates[1]!.key);
    assertTrue(element.keyedStates[1]!.dragPlaceholder === true);

    // Simulate dragleave where relatedTarget is a child.
    const dragLeaveEvent = new DragEvent('dragleave', {
      relatedTarget: childElB,
      bubbles: true,
      composed: true,
    });
    Object.defineProperty(dragLeaveEvent, 'dataTransfer', {
      value: {
        types: ['application/x-test'],
      },
    });
    element.dispatchEvent(dragLeaveEvent);
    await microtasksFinished();

    // Reconcile should NOT have been called, order should still be 'b' then
    // 'a'.
    assertEquals('b', element.keyedStates[0]!.key);
    assertEquals('a', element.keyedStates[1]!.key);
    assertTrue(element.keyedStates[1]!.dragPlaceholder === true);

    // Simulate dragleave where relatedTarget is null (outside).
    const rect = element.getBoundingClientRect();
    const dragLeaveOutsideEvent = new DragEvent('dragleave', {
      relatedTarget: null,
      bubbles: true,
      composed: true,
      clientX: rect.left - 10,
      clientY: rect.top - 10,
    });
    Object.defineProperty(dragLeaveOutsideEvent, 'dataTransfer', {
      value: {
        types: ['application/x-test'],
      },
    });
    element.dispatchEvent(dragLeaveOutsideEvent);
    await microtasksFinished();

    // Reconcile should have been called, order reverted to 'a' then 'b'.
    // Placeholder flag should still be present on 'a'.
    assertEquals('a', element.keyedStates[0]!.key);
    assertEquals('b', element.keyedStates[1]!.key);
    assertTrue(element.keyedStates[0]!.dragPlaceholder === true);
  });

  test('DragCannotCrossDivider', async function() {
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
      {id: 'divider', name: 'Divider'},
      {id: 'c', name: 'Item C'},
      {id: 'd', name: 'Item D'},
    ];
    await microtasksFinished();

    // Start drag on 'a'.
    element.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: 'a'},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Verify 'a' has placeholder.
    assertTrue(element.keyedStates[0]!.dragPlaceholder === true);

    const childElC = element.shadowRoot.querySelector('[data-key="c"]');
    assertTrue(!!childElC);

    // Try to drag 'a' over 'c' (past divider).
    const dragOverCEvent = {
      preventDefault: () => {},
      dataTransfer: {
        types: ['application/x-test'],
        dropEffect: 'none',
      },
      currentTarget: childElC,
    } as unknown as DragEvent;
    element.onActionDragover(dragOverCEvent);
    await microtasksFinished();

    // 'a' should NOT have moved past divider. It should be at index 1 (just
    // before divider). Order should be 'b', 'a' (placeholder), 'divider', 'c',
    // 'd'.
    assertEquals('b', element.keyedStates[0]!.key);
    assertEquals('a', element.keyedStates[1]!.key);
    assertEquals('divider', element.keyedStates[2]!.key);
    assertEquals('c', element.keyedStates[3]!.key);
    assertEquals('d', element.keyedStates[4]!.key);
    assertTrue(element.keyedStates[1]!.dragPlaceholder === true);
  });

  test('DragStartNotAbortedByNonOrderChangingUpdates', async function() {
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    const childElA = element.shadowRoot.querySelector('[data-key="a"]');
    assertTrue(!!childElA);

    // Simulate pointerdown on 'a'.
    childElA.dispatchEvent(new PointerEvent('pointerdown', {
      isPrimary: true,
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Simulate a state update that doesn't change order.
    element.states = [
      {id: 'a', name: 'Item A Updated'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    // The update should NOT be deferred, so DOM should have updated.
    const childElA_afterUpdate =
        element.shadowRoot.querySelector('[data-key="a"]');
    assertTrue(!!childElA_afterUpdate);
    assertEquals('Item A Updated', childElA_afterUpdate.textContent.trim());

    // Start drag on 'a'.
    element.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: 'a'},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Verify drag is NOT aborted (placeholder flag should be set).
    assertTrue(element.keyedStates[0]!.dragPlaceholder === true);
  });

  test('DragNotAbortedOnOrderChangeIfDraggedItemNotRemoved', async function() {
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    // Start drag.
    element.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: 'a'},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Verify element 'a' has dragPlaceholder.
    assertTrue(element.keyedStates[0]!.dragPlaceholder === true);
    assertEquals('a', (element as any).draggedItemId_);

    // Change states (swap order in Mojo).
    element.states = [
      {id: 'b', name: 'Item B'},
      {id: 'a', name: 'Item A'},
    ];
    await microtasksFinished();

    // Drag should NOT be aborted.
    assertEquals('a', (element as any).draggedItemId_);

    // Layout should NOT update to Mojo order immediately, preserve local
    // placeholder. So order is still 'a*' then 'b'.
    assertEquals('a', element.keyedStates[0]!.key);
    assertTrue(element.keyedStates[0]!.dragPlaceholder === true);
    assertEquals('b', element.keyedStates[1]!.key);

    // End drag (aborted)
    element.dispatchEvent(new CustomEvent('toolbar-action-drag-end', {
      detail: {itemId: 'a', dropEffect: 'none'},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    // Verify drag is ended and layout updated to Mojo order (b then a).
    assertEquals(null, (element as any).draggedItemId_);
    assertEquals('b', element.keyedStates[0]!.key);
    assertEquals('a', element.keyedStates[1]!.key);
    assertFalse(element.keyedStates[1]!.dragPlaceholder === true);
  });

  test('AbortDragOnDraggedItemRemoval', async function() {
    element.states = [
      {id: 'a', name: 'Item A'},
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    // Start drag.
    element.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: 'a'},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertTrue(element.keyedStates[0]!.dragPlaceholder === true);

    // Remove dragged item 'a'
    element.states = [
      {id: 'b', name: 'Item B'},
    ];
    await microtasksFinished();

    // Drag should be aborted immediately
    assertEquals(null, (element as any).draggedItemId_);

    // Layout updated immediately (a marked as exiting)
    assertEquals(2, element.keyedStates.length);
    assertEquals('a', element.keyedStates[0]!.key);
    assertTrue(element.keyedStates[0]!.exiting === true);
    assertEquals('b', element.keyedStates[1]!.key);
  });
});
