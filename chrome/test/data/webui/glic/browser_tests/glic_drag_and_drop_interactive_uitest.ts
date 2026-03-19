// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ApiTestFixtureBase, testMain, waitFor} from './browser_test_base.js';

class GlicDragAndDropPolicyTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  private setupDragAndDropHandlers() {
    const dragEnter = Promise.withResolvers<void>();
    const dragOver = Promise.withResolvers<void>();
    const drop = Promise.withResolvers<string>();
    const state = {wasDropped: false};

    window.addEventListener('dragenter', () => {
      dragEnter.resolve();
    }, {once: true});
    window.addEventListener('dragover', (e: DragEvent) => {
      e.preventDefault();
      dragOver.resolve();
    });
    window.addEventListener('drop', (e: DragEvent) => {
      e.preventDefault();
      state.wasDropped = true;
      const files = e.dataTransfer?.files;
      drop.resolve(files && files.length > 0 ? files[0]!.name : '');
    }, {once: true});

    return {
      dragEnterPromise: dragEnter.promise,
      dragOverPromise: dragOver.promise,
      dropPromise: drop.promise,
      state,
    };
  }

  async testDragAndDropDlp() {
    const {dragEnterPromise, dragOverPromise, dropPromise} =
        this.setupDragAndDropHandlers();

    await this.advanceToNextStep();

    await dragEnterPromise;
    await dragOverPromise;
    await this.advanceToNextStep('drag-ready');

    const droppedData = await waitFor(dropPromise, 40000, 'Drop never arrived');

    await this.advanceToNextStep(droppedData);
  }

  async testDragAndDropDlpBlocked() {
    const {dragEnterPromise, dragOverPromise, state} =
        this.setupDragAndDropHandlers();

    await this.advanceToNextStep();

    await dragEnterPromise;
    await dragOverPromise;
    await this.advanceToNextStep('drag-ready');

    // Wait for C++ to verify the dialog.
    await this.advanceToNextStep('final-check');

    if (state.wasDropped) {
      throw new Error('Drop occurred when it should have been blocked');
    }
  }
}

const TEST_FIXTURES = [
  GlicDragAndDropPolicyTest,
];

testMain(TEST_FIXTURES);
