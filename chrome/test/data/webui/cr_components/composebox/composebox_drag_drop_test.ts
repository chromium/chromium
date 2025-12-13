// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://new-tab-page/strings.m.js';
import 'chrome://resources/cr_components/composebox/composebox.js';
import 'chrome://resources/cr_components/composebox/file_carousel.js';

import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {DragAndDropHandler} from 'chrome://resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from 'chrome://resources/cr_components/search/drag_drop_host.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './composebox_test_utils.js';


const ADD_FILE_CONTEXT_FN = 'addFileContext';

// Creates drag event that is compatible across all OS's + w/bots
function createDragEvent(type: string, files: File[]): DragEvent {
  const event = new DragEvent(type, {
    bubbles: true,
    cancelable: true,
    composed: true,
  });

  /* Plain object for cross-OS compatibility
   * + usable by test bots (not stripped by browser)
    d*/
  const mockDataTransfer = {
    files: files,
    types: ['Files', 'text/plain'],
    items: files.map(f => ({
                       kind: 'file',
                       type: f.type,
                       getAsFile: () => f,
                     })),
    effectAllowed: 'all',
    dropEffect: 'copy',
    getData: () => '',
  };

  // Forcefully overwrite the read-only 'dataTransfer' property on the event
  Object.defineProperty(event, 'dataTransfer', {
    value: mockDataTransfer,
  });
  return event;
}

async function dispatchDragAndDropEvent(hostElement: Element, files: File[]) {
  const dropZone = hostElement.shadowRoot!.querySelector('#composebox');

  if (!dropZone) {
    throw new Error(
        'dispatchDragAndDropEvent: #composebox drop zone not rendered.');
  }

  const enterEvent = createDragEvent('dragenter', files);
  dropZone.dispatchEvent(enterEvent);
  await microtasksFinished();

  const overEvent = createDragEvent('dragover', files);
  dropZone.dispatchEvent(overEvent);
  await microtasksFinished();

  const dropEvent = createDragEvent('drop', files);
  dropZone.dispatchEvent(dropEvent);
  await microtasksFinished();
}

class MockDragAndDropHost implements DragAndDropHost {
  isDraggingFile: boolean = false;
  animationState: GlowAnimationState = GlowAnimationState.NONE;
  dragAndDropEnabled: boolean = true;
  droppedFiles: FileList|null = null;

  addDroppedFilesCallCount: number = 0;

  getDropTarget() {
    // Return object that has addFiles arrow function.
    return {
      addDroppedFiles: (files: FileList) => {
        this.addDroppedFilesCallCount++;
        this.droppedFiles = files;
      },
    };
  }
}

// --- SUITE 1: Unit Tests for the Logic Handler ---
suite('DragAndDropHandler', () => {
  let handler: DragAndDropHandler;
  let mockHost: MockDragAndDropHost;

  setup(() => {
    mockHost = new MockDragAndDropHost();
    handler = new DragAndDropHandler(mockHost, true);
  });

  test('handleDragEnter sets dragging state', () => {
    const dragEvent =
        new DragEvent('dragenter', {bubbles: true, composed: true});

    let preventDefaultCalled = false;
    dragEvent.preventDefault = () => {
      preventDefaultCalled = true;
    };

    handler.handleDragEnter(dragEvent);

    assertTrue(preventDefaultCalled);
    assertTrue(mockHost.isDraggingFile);
    assertEquals(GlowAnimationState.DRAGGING, mockHost.animationState);
  });

  test('handleDragLeave resets dragging state', () => {
    mockHost.isDraggingFile = true;
    const dragEnterEvent =
        new DragEvent('dragenter', {bubbles: true, composed: true});
    handler.handleDragEnter(dragEnterEvent);

    mockHost.animationState = GlowAnimationState.DRAGGING;
    const dragLeaveEvent =
        new DragEvent('dragleave', {bubbles: true, composed: true});

    handler.handleDragLeave(dragLeaveEvent);

    assertFalse(mockHost.isDraggingFile);
    assertEquals(GlowAnimationState.NONE, mockHost.animationState);
  });

  test('handleDragLeave ignores events to internal children', () => {
    mockHost.isDraggingFile = true;
    const childElement = document.createElement('div');
    const parentElement = document.createElement('div');
    parentElement.appendChild(childElement);

    // Simulate leaving the parent to go into the child
    const dragEvent = new DragEvent('dragleave', {relatedTarget: childElement});
    Object.defineProperty(dragEvent, 'currentTarget', {value: parentElement});

    handler.handleDragLeave(dragEvent);

    assertTrue(
        mockHost.isDraggingFile,
        'State should not change when leaving to a child');
  });

  test('handleDrop adds files and resets state', () => {
    mockHost.isDraggingFile = true;
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(new File([''], 'test.txt'));
    const dropEvent = new DragEvent('drop', {dataTransfer});

    let preventDefaultCalled = false;
    dropEvent.preventDefault = () => {
      preventDefaultCalled = true;
    };

    handler.handleDrop(dropEvent);

    assertTrue(preventDefaultCalled);
    assertEquals(1, mockHost.addDroppedFilesCallCount);
    assertEquals('test.txt', mockHost.droppedFiles![0]!.name);
    assertFalse(mockHost.isDraggingFile);
    assertEquals(GlowAnimationState.NONE, mockHost.animationState);
  });

  test('handleDragOver prevents default to allow drop', () => {
    const dragEvent = new DragEvent('dragover');
    let preventDefaultCalled = false;
    dragEvent.preventDefault = () => {
      preventDefaultCalled = true;
    };

    handler.handleDragOver(dragEvent);

    assertTrue(preventDefaultCalled);
  });

  test('handler does nothing when disabled', () => {
    mockHost.dragAndDropEnabled = false;

    handler.handleDragEnter(new DragEvent('dragenter'));
    assertTrue(mockHost.isDraggingFile);

    handler.handleDrop(new DragEvent('drop'));
    assertEquals(0, mockHost.addDroppedFilesCallCount);
  });
});

// --- SUITE 2: Integration Tests for the Element ---
suite('ComposeboxDragAndDrop', () => {
  let composeboxElement: ComposeboxElement;
  let searchboxHandler: TestMock<SearchboxPageHandlerRemote>;
  let windowProxy: TestMock<WindowProxy>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Set up ComposeboxProxyImpl
    installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);
    searchboxHandler.setResultFor('getRecentTabs', Promise.resolve({tabs: []}));

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('setTimeout', 0);

    loadTimeData.overrideValues({
      'composeboxContextDragAndDropEnabled': true,
      'composeboxFileMaxCount': 4,
      'composeboxFileMaxSize': 10000000,
    });
  });

  teardown(() => {
    loadTimeData.overrideValues({
      'composeboxFileMaxCount': 4,
      'composeboxFileMaxSize': 1000000,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    searchboxHandler.reset();
  });

  async function createComposeboxElement() {
    composeboxElement = document.createElement('cr-composebox');
    document.body.appendChild(composeboxElement);
    await composeboxElement.updateComplete;
  }

  test('sets is-dragging-file attribute on dragenter', async () => {
    await createComposeboxElement();
    await microtasksFinished();
    const dropZone = composeboxElement.shadowRoot.querySelector('#composebox');
    assertTrue(!!dropZone);

    assertFalse(composeboxElement.hasAttribute('is-dragging-file'));

    dropZone?.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertTrue(composeboxElement.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.DRAGGING, composeboxElement.animationState);
  });

  test('removes is-dragging-file attribute on dragleave', async () => {
    await createComposeboxElement();
    await microtasksFinished();
    const dropZone = composeboxElement.shadowRoot.querySelector('#composebox');
    assertTrue(!!dropZone);

    composeboxElement.animationState = GlowAnimationState.DRAGGING;
    dropZone?.dispatchEvent(new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
    }));
    dropZone?.dispatchEvent(new DragEvent('dragleave', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertFalse(composeboxElement.hasAttribute('is-dragging-file'));
    assertEquals(GlowAnimationState.NONE, composeboxElement.animationState);
  });

  test('accepts a dropped file and adds it to the carousel', async () => {
    await createComposeboxElement();
    await microtasksFinished();
    // Same token for auto inject (mac) and manual (linux/windows)
    const sharedToken = '12345678-1234-1234-1234-123456789abc';
    searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve({token: sharedToken}));

    const file = new File(['content'], 'foo.pdf', {type: 'application/pdf'});
    // Automatically add file (Mac)
    await dispatchDragAndDropEvent(composeboxElement, [file]);

    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);
    assertEquals(1, searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
    assertFalse(composeboxElement.hasAttribute('is-dragging-file'));

    const context = composeboxElement.$.context;
    // Mock backend response: manually add file to frontend to render it in the
    // frontend
    const mockAddedFile: ComposeboxFile = {
      uuid: sharedToken,
      name: 'foo.pdf',
      status: 0,
      type: 'application/pdf',
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
    };
    context.onFileContextAdded(mockAddedFile);
    await microtasksFinished();
    await context.updateComplete;
    await microtasksFinished();

    const carousel: ComposeboxFileCarouselElement|null =
        context.shadowRoot.querySelector('cr-composebox-file-carousel');

    assertTrue(!!carousel, 'Carousel should render');

    const carouselFiles = carousel.files;
    assertEquals(1, carouselFiles.length);
    assertEquals('foo.pdf', carouselFiles[0]!.name);
  });

  test('does not accept a dropped file that is too large', async () => {
    const sampleFileMaxSize = 10;  // bytes
    loadTimeData.overrideValues({'composeboxFileMaxSize': sampleFileMaxSize});
    await createComposeboxElement();
    await microtasksFinished();

    // Create a file 1 byte larger than max
    const blob = new Blob([new Uint8Array(sampleFileMaxSize + 1)]);
    const testFile =
        new File([blob], 'largefile.pdf', {type: 'application/pdf'});
    await dispatchDragAndDropEvent(composeboxElement, [testFile]);

    assertEquals(0, searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('does not accept wrong file type', async () => {
    await createComposeboxElement();
    await microtasksFinished();

    const testFile =
        new File(['foo'], 'malware.exe', {type: 'application/x-msdownload'});
    await dispatchDragAndDropEvent(composeboxElement, [testFile]);

    assertEquals(0, searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));
  });

  test('does not accept multiple files if only one allowed', async () => {
    loadTimeData.overrideValues({'composeboxFileMaxCount': 1});
    await createComposeboxElement();
    await microtasksFinished();
    const sharedToken = '12345678-1234-1234-1234-123456789abc';
    searchboxHandler.setResultFor(
        ADD_FILE_CONTEXT_FN, Promise.resolve({token: sharedToken}));

    const file1 = new File(['a'], 'a.pdf', {type: 'application/pdf'});
    const file2 = new File(['b'], 'b.pdf', {type: 'application/pdf'});

    await dispatchDragAndDropEvent(composeboxElement, [file1, file2]);

    await searchboxHandler.whenCalled(ADD_FILE_CONTEXT_FN);

    assertEquals(1, searchboxHandler.getCallCount(ADD_FILE_CONTEXT_FN));

    // Mock adding file in frontend from backend
    const context = composeboxElement.$.context;
    const mockAddedFile: ComposeboxFile = {
      uuid: sharedToken,
      name: 'a.pdf',
      status: 0,
      type: 'application/pdf',
      isDeletable: true,
      objectUrl: null,
      dataUrl: null,
      url: null,
      tabId: null,
    };
    context.onFileContextAdded(mockAddedFile);
    await microtasksFinished();
    await context.updateComplete;
    await microtasksFinished();

    const carousel: ComposeboxFileCarouselElement|null =
        context.shadowRoot.querySelector('cr-composebox-file-carousel');

    assertTrue(!!carousel, 'Carousel should render');

    const carouselFiles = carousel.files;
    assertEquals(1, carouselFiles.length);
    assertEquals('a.pdf', carouselFiles[0]?.name);
  });
});
