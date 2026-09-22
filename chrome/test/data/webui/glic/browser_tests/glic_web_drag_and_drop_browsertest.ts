// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path: chrome/browser/glic/host/glic_web_drag_and_drop_browsertest.cc

import {InvocationSource} from '/glic/glic_api/glic_api.js';
import type {InvokeOptions} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertEquals, testMain, waitFor} from './browser_test_base.js';

class GlicWebDragAndDropBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  private setupWebDragDropHandlers() {
    const dragEnter = Promise.withResolvers<void>();
    const dragOver = Promise.withResolvers<void>();
    const invokeDrop = Promise.withResolvers<string>();
    const state = {wasInvoked: false};

    window.addEventListener('dragenter', () => {
      dragEnter.resolve();
    }, {once: true});
    window.addEventListener('dragover', (e: DragEvent) => {
      e.preventDefault();
      dragOver.resolve();
    });

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
      invokeDropPromise: invokeDrop.promise,
      state,
    };
  }

  async testWebToGlicDragMaterialization() {
    const {dragEnterPromise, dragOverPromise, invokeDropPromise} =
        this.setupWebDragDropHandlers();

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
}

testMain([GlicWebDragAndDropBrowserTest]);
