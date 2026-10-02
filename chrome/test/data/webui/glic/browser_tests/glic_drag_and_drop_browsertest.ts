// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path: chrome/browser/glic/host/glic_drag_and_drop_browsertest.cc

import {ApiTestFixtureBase, assertEquals, assertFalse, testMain, waitFor} from './browser_test_base.js';

class GlicDragAndDropBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testDragAndDropFile() {
    const dragEnter = Promise.withResolvers<void>();
    const dragOver = Promise.withResolvers<void>();
    const nativeDrop = Promise.withResolvers<File|null>();

    window.addEventListener('dragenter', () => {
      dragEnter.resolve();
    }, {once: true});
    window.addEventListener('dragover', (e: DragEvent) => {
      e.preventDefault();
      dragOver.resolve();
    });
    window.addEventListener('drop', (e: DragEvent) => {
      e.preventDefault();
      const files = e.dataTransfer?.files;
      nativeDrop.resolve(files && files.length > 0 ? files[0]! : null);
    }, {once: true});

    await this.advanceToNextStep();

    await waitFor(dragEnter.promise, 40000, 'DragEnter never arrived');
    await waitFor(dragOver.promise, 40000, 'DragOver never arrived');
    await this.advanceToNextStep();

    const droppedFile =
        await waitFor(nativeDrop.promise, 40000, 'Drop never arrived');

    assertEquals('test.txt', droppedFile?.name);
    assertEquals(
        'This is some test content for file drop.', await droppedFile?.text());
  }

  async testDragAndDropTextRejected() {
    let dragEventReceived = false;
    const onDragEvent = () => {
      dragEventReceived = true;
    };
    window.addEventListener('dragenter', onDragEvent);
    window.addEventListener('dragover', onDragEvent);
    window.addEventListener('drop', onDragEvent);

    await this.advanceToNextStep();

    assertFalse(dragEventReceived);
  }
}

testMain([GlicDragAndDropBrowserTest]);
