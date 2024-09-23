// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {LensUploadDialogElement} from 'chrome://new-tab-page/lazy_load.js';
import {LensErrorType, LensSubmitType, LensUploadDialogAction, LensUploadDialogError} from 'chrome://new-tab-page/lazy_load.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('LensUploadDialogTest', () => {
  let uploadDialog: LensUploadDialogElement;
  let wrapperElement: HTMLDivElement;
  let outsideClickTarget: HTMLDivElement;
  let windowProxy: TestMock<WindowProxy>;
  let metrics: MetricsTracker;

  let submitUrlCalled = false;
  let submittedUrl: string|null = null;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metrics = fakeMetricsPrivate();
    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('onLine', true);

    // Larger than wrapper so that we can test outside clicks.
    document.body.style.width = '1000px';

    wrapperElement = document.createElement('div');
    // Rough approximate size of the realbox.
    wrapperElement.style.width = '500px';
    wrapperElement.style.margin = '0 auto';
    document.body.appendChild(wrapperElement);

    // Click target to test outside clicks.
    outsideClickTarget = document.createElement('div');
    outsideClickTarget.style.width = '50px';
    outsideClickTarget.style.height = '50px';
    outsideClickTarget.style.border = '1px dashed red';
    document.body.appendChild(outsideClickTarget);

    uploadDialog = document.createElement('ntp-lens-upload-dialog');
    wrapperElement.appendChild(uploadDialog);
    await microtasksFinished();

    uploadDialog.$.lensForm.submitUrl = (url: string) => {
      submitUrlCalled = true;
      submittedUrl = url;
      return Promise.resolve();
    };
  });

  teardown(() => {
    submitUrlCalled = false;
    submittedUrl = null;
  });

  test('creating ntp lens dialog opens containing dialog element', () => {
    // Assert.
    assertFalse(uploadDialog.$.dialog.hidden);
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.DIALOG_OPENED));
  });

  test('hides when close button is clicked', async () => {
    // Act.
    const closeButton =
        uploadDialog.shadowRoot!.querySelector<HTMLElement>('#closeButton');
    assertTrue(!!closeButton);
    closeButton.click();
    await microtasksFinished();

    // Assert.
    assertTrue(uploadDialog.$.dialog.hidden);
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.DIALOG_CLOSED));
  });

  test('focusing outside the upload dialog closes the dialog', async () => {
    // Arrange.
    const event =
        new FocusEvent('focusout', {relatedTarget: outsideClickTarget});

    // Act.
    uploadDialog.$.dialog.dispatchEvent(event);
    await microtasksFinished();

    // Assert.
    assertTrue(uploadDialog.$.dialog.hidden);
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.DIALOG_CLOSED));
  });

  test(
      'focusing inside the upload dialog does not close the dialog',
      async () => {
        // Arrange.
        const event = new FocusEvent(
            'focusout', {relatedTarget: uploadDialog.$.closeButton});

        // Act.
        uploadDialog.$.dialog.dispatchEvent(event);
        await microtasksFinished();

        // Assert.
        assertFalse(uploadDialog.$.dialog.hidden);
        assertEquals(
            0,
            metrics.count(
                'NewTabPage.Lens.UploadDialog.DialogAction',
                LensUploadDialogAction.DIALOG_CLOSED));
      });

  test(
      'focusout with null related target closes the dialog when doc has focus',
      async () => {
        // Arrange.
        const event = new FocusEvent('focusout', {relatedTarget: null});

        // Act.
        (document.activeElement as HTMLElement).focus();
        uploadDialog.$.dialog.dispatchEvent(event);
        await microtasksFinished();

        // Assert.
        assertTrue(uploadDialog.$.dialog.hidden);
        assertEquals(
            1,
            metrics.count(
                'NewTabPage.Lens.UploadDialog.DialogAction',
                LensUploadDialogAction.DIALOG_CLOSED));
      });

  test(
      'focusout with null related target closes the dialog when doc does not have focus',
      async () => {
        // Arrange.
        const event = new FocusEvent('focusout', {relatedTarget: null});

        // Act.
        const nativeHasFocus = document.hasFocus;
        document.hasFocus = () => {
          return false;
        };
        uploadDialog.$.dialog.dispatchEvent(event);
        await microtasksFinished();

        // Assert.
        assertFalse(uploadDialog.$.dialog.hidden);
        assertEquals(
            0,
            metrics.count(
                'NewTabPage.Lens.UploadDialog.DialogAction',
                LensUploadDialogAction.DIALOG_CLOSED));

        document.hasFocus = nativeHasFocus;
      });

  test('focusout that occurs during drag does not close dialog', async () => {
    // Arrange.
    const focusEvent = new FocusEvent('focusout', {relatedTarget: null});
    const dragEvent = new DragEvent('dragenter');
    // Act.
    uploadDialog.$.dragDropArea.dispatchEvent(dragEvent);
    uploadDialog.$.dialog.dispatchEvent(focusEvent);
    await microtasksFinished();

    // Assert.
    assertFalse(uploadDialog.$.dialog.hidden);
    assertEquals(
        0,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.DIALOG_CLOSED));
  });

  test('clicking esc key closes the dialog', async () => {
    // Act.
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    await microtasksFinished();

    // Assert.
    assertTrue(uploadDialog.$.dialog.hidden);
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.DIALOG_CLOSED));
  });

  test('opening dialog while offline shows offline UI', async () => {
    // Arrange.
    uploadDialog.remove();
    windowProxy.setResultFor('onLine', false);

    // Act.
    uploadDialog = document.createElement('ntp-lens-upload-dialog');
    wrapperElement.appendChild(uploadDialog);
    await microtasksFinished();

    // Assert.
    assertTrue(
        isVisible(uploadDialog.shadowRoot!.querySelector('#offlineContainer')));

    // Reset.
    windowProxy.setResultFor('onLine', true);
  });

  test(
      'clicking try again in offline state when online updates UI',
      async () => {
        // Arrange.
        uploadDialog.remove();
        windowProxy.setResultFor('onLine', false);

        // Act.
        uploadDialog = document.createElement('ntp-lens-upload-dialog');
        wrapperElement.appendChild(uploadDialog);
        await microtasksFinished();

        // Assert. (consistency check)
        assertTrue(isVisible(
            uploadDialog.shadowRoot!.querySelector('#offlineContainer')));

        // Arrange.
        windowProxy.setResultFor('onLine', true);

        // Act.
        uploadDialog.shadowRoot!
            .querySelector<HTMLElement>('#offlineRetryButton')!.click();
        await microtasksFinished();

        // Assert.
        assertFalse(isVisible(
            uploadDialog.shadowRoot!.querySelector('#offlineContainer')));
      });

  test('submit url does not submit with empty url', async () => {
    // Act.
    clickInputSubmit();

    // Assert.
    assertFalse(submitUrlCalled);
  });

  test(
      'submit valid url by clicking submit button should submit ', async () => {
        // Arrange.
        const url = 'http://google.com/image.png';

        // Act.
        setInputBoxValue(url);
        clickInputSubmit();

        // Assert.
        assertTrue(submitUrlCalled);
        assertEquals(url, submittedUrl);
      });

  test('pressing enter in input box should submit valid url', async () => {
    // Arrange.
    const url = 'http://google.com/image.png';

    // Act.
    setInputBoxValue(url);
    getInputBox().dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));

    // Assert.
    assertTrue(submitUrlCalled);
    assertEquals(url, submittedUrl);
  });

  test('pressing enter in search button should submit valid url', async () => {
    // Arrange.
    const url = 'http://google.com/image.png';

    // Act.
    setInputBoxValue(url);
    getInputSubmit().dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));

    // Assert.
    assertTrue(submitUrlCalled);
    assertEquals(url, submittedUrl);
  });

  test('pressing space in search button should submit valid url', async () => {
    // Arrange.
    const url = 'http://google.com/image.png';

    // Act.
    setInputBoxValue(url);
    getInputSubmit().dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));

    // Assert.
    assertTrue(submitUrlCalled);
    assertEquals(url, submittedUrl);
  });

  // TODO (crbug/1399340): De-flake this test.
  // test('dragenter event should transition to dragging state', async () => {
  //   // Arrange.
  //   uploadDialog.openDialog();
  //   await microtasksFinished();
  //   // Act.
  //   uploadDialog.$.dragDropArea.dispatchEvent(new DragEvent('dragenter'));
  //   await microtasksFinished();
  //   // Assert.
  //   assertTrue(uploadDialog.hasAttribute('is-dragging_'));
  // });

  test(
      'dragenter then dragleave event should transition to normal state',
      async () => {
        // Arrange.
        uploadDialog.$.dragDropArea.dispatchEvent(new DragEvent('dragenter'));
        await microtasksFinished();
        // Act.
        uploadDialog.$.dragDropArea.dispatchEvent(new DragEvent('dragleave'));
        await microtasksFinished();
        // Assert.
        assertTrue(isVisible(
            uploadDialog.shadowRoot!.querySelector('#urlUploadContainer')));
      });

  test('drop event should submit files', async () => {
    // Arrange.
    let submitFileListCalled = false;
    uploadDialog.$.lensForm.submitFileList = async (_fileList: FileList) => {
      submitFileListCalled = true;
    };
    // Act.
    const dataTransfer = new DataTransfer();
    const file = new File([], 'image-file.png', {type: 'image/png'});
    dataTransfer.items.add(file);
    uploadDialog.$.dragDropArea.dispatchEvent(
        new DragEvent('drop', {dataTransfer}));
    await microtasksFinished();
    // Assert.
    assertTrue(submitFileListCalled);
  });

  test('shows error state when FILE_TYPE error is dispatched', async () => {
    // Act.
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('error', {
      detail: LensErrorType.FILE_TYPE,
    }));
    await microtasksFinished();

    // Assert.
    assertTrue(
        isVisible(uploadDialog.shadowRoot!.querySelector('#dragDropError')));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.ERROR_SHOWN));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogError',
            LensUploadDialogError.FILE_TYPE));
  });

  test('clears error state when NO_FILE error is dispatched', async () => {
    // Act.
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('error', {
      detail: LensErrorType.FILE_TYPE,
    }));
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('error', {
      detail: LensErrorType.NO_FILE,
    }));
    await microtasksFinished();

    // Assert.
    assertFalse(
        isVisible(uploadDialog.shadowRoot!.querySelector('#dragDropError')));
  });

  test('shows loading state when file is submitted', async () => {
    // Act.
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('loading', {
      detail: LensSubmitType.FILE,
    }));
    await microtasksFinished();

    // Assert.
    assertTrue(
        isVisible(uploadDialog.shadowRoot!.querySelector('#loadingContainer')));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.FILE_SUBMITTED));
  });

  test('shows loading state when URL is submitted', async () => {
    // Act.
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('loading', {
      detail: LensSubmitType.URL,
    }));
    await microtasksFinished();

    // Assert.
    assertTrue(
        isVisible(uploadDialog.shadowRoot!.querySelector('#loadingContainer')));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.URL_SUBMITTED));
  });

  function getInputBox(): HTMLInputElement {
    return uploadDialog.shadowRoot!.querySelector('#inputBox')!;
  }

  function setInputBoxValue(value: string) {
    const inputBox = getInputBox();
    inputBox.value = value;
    inputBox.dispatchEvent(new InputEvent('input'));
  }

  function getInputSubmit(): HTMLInputElement {
    return uploadDialog.shadowRoot!.querySelector('#inputSubmit')!;
  }

  function clickInputSubmit() {
    const inputSubmit = getInputSubmit();
    inputSubmit.click();
  }
});
