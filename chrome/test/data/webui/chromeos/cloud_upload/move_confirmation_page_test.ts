// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/move_confirmation_page.js';

import {DialogPage, OperationType, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudProvider, MoveConfirmationPageElement} from 'chrome://cloud-upload/move_confirmation_page.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {CloudUploadTestBrowserProxy, ProxyOptions} from './cloud_upload_test_browser_proxy.js';

suite('<move-confirmation-page>', () => {
  /* Holds the <move-confirmation-page> app. */
  let container: HTMLDivElement;
  /* The <move-confirmation-page> app. */
  let moveConfirmationPageApp: MoveConfirmationPageElement;
  /* The BrowserProxy element to make assertions on when mojo methods are
     called. */
  let testProxy: CloudUploadTestBrowserProxy;

  async function setUp(options: ProxyOptions) {
    testProxy = new CloudUploadTestBrowserProxy(options);
    CloudUploadBrowserProxy.setInstance(testProxy);

    // Creates and attaches the <move-confirmation-page> element to the DOM
    // tree.
    moveConfirmationPageApp =
        document.createElement('move-confirmation-page') as
        MoveConfirmationPageElement;
    container.appendChild(moveConfirmationPageApp);

    // Initialise dialog
    switch (options.dialogPage) {
      case DialogPage.kMoveConfirmationOneDrive: {
        await moveConfirmationPageApp.setDialogAttributes(
            1, options.operationType, CloudProvider.ONE_DRIVE);
        break;
      }
      case DialogPage.kMoveConfirmationGoogleDrive: {
        await moveConfirmationPageApp.setDialogAttributes(
            1, options.operationType, CloudProvider.GOOGLE_DRIVE);
        break;
      }
      default:
        assertNotReached();
    }
  }

  /**
   * Runs prior to all the tests running, attaches a div to enable isolated
   * clearing and attaching of the web component.
   */
  suiteSetup(() => {
    container = document.createElement('div');
    document.body.appendChild(container);
  });

  /**
   * Runs after each test. Removes all elements from the <div> that holds
   * the <move-confirmation-page> component.
   */
  teardown(() => {
    assert(window.trustedTypes);
    container.innerHTML = window.trustedTypes.emptyHTML;
    testProxy.handler.reset();
  });

  /**
   * Test that the checkbox does not appear if the move confirmation page has
   * not been shown before for Google Drive when the cloud provider is Google
   * Drive. Test that clicking the action button calls the right mojo request.
   */
  test('No checkbox before first move confirmation for Drive', async () => {
    await setUp({
      fileName: 'text.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
      officeMoveConfirmationShownForDrive: false,
      operationType: OperationType.kMove,
    });
    const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
        '#always-copy-or-move-checkbox');
    assertFalse(!!checkbox);

    moveConfirmationPageApp.$('.action-button').click();

    // Check that the right |respondWithUserActionAndClose| mojo request is
    // called.
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kUploadToGoogleDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));

    // Check that the |setAlwaysMoveOfficeFilesToDrive| mojo request is
    // called with |false|.
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('setAlwaysMoveOfficeFilesToDrive'));
    assertDeepEquals(
        [false], testProxy.handler.getArgs('setAlwaysMoveOfficeFilesToDrive'));
  });

  /**
   * Test that the checkbox does appear if the move confirmation page has been
   * shown before for Google Drive when the cloud provider is Google Drive.
   * Test that clicking the checkbox and the action button calls the right mojo
   * requests.
   */
  test(
      'Checkbox after first move confirmation for Drive. Checkbox clicked',
      async () => {
        await setUp({
          fileName: 'text.docx',
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
          alwaysMoveOfficeFilesToDrive: false,
          alwaysMoveOfficeFilesToOneDrive: true,
          officeMoveConfirmationShownForDrive: true,
          operationType: OperationType.kMove,
        });
        const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
            '#always-copy-or-move-checkbox');
        assertTrue(!!checkbox);

        // Click checkbox.
        assertFalse(checkbox.checked);
        checkbox.checked = true;

        moveConfirmationPageApp.$('.action-button').click();

        // Check that the right |respondWithUserActionAndClose| mojo request is
        // called.
        await testProxy.handler.whenCalled('respondWithUserActionAndClose');
        assertEquals(
            1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
        assertDeepEquals(
            [UserAction.kUploadToGoogleDrive],
            testProxy.handler.getArgs('respondWithUserActionAndClose'));

        // Check that the right |setAlwaysMoveOfficeFilesToDrive| mojo request
        // is called with |true|.
        assertEquals(
            1,
            testProxy.handler.getCallCount('setAlwaysMoveOfficeFilesToDrive'));
        assertDeepEquals(
            [true],
            testProxy.handler.getArgs('setAlwaysMoveOfficeFilesToDrive'));

        // Check that the |setAlwaysMoveOfficeFilesToOneDrive| mojo request is
        // not called.
        assertEquals(
            0,
            testProxy.handler.getCallCount(
                'setAlwaysMoveOfficeFilesToOneDrive'));
      });

  /**
   * Test that the checkbox does appear if the move confirmation page has been
   * shown before for Google Drive when the cloud provider is Google Drive.
   * Test that clicking the action button calls the right mojo request.
   */
  test(
      'Checkbox after first move confirmation for Drive. Checkbox not clicked',
      async () => {
        await setUp({
          fileName: 'text.docx',
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
          alwaysMoveOfficeFilesToDrive: false,
          alwaysMoveOfficeFilesToOneDrive: true,
          officeMoveConfirmationShownForDrive: true,
          operationType: OperationType.kMove,
        });
        const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
            '#always-copy-or-move-checkbox');
        assertTrue(!!checkbox);

        // Don't click checkbox.
        assertFalse(checkbox.checked);

        moveConfirmationPageApp.$('.action-button').click();

        // Check that the right |respondWithUserActionAndClose| mojo request is
        // called.
        await testProxy.handler.whenCalled('respondWithUserActionAndClose');
        assertEquals(
            1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
        assertDeepEquals(
            [UserAction.kUploadToGoogleDrive],
            testProxy.handler.getArgs('respondWithUserActionAndClose'));

        // Check that the |setAlwaysMoveOfficeFilesToDrive| mojo request is
        // called with |false|.
        await testProxy.handler.whenCalled('respondWithUserActionAndClose');
        assertEquals(
            1,
            testProxy.handler.getCallCount('setAlwaysMoveOfficeFilesToDrive'));
        assertDeepEquals(
            [false],
            testProxy.handler.getArgs('setAlwaysMoveOfficeFilesToDrive'));
      });

  /**
   * Test that the checkbox doesn't appear if the move confirmation page has
   * been shown before for OneDrive but the cloud provider is Google Drive.
   */
  test(
      'No checkbox before first move confirmation for Drive but after move ' +
          'confirmation for OneDrive has already been shown',
      async () => {
        await setUp({
          fileName: 'text.docx',
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
          officeMoveConfirmationShownForDrive: false,
          officeMoveConfirmationShownForOneDrive: true,
          operationType: OperationType.kMove,
        });
        const hasCheckbox = moveConfirmationPageApp.$<CrCheckboxElement>(
                                '#always-copy-or-move-checkbox') !== null;
        assertFalse(hasCheckbox);
      });

  /**
   * Test that the checkbox does not appear if the move confirmation page has
   * not been shown before for OneDrive when the cloud provider is OneDrive.
   * Test that clicking the action button calls the right mojo request.
   */
  test('No checkbox before first move confirmation for OneDrive', async () => {
    await setUp({
      fileName: 'text.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationOneDrive,
      officeMoveConfirmationShownForOneDrive: false,
      operationType: OperationType.kMove,
    });
    const hasCheckbox = moveConfirmationPageApp.$<CrCheckboxElement>(
                            '#always-copy-or-move-checkbox') !== null;
    assertFalse(hasCheckbox);

    moveConfirmationPageApp.$('.action-button').click();

    // Check that the right |respondWithUserActionAndClose| mojo request is
    // called.
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kUploadToOneDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));

    // Check that the |setAlwaysMoveOfficeFilesToOneDrive| mojo request is
    // called with |false|.
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1,
        testProxy.handler.getCallCount('setAlwaysMoveOfficeFilesToOneDrive'));
    assertDeepEquals(
        [false],
        testProxy.handler.getArgs('setAlwaysMoveOfficeFilesToOneDrive'));
  });

  /**
   * Test that the checkbox does not appear if the move confirmation page has
   * not been shown before for OneDrive when the cloud provider is OneDrive.
   * Test that clicking the checkbox and the action button calls the right mojo
   * requests.
   */
  test(
      'Checkbox after first move confirmation for OneDrive. Checkbox clicked',
      async () => {
        await setUp({
          fileName: 'text.docx',
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationOneDrive,
          alwaysMoveOfficeFilesToDrive: true,
          alwaysMoveOfficeFilesToOneDrive: false,
          officeMoveConfirmationShownForOneDrive: true,
          operationType: OperationType.kMove,
        });
        const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
            '#always-copy-or-move-checkbox');
        assertTrue(!!checkbox);

        // Click checkbox.
        assertFalse(checkbox.checked);
        checkbox.checked = true;

        moveConfirmationPageApp.$('.action-button').click();

        // Check that the right |respondWithUserActionAndClose| mojo request is
        // called.
        await testProxy.handler.whenCalled('respondWithUserActionAndClose');
        assertEquals(
            1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
        assertDeepEquals(
            [UserAction.kUploadToOneDrive],
            testProxy.handler.getArgs('respondWithUserActionAndClose'));

        // Check that the right |setAlwaysMoveOfficeFilesToOneDrive| mojo
        // request is called.
        assertEquals(
            1,
            testProxy.handler.getCallCount(
                'setAlwaysMoveOfficeFilesToOneDrive'));
        assertDeepEquals(
            [true],
            testProxy.handler.getArgs('setAlwaysMoveOfficeFilesToOneDrive'));


        // Check that the |setAlwaysMoveOfficeFilesToDrive| mojo request is not
        // called.
        assertEquals(
            0,
            testProxy.handler.getCallCount('setAlwaysMoveOfficeFilesToDrive'));
      });


  /**
   * Test that the checkbox does appear if the move confirmation page has been
   * shown before for OneDrive when the cloud provider is OneDrive.
   * Test that clicking the action button calls the right mojo request.
   */
  test(
      'Checkbox after first move confirmation for OneDrive. Checkbox not ' +
          'clicked',
      async () => {
        await setUp({
          fileName: 'text.docx',
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationOneDrive,
          alwaysMoveOfficeFilesToDrive: true,
          alwaysMoveOfficeFilesToOneDrive: false,
          officeMoveConfirmationShownForOneDrive: true,
          operationType: OperationType.kMove,
        });
        const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
            '#always-copy-or-move-checkbox');
        assertTrue(!!checkbox);

        // Don't click checkbox.
        assertFalse(checkbox.checked);

        moveConfirmationPageApp.$('.action-button').click();

        // Check that the right |respondWithUserActionAndClose| mojo request is
        // called.
        await testProxy.handler.whenCalled('respondWithUserActionAndClose');
        assertEquals(
            1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
        assertDeepEquals(
            [UserAction.kUploadToOneDrive],
            testProxy.handler.getArgs('respondWithUserActionAndClose'));

        // Check that the |setAlwaysMoveOfficeFilesToOneDrive| mojo request is
        // called with |false|.
        await testProxy.handler.whenCalled('respondWithUserActionAndClose');
        assertEquals(
            1,
            testProxy.handler.getCallCount(
                'setAlwaysMoveOfficeFilesToOneDrive'));
        assertDeepEquals(
            [false],
            testProxy.handler.getArgs('setAlwaysMoveOfficeFilesToOneDrive'));
      });

  /**
   * Test that the checkbox doesn't appear if the move confirmation page has
   * been shown before for Google Drive but the cloud provider is OneDrive.
   */
  test(
      'No checkbox before first move confirmation for OneDrive but after ' +
          'move confirmation for Drive has already been shown',
      async () => {
        await setUp({
          fileName: 'text.docx',
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationOneDrive,
          officeMoveConfirmationShownForOneDrive: false,
          operationType: OperationType.kMove,
        });
        const hasCheckbox = moveConfirmationPageApp.$<CrCheckboxElement>(
                                '#always-copy-or-move-checkbox') !== null;
        assertFalse(hasCheckbox);
      });


  /**
   * Test that the checkbox is pre-checked if the "Always move to Drive"
   * preference is set to true.
   */
  test('Checkbox pre-checked for Drive', async () => {
    await setUp({
      fileName: 'text.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
      alwaysMoveOfficeFilesToDrive: true,
      alwaysMoveOfficeFilesToOneDrive: false,
      officeMoveConfirmationShownForDrive: true,
      operationType: OperationType.kMove,
    });
    const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
        '#always-copy-or-move-checkbox');
    assertTrue(!!checkbox);
    assertTrue(checkbox.checked);
  });

  /**
   * Test that the checkbox is pre-checked if the "Always move to OneDrive"
   * preference is set to true.
   */
  test('Checkbox pre-checked for OneDrive', async () => {
    await setUp({
      fileName: 'text.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationOneDrive,
      alwaysMoveOfficeFilesToDrive: false,
      alwaysMoveOfficeFilesToOneDrive: true,
      officeMoveConfirmationShownForOneDrive: true,
      operationType: OperationType.kMove,
    });
    const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
        '#always-copy-or-move-checkbox');
    assertTrue(!!checkbox);
    assertTrue(checkbox.checked);
  });

  /**
   * Check the dialog's text when the cloud provider is Drive.
   */
  test('DialogTextForDrive', async () => {
    await setUp({
      fileName: 'text.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
      officeMoveConfirmationShownForDrive: true,
      operationType: OperationType.kMove,
    });
    // Title.
    const titleElement = moveConfirmationPageApp.$<HTMLElement>('#title')!;
    assertTrue(titleElement.innerText.includes('Google Drive'));

    // Body.
    const bodyText = moveConfirmationPageApp.$('#body-text');
    assertTrue(bodyText.innerText.includes('Google Drive'));

    // Checkbox.
    const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
        '#always-copy-or-move-checkbox');
    assertTrue(!!checkbox);
    assertTrue(checkbox.innerText.includes('Don\'t ask again'));
  });

  /**
   * Check the dialog's text when the cloud provider is OneDrive.
   */
  test('DialogTextForOneDrive', async () => {
    await setUp({
      fileName: 'text.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationOneDrive,
      officeMoveConfirmationShownForOneDrive: true,
      operationType: OperationType.kMove,
    });
    // Title.
    const titleElement = moveConfirmationPageApp.$<HTMLElement>('#title')!;
    assertTrue(titleElement.innerText.includes('Microsoft OneDrive'));

    // Body.
    const bodyText = moveConfirmationPageApp.$('#body-text');
    assertTrue(bodyText.innerText.includes('OneDrive'));

    // Checkbox.
    const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
        '#always-copy-or-move-checkbox');
    assertTrue(!!checkbox);
    assertTrue(checkbox.innerText.includes('Don\'t ask again'));
  });

  /**
   * Check the dialog's text when the operation type is 'Move'.
   */
  test('DialogTextForMoveAndUpload', async () => {
    await setUp({
      fileName: 'text.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
      officeMoveConfirmationShownForDrive: true,
      operationType: OperationType.kMove,
    });
    // Title.
    const titleElement = moveConfirmationPageApp.$<HTMLElement>('#title')!;
    assertTrue(titleElement.innerText.includes('Move'));

    // Button.
    const actionButton =
        moveConfirmationPageApp.$<HTMLElement>('.action-button')!;
    assertEquals('Move and open', actionButton.innerText);
  });

  /**
   * Check the dialog's text when the operation type is 'Copy'.
   */
  test('DialogTextForCopyAndUpload', async () => {
    await setUp({
      fileName: 'text.docx',
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
      officeMoveConfirmationShownForDrive: true,
      operationType: OperationType.kCopy,
    });
    // Title.
    const titleElement = moveConfirmationPageApp.$<HTMLElement>('#title')!;
    assertTrue(titleElement.innerText.includes('Copy'));

    // Button.
    const actionButton =
        moveConfirmationPageApp.$<HTMLElement>('.action-button')!;
    assertEquals('Copy and open', actionButton.innerText);
  });
});
