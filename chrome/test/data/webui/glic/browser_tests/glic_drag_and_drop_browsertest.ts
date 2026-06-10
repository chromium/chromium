// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {InvocationSource} from '/glic/glic_api/glic_api.js';
import type {InvokeOptions} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertEquals, assertFalse, testMain, waitFor} from './browser_test_base.js';

class GlicDragAndDropPolicyTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  private setupDragAndDropHandlers() {
    const dragEnter = Promise.withResolvers<void>();
    const dragOver = Promise.withResolvers<void>();
    const nativeDrop = Promise.withResolvers<string>();
    const invokeDrop = Promise.withResolvers<string>();
    const state = {wasDropped: false, wasInvoked: false};

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
      nativeDrop.resolve(files && files.length > 0 ? files[0]!.name : '');
    }, {once: true});
    this.client.invokeData.subscribe((options: InvokeOptions|undefined) => {
      if (!options) {
        return;
      }
      if (options.invocationSource === InvocationSource.WEB_DRAG_DROP) {
        state.wasInvoked = true;
        if (!options.context) {
          invokeDrop.reject(new Error('InvokeOptions context is missing!'));
          return;
        }
        if (!options.context.parts || options.context.parts.length === 0) {
          invokeDrop.reject(new Error('InvokeOptions context has no parts!'));
          return;
        }
        let foundImage = false;
        for (const part of options.context.parts) {
          if (part.data) {
            foundImage = true;
            const blob = part.data;
            if (blob && blob.size > 0) {
              invokeDrop.resolve(part.filename || options.context.name || '');
            } else {
              invokeDrop.reject(new Error('Received empty image bytes!'));
            }
          }
        }
        if (!foundImage) {
          invokeDrop.reject(
              new Error('No parts with image data found in InvokeOptions!'));
        }
      }
    });

    return {
      dragEnterPromise: dragEnter.promise,
      dragOverPromise: dragOver.promise,
      nativeDropPromise: nativeDrop.promise,
      invokeDropPromise: invokeDrop.promise,
      state,
    };
  }

  async testDragAndDropDlp() {
    const {dragEnterPromise, dragOverPromise, nativeDropPromise} =
        this.setupDragAndDropHandlers();

    await this.advanceToNextStep();

    await waitFor(dragEnterPromise, 40000, 'DragEnter never arrived');
    await waitFor(dragOverPromise, 40000, 'DragOver never arrived');
    await this.advanceToNextStep();

    const droppedData =
        await waitFor(nativeDropPromise, 40000, 'Drop never arrived');

    assertEquals('test.txt', droppedData);
  }

  async testDragAndDropDlpBlocked() {
    const {dragEnterPromise, dragOverPromise, state} =
        this.setupDragAndDropHandlers();

    await this.advanceToNextStep();

    await waitFor(dragEnterPromise, 40000, 'DragEnter never arrived');
    await waitFor(dragOverPromise, 40000, 'DragOver never arrived');
    await this.advanceToNextStep();

    // Wait for C++ to verify the dialog.
    await this.advanceToNextStep();

    assertFalse(
        state.wasDropped, 'Drop occurred when it should have been blocked');
  }
  async testWebToGlicDragMaterialization() {
    const {dragEnterPromise, dragOverPromise, invokeDropPromise} =
        this.setupDragAndDropHandlers();

    await this.advanceToNextStep();

    await waitFor(dragEnterPromise, 40000, 'DragEnter never arrived');
    await waitFor(dragOverPromise, 40000, 'DragOver never arrived');

    const droppedData =
        await waitFor(invokeDropPromise, 40000, 'Invoke never arrived');

    assertEquals('cors-allowed.jpg', droppedData);
  }

  async testWebToGlicDragMaterializationFromDetached() {
    await this.testWebToGlicDragMaterialization();
  }

  async testWebToGlicDragDlpBlocked() {
    const {dragEnterPromise, dragOverPromise, state} =
        this.setupDragAndDropHandlers();

    await this.advanceToNextStep();

    await waitFor(dragEnterPromise, 40000, 'DragEnter never arrived');
    await waitFor(dragOverPromise, 40000, 'DragOver never arrived');

    // Wait for C++ to verify Glic invocation block.
    await this.advanceToNextStep();

    assertFalse(
        state.wasInvoked, 'Invoke occurred when it should have been blocked');
  }
}

const TEST_FIXTURES = [
  GlicDragAndDropPolicyTest,
];

testMain(TEST_FIXTURES);
