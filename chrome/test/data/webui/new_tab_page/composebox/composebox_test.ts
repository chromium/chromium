// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxPageHandlerRemote} from 'chrome://new-tab-page/composebox.mojom-webui.js';
import {ComposeboxElement, ComposeboxProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from '../test_support.js';

suite('NewTabPageComposeboxTest', () => {
  let composeboxElement: ComposeboxElement;
  let handler: TestMock<ComposeboxPageHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        ComposeboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(mock)));
  });
  function createComposeboxElement() {
    composeboxElement = new ComposeboxElement();
    document.body.appendChild(composeboxElement);
  }

  async function waitForAddFileCallCount(expectedCount: number): Promise<void> {
    const startTime = Date.now();
    return new Promise((resolve, reject) => {
      const checkCount = () => {
        const currentCount = handler.getCallCount('addFile');
        if (currentCount === expectedCount) {
          resolve();
          return;
        }

        if (Date.now() - startTime >= 5000) {
          reject(new Error(`Could not add file ${expectedCount} times.`));
          return;
        }

        setTimeout(checkCount, 50);
      };
      checkCount();
    });
  }

  test('clear functionality', async () => {
    createComposeboxElement();

    // Check submit button disabled.
    assertEquals(
        window
            .getComputedStyle(
                $$<HTMLElement>(composeboxElement, '#submitIcon')!)
            .cursor,
        'default');

    // Add input.
    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo1'], 'foo1.pdf', {type: 'application/pdf'}));
    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(new Event('change'));

    await handler.whenCalled('addFile');
    await microtasksFinished();

    // Check submit button enabled and file uploaded.
    assertEquals(
        window
            .getComputedStyle(
                $$<HTMLElement>(composeboxElement, '#submitIcon')!)
            .cursor,
        'pointer');
    assertEquals(composeboxElement.$.carousel.files.length, 1);

    // Clear input.
    $$<HTMLElement>(composeboxElement, '#cancelIcon')!.click();
    await microtasksFinished();

    // Check submit button disabled and files empty.
    assertEquals(
        window
            .getComputedStyle(
                $$<HTMLElement>(composeboxElement, '#submitIcon')!)
            .cursor,
        'default');
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    // Close composebox.
    const whenToggleComposebox =
        eventToPromise('toggle-composebox', composeboxElement);
    $$<HTMLElement>(composeboxElement, '#cancelIcon')!.click();
    await whenToggleComposebox;
  });

  test('upload image', async () => {
    createComposeboxElement();

    // Assert no files.
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    // Act.
    const dataTransfer = new DataTransfer();

    const file = new File(['foo'], 'foo.jpg', {type: 'image/jpeg'});
    dataTransfer.items.add(file);

    // Since the `onFileChange_` method checks the event target when creating
    // the `objectUrl`, we have to mock it here.
    const mockFileChange = new Event('change', {bubbles: true});
    Object.defineProperty(mockFileChange, 'target', {
      writable: false,
      value: composeboxElement.$.imageInput,
    });
    composeboxElement.$.imageInput.files = dataTransfer.files;
    composeboxElement.$.imageInput.dispatchEvent(mockFileChange);

    await handler.whenCalled('addFile');
    await microtasksFinished();

    // Assert one image file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'image/jpeg');
    assertEquals(files[0]!.name, 'foo.jpg');
    assertTrue(!!files[0]!.objectUrl);

    assertEquals(handler.getCallCount('notifySessionStarted'), 1);

    // Assert file is uploaded.
    assertEquals(handler.getCallCount('addFile'), 1);

    const fileBuffer = await file.arrayBuffer();
    const fileArray = Array.from(new Uint8Array(fileBuffer));

    const [[fileInfo, fileData]] = handler.getArgs('addFile');
    assertEquals(fileInfo.fileName, 'foo.jpg');
    assertDeepEquals(fileData.bytes, fileArray);
  });

  test('upload pdf', async () => {
    createComposeboxElement();

    // Assert no files.
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    // Arrange.
    const dataTransfer = new DataTransfer();
    const file = new File(['foo'], 'foo.pdf', {type: 'application/pdf'});
    dataTransfer.items.add(file);
    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(new Event('change'));

    await handler.whenCalled('addFile');
    await microtasksFinished();

    // Assert one pdf file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'application/pdf');
    assertEquals(files[0]!.name, 'foo.pdf');
    assertFalse(!!files[0]!.objectUrl);

    assertEquals(handler.getCallCount('notifySessionStarted'), 1);

    const fileBuffer = await file.arrayBuffer();
    const fileArray = Array.from(new Uint8Array(fileBuffer));

    // Assert file is uploaded.
    assertEquals(handler.getCallCount('addFile'), 1);
    const [[fileInfo, fileData]] = handler.getArgs('addFile');
    assertEquals(fileInfo.fileName, 'foo.pdf');
    assertDeepEquals(fileData.bytes, fileArray);
  });

  test('delete file', async () => {
    createComposeboxElement();

    // Arrange.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo'], 'foo.pdf', {type: 'application/pdf'}));
    dataTransfer.items.add(
        new File(['foo2'], 'foo2.pdf', {type: 'application/pdf'}));

    // Since the `onFileChange_` method checks the event target when creating
    // the `objectUrl`, we have to mock it here.
    const mockFileChange = new Event('change', {bubbles: true});
    Object.defineProperty(mockFileChange, 'target', {
      writable: false,
      value: composeboxElement.$.fileInput,
    });

    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(mockFileChange);

    await waitForAddFileCallCount(2);
    await composeboxElement.updateComplete;
    await microtasksFinished();

    // Assert two files are present initially.
    assertEquals(composeboxElement.$.carousel.files.length, 2);

    // Act.
    composeboxElement.$.carousel.dispatchEvent(new CustomEvent('delete-file', {
      detail: {
        uuid: composeboxElement.$.carousel.files[0]!.uuid,
      },
      bubbles: true,
      composed: true,
    }));

    await microtasksFinished();

    // Assert.
    assertEquals(composeboxElement.$.carousel.files.length, 1);
  });

  test('NotifySessionStarted called on composebox created', () => {
    // Assert call has not occurred.
    assertEquals(handler.getCallCount('notifySessionStarted'), 0);

    createComposeboxElement();

    // Assert call occurs.
    assertEquals(handler.getCallCount('notifySessionStarted'), 1);
  });

  test('image upload button clicks file input', async () => {
    const imageUploadEventPromise =
        eventToPromise('click', composeboxElement.$.imageInput);
    composeboxElement.$.imageUploadButton.click();

    // Assert.
    await imageUploadEventPromise;
  });

  test('file upload button clicks file input', async () => {
    const fileUploadClickEventPromise =
        eventToPromise('click', composeboxElement.$.fileInput);
    composeboxElement.$.fileUploadButton.click();

    // Assert.
    await fileUploadClickEventPromise;
  });

  test('session abandoned on esc click', async () => {
    // Arrange.
    createComposeboxElement();

    // Assert call has not occurred.
    assertEquals(handler.getCallCount('notifySessionAbandoned'), 0);

    // Assert call occurs.
    composeboxElement.$.composebox.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Escape'}));
    await microtasksFinished();
    assertEquals(handler.getCallCount('notifySessionAbandoned'), 1);
  });

  test('session abandoned on cancel button click', async () => {
    // Arrange.
    createComposeboxElement();

    // Assert call has not occurred.
    assertEquals(handler.getCallCount('notifySessionAbandoned'), 0);

    // Close composebox.
    const whenToggleComposebox =
        eventToPromise('toggle-composebox', composeboxElement);
    const cancelIcon = $$<HTMLElement>(composeboxElement, '#cancelIcon');
    assertTrue(!!cancelIcon);
    cancelIcon.click();
    await whenToggleComposebox;
    assertEquals(handler.getCallCount('notifySessionAbandoned'), 1);
  });

  test('submit button click leads to handler called', async () => {
    createComposeboxElement();
    // Assert.
    assertEquals(handler.getCallCount('submitQuery'), 0);

    // Arrange.
    composeboxElement.$.input.value = 'test';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const submitIcon = $$<HTMLElement>(composeboxElement, '#submitIcon');
    assertTrue(!!submitIcon);
    submitIcon.click();
    await microtasksFinished();

    // Assert call occurs.
    assertEquals(handler.getCallCount('submitQuery'), 1);
  });

  test('empty input does not lead to submission', async () => {
    createComposeboxElement();
    // Assert.
    assertEquals(handler.getCallCount('submitQuery'), 0);

    // Arrange.
    composeboxElement.$.input.value = '';
    composeboxElement.$.input.dispatchEvent(new Event('input'));
    const submitIcon = $$<HTMLElement>(composeboxElement, '#submitIcon');
    assertTrue(!!submitIcon);
    submitIcon.click();
    await microtasksFinished();

    // Assert call does not occur.
    assertEquals(handler.getCallCount('submitQuery'), 0);
  });
});
