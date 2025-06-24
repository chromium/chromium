// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxPageHandlerRemote} from 'chrome://new-tab-page/composebox.mojom-webui.js';
import {ComposeboxElement, ComposeboxProxyImpl} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
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
    dataTransfer.items.add(new File(['foo'], 'foo.jpg', {type: 'image/jpeg'}));
    composeboxElement.$.imageInput.files = dataTransfer.files;
    composeboxElement.$.imageInput.dispatchEvent(new Event('change'));
    await microtasksFinished();

    // Assert one image file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'image/jpeg');
    assertEquals(files[0]!.name, 'foo.jpg');
    assertTrue(!!files[0]!.objectUrl);
  });

  test('upload pdf', async () => {
    createComposeboxElement();

    // Assert no files.
    assertEquals(composeboxElement.$.carousel.files.length, 0);

    // Arrange.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo'], 'foo.pdf', {type: 'application/pdf'}));
    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(new Event('change'));
    await microtasksFinished();

    // Assert one pdf file.
    const files = composeboxElement.$.carousel.files;
    assertEquals(files.length, 1);
    assertEquals(files[0]!.type, 'application/pdf');
    assertEquals(files[0]!.name, 'foo.pdf');
    assertFalse(!!files[0]!.objectUrl);
  });

  test('delete file', async () => {
    createComposeboxElement();

    // Arrange.
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(
        new File(['foo1'], 'foo1.pdf', {type: 'application/pdf'}));
    dataTransfer.items.add(
        new File(['foo2'], 'foo2.pdf', {type: 'application/pdf'}));
    composeboxElement.$.fileInput.files = dataTransfer.files;
    composeboxElement.$.fileInput.dispatchEvent(new Event('change'));
    await microtasksFinished();

    // Assert two files.
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
});
