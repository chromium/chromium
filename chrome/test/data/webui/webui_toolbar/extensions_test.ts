// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {ExtensionsElement} from 'chrome://webui-toolbar.top-chrome/app.js';

suite('Extensions', function() {
  let container: ExtensionsElement;
  let moveCalls: Array<{extensionId: string, index: number}> = [];
  let moveByCalls: Array<{extensionId: string, delta: number}> = [];

  let originalBroadcastChannel: typeof BroadcastChannel;
  let channels: any[] = [];

  suiteSetup(() => {
    originalBroadcastChannel = window.BroadcastChannel;
    class MockBroadcastChannel {
      name: string;
      onmessage: ((e: MessageEvent) => void)|null = null;
      constructor(name: string) {
        this.name = name;
        channels.push(this);
      }
      postMessage(msg: any) {
        for (const channel of channels) {
          if (channel !== this && channel.name === this.name &&
              channel.onmessage) {
            channel.onmessage(new MessageEvent('message', {data: msg}));
          }
        }
      }
      close() {
        const idx = channels.indexOf(this);
        if (idx !== -1) {
          channels.splice(idx, 1);
        }
      }
    }
    window.BroadcastChannel = MockBroadcastChannel as any;
  });

  suiteTeardown(() => {
    window.BroadcastChannel = originalBroadcastChannel;
  });

  setup(async () => {
    channels = [];  // Reset active channels
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    moveCalls = [];
    moveByCalls = [];

    const mockHandler = {
      moveExtensionAction: (extensionId: string, index: number) => {
        moveCalls.push({extensionId, index});
      },
      moveExtensionActionBy: (extensionId: string, delta: number) => {
        moveByCalls.push({extensionId, delta});
      },
    };
    BrowserProxyImpl.setInstance({toolbarUIHandler: mockHandler} as any);

    const mockTrackedElementManager = {
      startTracking: () => {},
      stopTracking: () => {},
    };
    TrackedElementManager.setInstance(mockTrackedElementManager as any);

    container = document.createElement('webui-toolbar-extensions');
    container.style.setProperty('--animations-enabled', '0');
    document.body.appendChild(container);

    // Initial state with 2 extensions and the menu button
    container.states = [
      {
        id: 'action-1',
        accessibleName: 'Action 1',
        tooltip: 'Action 1 Tooltip',
        isVisible: true,
        icon: {handleId: 1n},
      },
      {
        id: 'action-2',
        accessibleName: 'Action 2',
        tooltip: 'Action 2 Tooltip',
        isVisible: true,
        icon: {handleId: 2n},
      },
      {
        id: '',
        accessibleName: 'Extensions Button',
        tooltip: 'Extensions Button Tooltip',
        isVisible: true,
        icon: {handleId: 3n},
      },
    ];
    await microtasksFinished();
  });

  test('Keyboard reorder retains focus', async () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const firstAction = actionElements[0]!;

    // Focus the first action's button
    firstAction.focus();
    assertEquals(
        firstAction.shadowRoot.activeElement,
        firstAction.shadowRoot.querySelector('cr-button'));

    // Trigger keyboard reorder (Ctrl+ArrowRight)
    const button = firstAction.shadowRoot.querySelector('cr-button')!;
    button.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowRight',
      ctrlKey: true,
      bubbles: true,
      composed: true,
    }));

    assertEquals(1, moveByCalls.length);
    assertEquals('action-1', moveByCalls[0]!.extensionId);
    assertEquals(1, moveByCalls[0]!.delta);

    // Simulate model update resulting from the move
    // We move Action 1 to index 1 (after Action 2)
    container.states = [
      {
        id: 'action-2',
        accessibleName: 'Action 2',
        tooltip: 'Action 2 Tooltip',
        isVisible: true,
        icon: {handleId: 2n},
      },
      {
        id: 'action-1',
        accessibleName: 'Action 1',
        tooltip: 'Action 1 Tooltip',
        isVisible: true,
        icon: {handleId: 1n},
      },
      {
        id: 'extensions_button',
        accessibleName: 'Extensions Button',
        tooltip: 'Extensions Button Tooltip',
        isVisible: true,
        icon: {handleId: 3n},
      },
    ];
    await microtasksFinished();

    // Verify focus is restored to Action 1 (which is now the second element in
    // DOM)
    const newActionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    const newSecondAction = newActionElements[1]!;
    assertEquals(
        'action-1', newSecondAction.state.id);  // Verify it is indeed Action 1
    assertEquals(
        newSecondAction.shadowRoot.activeElement,
        newSecondAction.shadowRoot.querySelector('cr-button'));
  });

  test('Cross-window dragover identifies action from broadcast', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const secondAction = actionElements[1]!;  // Action 2

    // Simulate broadcast from another window
    const helperChannel = new BroadcastChannel('extension-action-drag');
    helperChannel.postMessage(
        {type: 'drag-start', itemId: 'action-1'});  // Action 1 is dragged

    // Simulate dragenter on the container first to initialize placeholder
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
      },
    });
    container.dispatchEvent(dragEnterEvent);

    // Create a fake dragover event coming from another window
    const dragEvent = new DragEvent('dragover', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
        dropEffect: 'none',
      },
    });

    // Hover over Action 2
    secondAction.dispatchEvent(dragEvent);

    // Verify dragover was accepted
    assertTrue(dragEvent.defaultPrevented);
    assertEquals('move', dragEvent.dataTransfer!.dropEffect);

    // Verify that keyedStates was updated to show placeholder for Action 1 at
    // the hovered position
    const keyedStates = container.keyedStates;
    assertEquals(3, keyedStates.length);
    assertEquals('action-2', keyedStates[0]!.key);  // Action 2 is now first
    assertEquals('action-1', keyedStates[1]!.key);  // Action 1 is now second
    assertTrue(!!keyedStates[1]!.dragPlaceholder);
  });

  test('Local drag start broadcasts', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const firstAction = actionElements[0]!;

    let receivedMessage: any = null;
    const listenerChannel = new BroadcastChannel('extension-action-drag');
    listenerChannel.onmessage = (e) => {
      receivedMessage = e.data;
    };

    // Trigger dragstart on first action
    firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: 'action-1'},
      bubbles: true,
      composed: true,
    }));

    assertEquals('drag-start', receivedMessage?.type);
    assertEquals('action-1', receivedMessage?.itemId);
  });

  test('Local drag end broadcasts', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const firstAction = actionElements[0]!;

    // Start drag first
    firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: 'action-1'},
      bubbles: true,
      composed: true,
    }));

    let receivedMessage: any = null;
    const listenerChannel = new BroadcastChannel('extension-action-drag');
    listenerChannel.onmessage = (e) => {
      receivedMessage = e.data;
    };

    // Trigger dragend
    container.dispatchEvent(new CustomEvent('toolbar-action-drag-end', {
      detail: {itemId: 'action-1', dropEffect: 'move'},
      bubbles: true,
      composed: true,
    }));

    assertEquals('drag-end', receivedMessage?.type);
  });

  test('Cross-window dragover on itself marks as placeholder', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const firstAction = actionElements[0]!;

    const helperChannel = new BroadcastChannel('extension-action-drag');
    helperChannel.postMessage({type: 'drag-start', itemId: 'action-1'});

    // Simulate dragenter on the container first to initialize placeholder
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
      },
    });
    container.dispatchEvent(dragEnterEvent);

    const dragEvent = new DragEvent('dragover', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
        dropEffect: 'none',
      },
    });

    firstAction.dispatchEvent(dragEvent);

    assertTrue(dragEvent.defaultPrevented);

    const keyedStates = container.keyedStates;
    assertEquals(3, keyedStates.length);
    assertEquals('action-1', keyedStates[0]!.key);
    assertTrue(!!keyedStates[0]!.dragPlaceholder);
  });

  test(
      'Cross-window dragenter before broadcast sets placeholder on broadcast',
      () => {
        // This test simulates a fast drag where the native dragenter event
        // is dispatched before the BroadcastChannel message is processed by
        // the target window.
        // 1. Dispatch dragenter first (no broadcast has happened yet)
        const dragEnterEvent = new DragEvent('dragenter', {
          bubbles: true,
          cancelable: true,
          composed: true,
        });
        Object.defineProperty(dragEnterEvent, 'dataTransfer', {
          value: {
            types: ['application/x-webui-extension-action'],
          },
        });
        container.dispatchEvent(dragEnterEvent);

        // Verify no placeholder is set yet
        let keyedStates = container.keyedStates;
        assertEquals(3, keyedStates.length);
        assertTrue(!keyedStates[0]!.dragPlaceholder);
        assertTrue(!keyedStates[1]!.dragPlaceholder);
        assertTrue(!keyedStates[2]!.dragPlaceholder);

        // 2. Simulate broadcast arriving now
        const helperChannel = new BroadcastChannel('extension-action-drag');
        helperChannel.postMessage({type: 'drag-start', itemId: 'action-1'});

        // Verify that the placeholder is now set for Action 1
        keyedStates = container.keyedStates;
        assertEquals('action-1', keyedStates[0]!.key);
        assertTrue(!!keyedStates[0]!.dragPlaceholder);
      });

  test('Mousemove during local drag triggers fallback drag-end cleanup', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const firstAction = actionElements[0]!;

    // 1. Start a local drag
    firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: 'action-1'},
      bubbles: true,
      composed: true,
    }));

    // Verify it is marked as dragging/placeholder locally
    let keyedStates = container.keyedStates;
    assertTrue(!!keyedStates[0]!.dragPlaceholder);
    assertEquals('action-1', (container as any).draggedItemId_);

    // Set up broadcast listener to verify it broadcasts drag-end
    let receivedMessage: any = null;
    const listenerChannel = new BroadcastChannel('extension-action-drag');
    listenerChannel.onmessage = (e) => {
      receivedMessage = e.data;
    };

    // 2. Dispatch a window-level mousemove event
    window.dispatchEvent(new MouseEvent('mousemove'));

    // Verify cleanup occurred
    assertEquals(null, (container as any).draggedItemId_);
    keyedStates = container.keyedStates;
    assertTrue(!keyedStates[0]!.dragPlaceholder);

    // Verify broadcast occurred
    assertEquals('drag-end', receivedMessage?.type);
  });

  test(
      'Mousemove with buttons pressed does not trigger fallback cleanup',
      () => {
        const actionElements =
            container.shadowRoot.querySelectorAll('webui-toolbar-extension');
        assertEquals(3, actionElements.length);
        const firstAction = actionElements[0]!;

        // 1. Start a local drag
        firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
          detail: {itemId: 'action-1'},
          bubbles: true,
          composed: true,
        }));

        // Verify it is marked as dragging/placeholder locally
        let keyedStates = container.keyedStates;
        assertTrue(keyedStates[0]!.dragPlaceholder === true);
        assertEquals('action-1', (container as any).draggedItemId_);

        // 2. Dispatch a window-level mousemove event with buttons pressed
        window.dispatchEvent(new MouseEvent('mousemove', {buttons: 1}));

        // Verify cleanup did NOT occur
        assertEquals('action-1', (container as any).draggedItemId_);
        keyedStates = container.keyedStates;
        assertTrue(keyedStates[0]!.dragPlaceholder === true);
      });

  test('Drop on child button triggers Mojo moveExtensionAction', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const secondAction = actionElements[1]!;  // Action 2

    // 1. Simulate cross-window drag enter of Action 1
    const helperChannel = new BroadcastChannel('extension-action-drag');
    helperChannel.postMessage({type: 'drag-start', itemId: 'action-1'});

    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {types: ['application/x-webui-extension-action']},
    });
    container.dispatchEvent(dragEnterEvent);

    // 2. Simulate dragover over Action 2 to calculate reorder position
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragOverEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
        dropEffect: 'none',
      },
    });
    secondAction.dispatchEvent(dragOverEvent);

    // Verify reorder happened locally (Action 1 is now second, i.e., index 1)
    const keyedStates = container.keyedStates;
    assertEquals('action-1', keyedStates[1]!.key);

    // 3. Simulate drop on Action 2 (which is now at index 0)
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dropEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
        getData: (type: string) => {
          if (type === 'application/x-webui-extension-action') {
            return JSON.stringify({itemId: 'action-1'});
          }
          return '';
        },
      },
    });

    secondAction.dispatchEvent(dropEvent);

    // 4. Simulate broadcast drag-end (successful drop) from the other window
    helperChannel.postMessage({type: 'drag-end', aborted: false});

    // Verify Mojo call was made
    assertEquals(1, moveCalls.length);
    assertEquals('action-1', moveCalls[0]!.extensionId);
    assertEquals(1, moveCalls[0]!.index);

    // Verify layout remains in the new order after drop
    const postDropKeyedStates = container.keyedStates;
    assertEquals('action-2', postDropKeyedStates[0]!.key);
    assertEquals('action-1', postDropKeyedStates[1]!.key);
    assertTrue(!postDropKeyedStates[1]!.dragPlaceholder);
  });

  test('Drop on container (empty space) commits reordering', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const secondAction = actionElements[1]!;  // Action 2

    // 1. Simulate cross-window drag enter and reorder
    const helperChannel = new BroadcastChannel('extension-action-drag');
    helperChannel.postMessage({type: 'drag-start', itemId: 'action-1'});

    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {types: ['application/x-webui-extension-action']},
    });
    container.dispatchEvent(dragEnterEvent);

    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragOverEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
        dropEffect: 'none',
      },
    });
    secondAction.dispatchEvent(dragOverEvent);

    // Verify local reorder happened
    let keyedStates = container.keyedStates;
    assertEquals('action-2', keyedStates[0]!.key);
    assertEquals('action-1', keyedStates[1]!.key);

    // 2. Dispatch drop on the container itself (simulating drop on empty
    // space)
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dropEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
        getData: (type: string) => {
          if (type === 'application/x-webui-extension-action') {
            return JSON.stringify({itemId: 'action-1'});
          }
          return '';
        },
      },
    });
    container.dispatchEvent(dropEvent);

    // Verify Mojo call was made to move Action 1 to the placeholder position
    // (index 1)
    assertEquals(1, moveCalls.length);
    assertEquals('action-1', moveCalls[0]!.extensionId);
    assertEquals(1, moveCalls[0]!.index);

    // 3. Simulate broadcast drag-end (successful drop) from the other window
    helperChannel.postMessage({type: 'drag-end', aborted: false});

    // Verify layout remains in the new order after drop
    keyedStates = container.keyedStates;
    assertEquals('action-2', keyedStates[0]!.key);
    assertEquals('action-1', keyedStates[1]!.key);
    assertTrue(!keyedStates[1]!.dragPlaceholder);
    assertEquals(0, (container as any).dragEnterCount_);
  });

  test('Extension action is draggable', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    const firstAction = actionElements[0]!;
    assertTrue(firstAction.isDraggable());
    const button = firstAction.shadowRoot.querySelector('cr-button')!;
    assertEquals('true', button.getAttribute('draggable'));
  });

  test('Extension action dragstart is not cancelled', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    const firstAction = actionElements[0]!;
    const button = firstAction.shadowRoot.querySelector('cr-button')!;

    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    let dataSet = false;
    Object.defineProperty(dragStartEvent, 'dataTransfer', {
      value: {
        setData: (type: string, value: string) => {
          assertEquals('application/x-webui-extension-action', type);
          const data = JSON.parse(value);
          assertEquals('action-1', data.itemId);
          dataSet = true;
        },
        effectAllowed: 'none',
      },
    });
    button.dispatchEvent(dragStartEvent);

    assertTrue(!dragStartEvent.defaultPrevented);
    assertTrue(dataSet);
  });

  test('State updates are deferred during pointerdown', async () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    const firstAction = actionElements[0]!;

    // 1. Dispatch pointerdown on the action
    const pointerDownEvent = new PointerEvent('pointerdown', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(pointerDownEvent, 'isPrimary', {value: true});
    firstAction.dispatchEvent(pointerDownEvent);

    // 2. Update state (change tooltip of action-1)
    container.states = [
      {
        id: 'action-1',
        accessibleName: 'Action 1',
        tooltip: 'Action 1 Tooltip Changed',
        isVisible: true,
        icon: {handleId: 1n},
      },
      {
        id: 'action-2',
        accessibleName: 'Action 2',
        tooltip: 'Action 2 Tooltip',
        isVisible: true,
        icon: {handleId: 2n},
      },
      {
        id: '',
        accessibleName: 'Extensions Button',
        tooltip: 'Extensions Button Tooltip',
        isVisible: true,
        icon: {handleId: 3n},
      },
    ];

    await microtasksFinished();

    // Verify update is deferred (still has old tooltip in keyedStates)
    let keyedStates = container.keyedStates;
    assertEquals('Action 1 Tooltip', keyedStates[0]!.state.tooltip);

    // 3. Dispatch pointerup on window
    const pointerUpEvent = new PointerEvent('pointerup', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    window.dispatchEvent(pointerUpEvent);

    await microtasksFinished();

    // Verify update is now applied
    keyedStates = container.keyedStates;
    assertEquals('Action 1 Tooltip Changed', keyedStates[0]!.state.tooltip);
  });

  test('State updates are deferred during drag', async () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    const firstAction = actionElements[0]!;
    const button = firstAction.shadowRoot.querySelector('cr-button')!;

    // 1. Start drag
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragStartEvent, 'dataTransfer', {
      value: {
        setData: () => {},
        effectAllowed: 'none',
      },
    });
    button.dispatchEvent(dragStartEvent);

    // 2. Update state (change tooltip)
    container.states = [
      {
        id: 'action-1',
        accessibleName: 'Action 1',
        tooltip: 'Action 1 Tooltip Changed',
        isVisible: true,
        icon: {handleId: 1n},
      },
      {
        id: 'action-2',
        accessibleName: 'Action 2',
        tooltip: 'Action 2 Tooltip',
        isVisible: true,
        icon: {handleId: 2n},
      },
      {
        id: '',
        accessibleName: 'Extensions Button',
        tooltip: 'Extensions Button Tooltip',
        isVisible: true,
        icon: {handleId: 3n},
      },
    ];

    await microtasksFinished();

    // Verify update is deferred
    let keyedStates = container.keyedStates;
    assertEquals('Action 1 Tooltip', keyedStates[0]!.state.tooltip);

    // 3. End drag (aborted)
    const dragEndEvent = new DragEvent('dragend', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEndEvent, 'dataTransfer', {
      value: {
        dropEffect: 'none',  // aborted
      },
    });
    button.dispatchEvent(dragEndEvent);

    await microtasksFinished();

    // Verify update is now applied
    keyedStates = container.keyedStates;
    assertEquals('Action 1 Tooltip Changed', keyedStates[0]!.state.tooltip);
  });

  test('State updates that change order abort the drag', async () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    const firstAction = actionElements[0]!;
    const button = firstAction.shadowRoot.querySelector('cr-button')!;

    // 1. Start drag
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragStartEvent, 'dataTransfer', {
      value: {
        setData: () => {},
        effectAllowed: 'none',
      },
    });
    button.dispatchEvent(dragStartEvent);

    assertEquals('action-1', (container as any).draggedItemId_);

    // 2. Update state changing order (swap action-1 and action-2)
    container.states = [
      {
        id: 'action-2',
        accessibleName: 'Action 2',
        tooltip: 'Action 2 Tooltip',
        isVisible: true,
        icon: {handleId: 2n},
      },
      {
        id: 'action-1',
        accessibleName: 'Action 1',
        tooltip: 'Action 1 Tooltip',
        isVisible: true,
        icon: {handleId: 1n},
      },
      {
        id: '',
        accessibleName: 'Extensions Button',
        tooltip: 'Extensions Button Tooltip',
        isVisible: true,
        icon: {handleId: 3n},
      },
    ];

    await microtasksFinished();

    // Verify drag is aborted (draggedItemId_ becomes null)
    assertEquals(null, (container as any).draggedItemId_);

    // Verify layout is updated immediately to the new order
    const keyedStates = container.keyedStates;
    assertEquals('action-2', keyedStates[0]!.key);
    assertEquals('action-1', keyedStates[1]!.key);
    assertTrue(!keyedStates[1]!.dragPlaceholder);
  });

  test('Extensions menu button is not draggable', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    const menuButton = actionElements[2]!;
    assertEquals('', menuButton.state.id);

    // Verify it returns false for isDraggable
    assertTrue(!menuButton.isDraggable());

    const button = menuButton.shadowRoot.querySelector('cr-button')!;
    assertEquals('false', button.getAttribute('draggable'));

    // Try to trigger dragstart
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragStartEvent, 'dataTransfer', {
      value: {
        setData: () => {},
        effectAllowed: 'none',
      },
    });
    button.dispatchEvent(dragStartEvent);

    // Verify dragstart was cancelled
    assertTrue(dragStartEvent.defaultPrevented);
  });

  test('Can drag over extensions menu button to place at end', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('webui-toolbar-extension');
    assertEquals(3, actionElements.length);
    const menuButton = actionElements[2]!;

    // 1. Simulate cross-window drag enter of Action 1
    const helperChannel = new BroadcastChannel('extension-action-drag');
    helperChannel.postMessage({type: 'drag-start', itemId: 'action-1'});

    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {types: ['application/x-webui-extension-action']},
    });
    container.dispatchEvent(dragEnterEvent);

    // 2. Simulate dragover over Extensions Button
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragOverEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
        dropEffect: 'none',
      },
    });
    menuButton.dispatchEvent(dragOverEvent);

    // Verify reorder happened locally. Action 1 should be moved before
    // Extensions Button. Order should be Action 2, Action 1, Extensions Button.
    const keyedStates = container.keyedStates;
    assertEquals('action-2', keyedStates[0]!.key);
    assertEquals('action-1', keyedStates[1]!.key);
    assertEquals('', keyedStates[2]!.key);
    assertTrue(!!keyedStates[1]!.dragPlaceholder);

    // 3. Simulate drop on Extensions Button
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dropEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-extension-action'],
        getData: (type: string) => {
          if (type === 'application/x-webui-extension-action') {
            return JSON.stringify({itemId: 'action-1'});
          }
          return '';
        },
      },
    });
    menuButton.dispatchEvent(dropEvent);

    // Verify Mojo call was made to move to index 1.
    // In WebUI, this inserts the item before the Extensions Menu Button.
    // In the C++ model (which doesn't include the menu button), index 1
    // corresponds to the end of the pinned extensions list.
    assertEquals(1, moveCalls.length);
    assertEquals('action-1', moveCalls[0]!.extensionId);
    assertEquals(1, moveCalls[0]!.index);
  });
});
