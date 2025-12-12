// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DragAndDropHandler, Service} from 'chrome://extensions/extensions.js';

import {TestService} from './test_service.js';

// Allows for mocking `DataTransfer`.
// We need to override the items property because `DataTransfer.items` is
// readonly and we can't easily add a mock item that returns `null` for
// `webkitGetAsEntry()` via the standard API. So we'll define a property on
// the event instance.
class MockDataTransferItem {
  kind: string;
  type: string;
  entry: FileSystemEntry|null;

  constructor(kind: string, type: string, entry: FileSystemEntry|null) {
    this.kind = kind;
    this.type = type;
    this.entry = entry;
  }

  webkitGetAsEntry() {
    return this.entry;
  }
}

suite('DragAndDropHandlerUnitTest', function() {
  let handler: DragAndDropHandler;
  let mockService: TestService;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockService = new TestService();
    Service.setInstance(mockService);
    handler = new DragAndDropHandler(true, document.body);
  });

  // Tests that the handler does not crash when `webkitGetAsEntry()` returns
  // null. This can happen when the dropped item is not a file, or if the
  // `DataTransferItem` is not in read/write mode. Regression test for
  // crbug.com/466050994.
  test('DragWithNullEntry', function() {
    const event = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    // Mock `DataTransfer`.
    Object.defineProperty(event, 'dataTransfer', {
      value: {
        files: {length: 1, 0: {name: 'test.png'}},
        types: ['Files'],
        items: [new MockDataTransferItem(
            /* kind = */ 'file', /* type = */ 'image/png', /* entry = */ null)],
        dropEffect: 'none',
        effectAllowed: 'all',
      },
    });

    // This should not throw "TypeError: Cannot read properties of null
    // (reading 'isDirectory')"
    handler.doDrop(event);
  });

  // Tests that the handler correctly detects a directory drop and calls
  // `loadUnpackedFromDrag`.
  test('DragDirectory', function() {
    const event = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    const mockEntry = {isDirectory: true} as FileSystemEntry;

    // Mock `DataTransfer`.
    Object.defineProperty(event, 'dataTransfer', {
      value: {
        files: {length: 1, 0: {name: 'folder'}},
        types: ['Files'],
        items: [new MockDataTransferItem(/* kind= */ 'file', /* type = */ '',
                                         /* entry = */ mockEntry)],
        dropEffect: 'none',
        effectAllowed: 'all',
      },
    });

    handler.doDrop(event);
    return mockService.whenCalled('loadUnpackedFromDrag');
  });

  // Tests that the handler correctly detects a file drop and calls
  // `installDroppedFile`.
  test('DragFile', function() {
    const event = new DragEvent('drop', {
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    const mockEntry = {isDirectory: false} as FileSystemEntry;

    // Mock `DataTransfer`.
    Object.defineProperty(event, 'dataTransfer', {
      value: {
        files: {length: 1, 0: {name: 'test.crx'}},
        types: ['Files'],
        items: [new MockDataTransferItem(
            /* kind= */ 'file', /* type = */ '', /* entry = */ mockEntry)],
        dropEffect: 'none',
        effectAllowed: 'all',
      },
    });

    handler.doDrop(event);
    return mockService.whenCalled('installDroppedFile');
  });
});
