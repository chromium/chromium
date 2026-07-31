// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-toolbar.top-chrome/app.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {BrowserProxyImpl, TrackedElementManager} from 'chrome://webui-toolbar.top-chrome/app.js';
import type {PinnedToolbarActionsElement} from 'chrome://webui-toolbar.top-chrome/app.js';
import {PinnedToolbarAction} from 'chrome://webui-toolbar.top-chrome/shared/toolbar_ui_api_data_model.mojom-webui.js';

suite('PinnedToolbarActions', function() {
  let container: PinnedToolbarActionsElement;
  let moveCalls: Array<{itemId: number, index: number}> = [];
  let moveByCalls: Array<{itemId: number, delta: number}> = [];

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
      movePinnedToolbarAction: (itemId: number, index: number) => {
        moveCalls.push({itemId, index});
      },
      movePinnedToolbarActionBy: (itemId: number, delta: number) => {
        moveByCalls.push({itemId, delta});
      },
    };
    BrowserProxyImpl.setInstance({toolbarUIHandler: mockHandler} as any);

    const mockTrackedElementManager = {
      startTracking: () => {},
      stopTracking: () => {},
    };
    TrackedElementManager.setInstance(mockTrackedElementManager as any);

    container = document.createElement('pinned-toolbar-actions');
    document.body.appendChild(container);

    // Initial state with 3 items and a divider
    container.states = [
      {
        action: 1,  // kNewIncognitoWindow
        highlighted: false,
        enabled: true,
        activated: false,
        tooltip: 'Action 1',
        accessibilityText: '',
        elementId: 'action-1',
        icon: {handleId: 1n},
      },
      {
        action: 2,  // kShowPasswordsBubbleOrPage
        highlighted: false,
        enabled: true,
        activated: false,
        tooltip: 'Action 2',
        accessibilityText: '',
        elementId: 'action-2',
        icon: {handleId: 2n},
      },
      {
        action: PinnedToolbarAction.kDivider,
        highlighted: false,
        enabled: true,
        activated: false,
        tooltip: '',
        accessibilityText: '',
        elementId: '',
        icon: {handleId: 0n},
      },
    ];
    await microtasksFinished();
  });

  test('Keyboard reorder retains focus', async () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, actionElements.length);
    const firstAction = actionElements[0]!;

    // Focus the first action's button
    firstAction.focus();
    assertEquals(
        firstAction.shadowRoot.activeElement,
        firstAction.shadowRoot.querySelector('cr-icon-button'));

    // Trigger keyboard reorder (Ctrl+ArrowRight)
    const button = firstAction.shadowRoot.querySelector('cr-icon-button')!;
    button.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'ArrowRight',
      ctrlKey: true,
      bubbles: true,
      composed: true,
    }));

    assertEquals(1, moveByCalls.length);
    assertEquals(1, moveByCalls[0]!.itemId);
    assertEquals(1, moveByCalls[0]!.delta);

    // Simulate model update resulting from the move
    // We move Action 1 to index 1 (after Action 2)
    container.states = [
      {
        action: 2,
        highlighted: false,
        enabled: true,
        activated: false,
        tooltip: 'Action 2',
        accessibilityText: '',
        elementId: 'action-2',
        icon: {handleId: 2n},
      },
      {
        action: 1,
        highlighted: false,
        enabled: true,
        activated: false,
        tooltip: 'Action 1',
        accessibilityText: '',
        elementId: 'action-1',
        icon: {handleId: 1n},
      },
      {
        action: PinnedToolbarAction.kDivider,
        highlighted: false,
        enabled: true,
        activated: false,
        tooltip: '',
        accessibilityText: '',
        elementId: '',
        icon: {handleId: 0n},
      },
    ];
    await microtasksFinished();

    // Verify focus is restored to Action 1 (which is now the second element in
    // DOM)
    const newActionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, newActionElements.length);
    const newSecondAction = newActionElements[1]!;
    assertEquals(
        1, newSecondAction.state.action);  // Verify it is indeed Action 1
    assertEquals(
        newSecondAction.shadowRoot.activeElement,
        newSecondAction.shadowRoot.querySelector('cr-icon-button'));
  });

  test('Cross-window dragover identifies action from broadcast', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, actionElements.length);
    const secondAction = actionElements[1]!;  // Action 2

    // Simulate broadcast from another window
    const helperChannel = new BroadcastChannel('pinned-action-drag');
    helperChannel.postMessage(
        {type: 'drag-start', itemId: '1'});  // Action 1 is dragged

    // Simulate dragenter on the container first to initialize placeholder
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-pinned-action'],
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
        types: ['application/x-webui-pinned-action'],
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
    assertEquals('2', keyedStates[0]!.key);  // Action 2 is now first
    assertEquals('1', keyedStates[1]!.key);  // Action 1 is now second
    assertTrue(!!keyedStates[1]!.dragPlaceholder);
  });

  test('Local drag start broadcasts', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, actionElements.length);
    const firstAction = actionElements[0]!;

    let receivedMessage: any = null;
    const listenerChannel = new BroadcastChannel('pinned-action-drag');
    listenerChannel.onmessage = (e) => {
      receivedMessage = e.data;
    };

    // Trigger dragstart on first action
    firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: '1'},
      bubbles: true,
      composed: true,
    }));

    assertEquals('drag-start', receivedMessage?.type);
    assertEquals('1', receivedMessage?.itemId);
  });

  test('Local drag end broadcasts', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, actionElements.length);
    const firstAction = actionElements[0]!;

    // Start drag first
    firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: '1'},
      bubbles: true,
      composed: true,
    }));

    let receivedMessage: any = null;
    const listenerChannel = new BroadcastChannel('pinned-action-drag');
    listenerChannel.onmessage = (e) => {
      receivedMessage = e.data;
    };

    // Trigger dragend
    container.dispatchEvent(new CustomEvent('toolbar-action-drag-end', {
      bubbles: true,
      composed: true,
    }));

    assertEquals('drag-end', receivedMessage?.type);
  });

  test('Cross-window dragover on itself marks as placeholder', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, actionElements.length);
    const firstAction = actionElements[0]!;

    const helperChannel = new BroadcastChannel('pinned-action-drag');
    helperChannel.postMessage({type: 'drag-start', itemId: '1'});

    // Simulate dragenter on the container first to initialize placeholder
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-pinned-action'],
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
        types: ['application/x-webui-pinned-action'],
        dropEffect: 'none',
      },
    });

    firstAction.dispatchEvent(dragEvent);


    assertTrue(dragEvent.defaultPrevented);

    const keyedStates = container.keyedStates;
    assertEquals(3, keyedStates.length);
    assertEquals('1', keyedStates[0]!.key);
    assertTrue(!!keyedStates[0]!.dragPlaceholder);
  });

  test(
      'Cross-window dragenter before broadcast sets placeholder on broadcast',
      () => {
        // 1. Dispatch dragenter first (no broadcast has happened yet)
        const dragEnterEvent = new DragEvent('dragenter', {
          bubbles: true,
          cancelable: true,
          composed: true,
        });
        Object.defineProperty(dragEnterEvent, 'dataTransfer', {
          value: {
            types: ['application/x-webui-pinned-action'],
          },
        });
        container.dispatchEvent(dragEnterEvent);

        // Verify no placeholder is set yet (since we don't know the action ID)
        let keyedStates = container.keyedStates;
        assertEquals(3, keyedStates.length);
        assertTrue(!keyedStates[0]!.dragPlaceholder);
        assertTrue(!keyedStates[1]!.dragPlaceholder);
        assertTrue(!keyedStates[2]!.dragPlaceholder);

        // 2. Simulate broadcast arriving now
        const helperChannel = new BroadcastChannel('pinned-action-drag');
        helperChannel.postMessage({type: 'drag-start', itemId: '1'});

        // Verify that the placeholder is now set for Action 1
        keyedStates = container.keyedStates;
        assertEquals('1', keyedStates[0]!.key);
        assertTrue(!!keyedStates[0]!.dragPlaceholder);
      });

  test('Mousemove during local drag triggers fallback drag-end cleanup', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, actionElements.length);
    const firstAction = actionElements[0]!;

    // 1. Start a local drag
    firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
      detail: {itemId: '1'},
      bubbles: true,
      composed: true,
    }));

    // Verify it is marked as dragging/placeholder locally
    let keyedStates = container.keyedStates;
    assertTrue(!!keyedStates[0]!.dragPlaceholder);
    assertEquals('1', (container as any).draggedItemId_);

    // Set up broadcast listener to verify it broadcasts drag-end
    let receivedMessage: any = null;
    const listenerChannel = new BroadcastChannel('pinned-action-drag');
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
            container.shadowRoot.querySelectorAll('pinned-toolbar-action');
        assertEquals(2, actionElements.length);
        const firstAction = actionElements[0]!;

        // 1. Start a local drag
        firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-start', {
          detail: {itemId: '1'},
          bubbles: true,
          composed: true,
        }));

        // Verify it is marked as dragging/placeholder locally
        let keyedStates = container.keyedStates;
        assertTrue(keyedStates[0]!.dragPlaceholder === true);
        assertEquals('1', (container as any).draggedItemId_);

        // 2. Dispatch a window-level mousemove event with buttons pressed
        window.dispatchEvent(new MouseEvent('mousemove', {buttons: 1}));

        // Verify cleanup did NOT occur
        assertEquals('1', (container as any).draggedItemId_);
        keyedStates = container.keyedStates;
        assertTrue(keyedStates[0]!.dragPlaceholder === true);
      });

  test('Drop on child button triggers Mojo movePinnedToolbarAction', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, actionElements.length);
    const secondAction = actionElements[1]!;  // Action 2

    // 1. Simulate cross-window drag enter of Action 1
    const helperChannel = new BroadcastChannel('pinned-action-drag');
    helperChannel.postMessage({type: 'drag-start', itemId: '1'});

    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {types: ['application/x-webui-pinned-action']},
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
        types: ['application/x-webui-pinned-action'],
        dropEffect: 'none',
      },
    });
    secondAction.dispatchEvent(dragOverEvent);

    // Verify reorder happened locally (Action 1 is now second, i.e., index 1)
    const keyedStates = container.keyedStates;
    assertEquals('1', keyedStates[1]!.key);

    // 3. Simulate drop on Action 2 (which is now at index 0)
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dropEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-pinned-action'],
        getData: (type: string) => {
          if (type === 'application/x-webui-pinned-action') {
            return JSON.stringify({itemId: '1'});
          }
          return '';
        },
      },
    });

    secondAction.dispatchEvent(dropEvent);

    // 4. Simulate broadcast drag-end (successful drop) from the other window
    helperChannel.postMessage({type: 'drag-end', aborted: false});


    // Verify Mojo call was made to move Action 1 to the placeholder position
    // (index 1)
    assertEquals(1, moveCalls.length);
    assertEquals(1, moveCalls[0]!.itemId);
    assertEquals(1, moveCalls[0]!.index);

    // Verify layout remains in the new order after drop
    // (Action 2 remains at index 0, Action 1 at index 1 with placeholder
    // cleared)
    const postDropKeyedStates = container.keyedStates;
    assertEquals('2', postDropKeyedStates[0]!.key);
    assertEquals('1', postDropKeyedStates[1]!.key);
    assertTrue(!postDropKeyedStates[1]!.dragPlaceholder);
  });

  test('Drop on container (empty space) commits reordering', () => {
    const actionElements =
        container.shadowRoot.querySelectorAll('pinned-toolbar-action');
    assertEquals(2, actionElements.length);
    const secondAction = actionElements[1]!;  // Action 2

    // 1. Simulate cross-window drag enter and reorder
    const helperChannel = new BroadcastChannel('pinned-action-drag');
    helperChannel.postMessage({type: 'drag-start', itemId: '1'});

    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {types: ['application/x-webui-pinned-action']},
    });
    container.dispatchEvent(dragEnterEvent);

    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragOverEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-pinned-action'],
        dropEffect: 'none',
      },
    });
    secondAction.dispatchEvent(dragOverEvent);

    // Verify local reorder happened
    let keyedStates = container.keyedStates;
    assertEquals('2', keyedStates[0]!.key);
    assertEquals('1', keyedStates[1]!.key);

    // 2. Dispatch drop on the container itself (simulating drop on empty
    // space)
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dropEvent, 'dataTransfer', {
      value: {
        types: ['application/x-webui-pinned-action'],
        getData: (type: string) => {
          if (type === 'application/x-webui-pinned-action') {
            return JSON.stringify({itemId: '1'});
          }
          return '';
        },
      },
    });
    container.dispatchEvent(dropEvent);

    // Verify Mojo call was made to move Action 1 to the placeholder position
    // (index 1)
    assertEquals(1, moveCalls.length);
    assertEquals(1, moveCalls[0]!.itemId);
    assertEquals(1, moveCalls[0]!.index);

    // 3. Simulate broadcast drag-end (successful drop) from the other window
    helperChannel.postMessage({type: 'drag-end', aborted: false});

    // Verify layout remains in the new order after drop
    // (Action 2 remains at index 0, Action 1 at index 1 with placeholder
    // cleared)
    keyedStates = container.keyedStates;
    assertEquals('2', keyedStates[0]!.key);
    assertEquals('1', keyedStates[1]!.key);
    assertTrue(!keyedStates[1]!.dragPlaceholder);
    assertEquals(0, (container as any).dragEnterCount_);
  });

  test('Cross-window drag abort clears placeholder in target window', () => {
    // 1. Simulate cross-window drag enter of Action 1
    const helperChannel = new BroadcastChannel('pinned-action-drag');
    helperChannel.postMessage({type: 'drag-start', itemId: '1'});

    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    Object.defineProperty(dragEnterEvent, 'dataTransfer', {
      value: {types: ['application/x-webui-pinned-action']},
    });
    container.dispatchEvent(dragEnterEvent);

    // Verify Action 1 is placeholder
    let keyedStates = container.keyedStates;
    assertTrue(!!keyedStates[0]!.dragPlaceholder);
    assertEquals(1, (container as any).dragEnterCount_);

    // 2. Simulate broadcast drag-end (aborted)
    helperChannel.postMessage({type: 'drag-end'});

    // Verify placeholder is cleared even though dragEnterCount_ is still 1
    keyedStates = container.keyedStates;
    assertTrue(!keyedStates[0]!.dragPlaceholder);
    assertEquals(1, (container as any).dragEnterCount_);  // still inside
  });

  test(
      'Drop with mismatched action ID is ignored and layout is reverted',
      () => {
        const actionElements =
            container.shadowRoot.querySelectorAll('pinned-toolbar-action');
        assertEquals(2, actionElements.length);
        const firstAction = actionElements[0]!;

        // 1. Simulate local drag start of Action 1 on its internal button
        const firstActionButton =
            firstAction.shadowRoot.querySelector('cr-icon-button')!;
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
        firstActionButton.dispatchEvent(dragStartEvent);


        // 2. Simulate dragover Action 2 to reorder locally
        const secondAction = actionElements[1]!;
        const dragOverEvent = new DragEvent('dragover', {
          bubbles: true,
          cancelable: true,
          composed: true,
        });
        Object.defineProperty(dragOverEvent, 'dataTransfer', {
          value: {
            types: ['application/x-webui-pinned-action'],
            dropEffect: 'none',
          },
        });
        secondAction.dispatchEvent(dragOverEvent);

        // Verify local reorder happened (Action 1 is at index 1)
        let keyedStates = container.keyedStates;
        assertEquals('1', keyedStates[1]!.key);

        // 3. Simulate drop on Action 2 with mismatched Action ID (999)
        const dropEvent = new DragEvent('drop', {
          bubbles: true,
          cancelable: true,
          composed: true,
        });
        Object.defineProperty(dropEvent, 'dataTransfer', {
          value: {
            types: ['application/x-webui-pinned-action'],
            getData: (type: string) => {
              if (type === 'application/x-webui-pinned-action') {
                return JSON.stringify({itemId: '999'});  // Mismatched!
              }
              return '';
            },
          },
        });
        secondAction.dispatchEvent(dropEvent);

        // Verify NO Mojo calls were made
        assertEquals(0, moveCalls.length);

        // 4. Simulate dragend (browser will report aborted since we didn't call
        // preventDefault)
        firstAction.dispatchEvent(new CustomEvent('toolbar-action-drag-end', {
          detail: {itemId: '1', dropEffect: 'none'},  // none = aborted
          bubbles: true,
          composed: true,
        }));

        // Verify layout was reverted to original
        keyedStates = container.keyedStates;
        assertEquals('1', keyedStates[0]!.key);
        assertEquals('2', keyedStates[1]!.key);
        assertTrue(!keyedStates[0]!.dragPlaceholder);
      });

  test(
      'State updates during drag cancel the drag and apply immediately',
      async () => {
        const actionElements =
            container.shadowRoot.querySelectorAll('pinned-toolbar-action');
        assertEquals(2, actionElements.length);
        const firstAction = actionElements[0]!;

        // Set up broadcast listener
        const receivedMessages: any[] = [];
        const listenerChannel = new BroadcastChannel('pinned-action-drag');
        listenerChannel.onmessage = (e) => {
          receivedMessages.push(e.data);
        };

        // 1. Simulate local drag start of Action 1 on its internal button
        const firstActionButton =
            firstAction.shadowRoot.querySelector('cr-icon-button')!;
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
        firstActionButton.dispatchEvent(dragStartEvent);

        // Verify Action 1 is marked as placeholder and dragged ID is set
        let keyedStates = container.keyedStates;
        assertTrue(!!keyedStates[0]!.dragPlaceholder);
        assertEquals('1', (container as any).draggedItemId_);

        // Verify drag-start was broadcast
        assertEquals(1, receivedMessages.length);
        assertEquals('drag-start', receivedMessages[0].type);
        assertEquals('1', receivedMessages[0].itemId);

        // 2. Simulate a backend state update during drag (removing Action 2)
        container.states = [
          container.states[0]!,
          container.states[2]!,
        ];
        await microtasksFinished();

        // Verify drag was aborted immediately and layout updated
        assertEquals(null, (container as any).draggedItemId_);
        keyedStates = container.keyedStates;
        assertEquals(2, keyedStates.length);  // Action 1, Divider
        assertEquals('1', keyedStates[0]!.key);
        assertTrue(!keyedStates[0]!.dragPlaceholder);

        // Verify abort message was broadcasted
        assertEquals(2, receivedMessages.length);
        assertEquals('drag-end', receivedMessages[1].type);
        assertTrue(receivedMessages[1].aborted);

        listenerChannel.close();
      });
});
