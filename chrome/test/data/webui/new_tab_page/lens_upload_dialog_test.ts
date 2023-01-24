// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/new_tab_page.js';

import {LensErrorType, LensSubmitType, LensUploadDialogAction, LensUploadDialogElement, LensUploadDialogError} from 'chrome://new-tab-page/lazy_load.js';
import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeMetricsPrivate, MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {installMock} from './test_support.js';

suite('LensUploadDialogTest', () => {
  let uploadDialog: LensUploadDialogElement;
  let wrapperElement: HTMLDivElement;
  let outsideClickTarget: HTMLDivElement;
  let windowProxy: TestBrowserProxy<WindowProxy>;
  let metrics: MetricsTracker;

  let submitUrlCalled = false;
  let submittedUrl: string|null = null;

  setup(() => {
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

    uploadDialog.$.lensForm.submitUrl = (url: string) => {
      submitUrlCalled = true;
      submittedUrl = url;
    };
  });

  teardown(() => {
    submitUrlCalled = false;
    submittedUrl = null;
  });

  test('hidden be default', () => {
    // Assert.
    assertTrue(uploadDialog.$.dialog.hidden);
  });

  test('shows when openDialog is called', async () => {
    // Act.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Assert.
    assertFalse(uploadDialog.$.dialog.hidden);
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.DIALOG_OPENED));
  });

  test('hides when close button is clicked', async () => {
    // Arrange.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Act.
    const closeButton =
      uploadDialog.shadowRoot!.querySelector('#closeButton') as HTMLElement;
    closeButton.click();

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
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);
    const event =
        new FocusEvent('focusout', {relatedTarget: outsideClickTarget});

    // Act.
    uploadDialog.$.dialog.dispatchEvent(event);

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
        uploadDialog.openDialog();
        await waitAfterNextRender(uploadDialog);
        const event = new FocusEvent(
            'focusout', {relatedTarget: uploadDialog.$.closeButton});

        // Act.
        uploadDialog.$.dialog.dispatchEvent(event);

        // Assert.
        assertFalse(uploadDialog.$.dialog.hidden);
        assertEquals(
            0,
            metrics.count(
                'NewTabPage.Lens.UploadDialog.DialogAction',
                LensUploadDialogAction.DIALOG_CLOSED));
      });

  test('clicking esc key closes the dialog', async () => {
    // Arrange.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Act.
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));

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
    windowProxy.setResultFor('onLine', false);

    // Act.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Assert.
    assertTrue(uploadDialog.hasAttribute('is-offline_'));

    // Reset.
    windowProxy.setResultFor('onLine', true);
  });

  test(
      'clicking try again in offline state when online updates UI',
      async () => {
        // Arrange.
        windowProxy.setResultFor('onLine', false);

        // Act.
        uploadDialog.openDialog();
        await waitAfterNextRender(uploadDialog);

        // Assert. (consistency check)
        assertTrue(uploadDialog.hasAttribute('is-offline_'));

        // Arrange.
        windowProxy.setResultFor('onLine', true);

        // Act.
        (uploadDialog.shadowRoot!.querySelector('#offlineRetryButton') as
         HTMLElement)!.click();
        await waitAfterNextRender(uploadDialog);

        // Assert.
        assertFalse(uploadDialog.hasAttribute('is-offline_'));
      });

  test('submit url does not submit with empty url', async () => {
    // Arrange.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Act.
    clickInputSubmit();

    // Assert.
    assertFalse(submitUrlCalled);
  });

  test(
      'submit valid url by clicking submit button should submit ', async () => {
        // Arrange.
        const url = 'http://google.com/image.png';
        uploadDialog.openDialog();
        await waitAfterNextRender(uploadDialog);

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
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

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
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

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
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

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
  //   await waitAfterNextRender(uploadDialog);
  //   // Act.
  //   uploadDialog.$.dragDropArea.dispatchEvent(new DragEvent('dragenter'));
  //   await waitAfterNextRender(uploadDialog);
  //   // Assert.
  //   assertTrue(uploadDialog.hasAttribute('is-dragging_'));
  // });

  test(
      'dragenter then dragleave event should transition to normal state',
      async () => {
        // Arrange.
        uploadDialog.openDialog();
        await waitAfterNextRender(uploadDialog);
        uploadDialog.$.dragDropArea.dispatchEvent(new DragEvent('dragenter'));
        await waitAfterNextRender(uploadDialog);
        // Act.
        uploadDialog.$.dragDropArea.dispatchEvent(new DragEvent('dragleave'));
        await waitAfterNextRender(uploadDialog);
        // Assert.
        assertTrue(uploadDialog.hasAttribute('is-normal-or-error_'));
      });

  test('drop event should submit files', async () => {
    // Arrange.
    let submitFileListCalled = false;
    uploadDialog.$.lensForm.submitFileList = (_fileList: FileList) => {
      submitFileListCalled = true;
    };
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);
    // Act.
    const dataTransfer = new DataTransfer();
    const file = new File([], 'image-file.png', {type: 'image/png'});
    dataTransfer.items.add(file);
    uploadDialog.$.dragDropArea.dispatchEvent(
        new DragEvent('drop', {dataTransfer}));
    await waitAfterNextRender(uploadDialog);
    // Assert.
    assertTrue(submitFileListCalled);
  });

  test('shows error state when FILE_TYPE error is dispatched', async () => {
    // Arrange.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Act.
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('error', {
      detail: LensErrorType.FILE_TYPE,
    }));

    // Assert.
    assertTrue(uploadDialog.hasAttribute('is-error_'));
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
    // Arrange.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Act.
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('error', {
      detail: LensErrorType.FILE_TYPE,
    }));
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('error', {
      detail: LensErrorType.NO_FILE,
    }));

    // Assert.
    assertFalse(uploadDialog.hasAttribute('is-error_'));
  });

  test('shows loading state when file is submitted', async () => {
    // Arrange.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Act.
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('loading', {
      detail: LensSubmitType.FILE,
    }));

    // Assert.
    assertTrue(uploadDialog.hasAttribute('is-loading_'));
    assertEquals(
        1,
        metrics.count(
            'NewTabPage.Lens.UploadDialog.DialogAction',
            LensUploadDialogAction.FILE_SUBMITTED));
  });

  test('shows loading state when URL is submitted', async () => {
    // Arrange.
    uploadDialog.openDialog();
    await waitAfterNextRender(uploadDialog);

    // Act.
    uploadDialog.$.lensForm.dispatchEvent(new CustomEvent('loading', {
      detail: LensSubmitType.URL,
    }));

    // Assert.
    assertTrue(uploadDialog.hasAttribute('is-loading_'));
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
