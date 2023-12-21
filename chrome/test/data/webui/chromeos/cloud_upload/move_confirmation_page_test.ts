// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/move_confirmation_page.js';

import {OperationType, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudProvider, MoveConfirmationPageElement} from 'chrome://cloud-upload/move_confirmation_page.js';
import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrosLottieEvent} from 'chrome://resources/cros_components/lottie_renderer/lottie-renderer.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
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

    // Setup fake strings, there are tests below to assert these strings.
    loadTimeData.resetForTesting({
      'moveAndOpen': 'Move and open',
      'copyAndOpen': 'Copy and open',
      'moveConfirmationMoveTitle': 'Move 1 file to $1',
      'moveConfirmationMoveTitlePlural': 'Move $2 files to $1',
      'moveConfirmationCopyTitle': 'Copy 1 file to $1',
      'moveConfirmationCopyTitlePlural': 'Copy $2 files to $1',
      'moveConfirmationOneDriveBodyText': 'OneDrive body',
      'moveConfirmationGoogleDriveBodyText': 'Google Drive body',
      'moveConfirmationAlwaysMove': 'Don\'t ask again',
      'oneDrive': 'Microsoft OneDrive',
      'googleDrive': 'Google Drive',
    });

    // Define promise to wait for a `CrosLottieEvent.INITIALIZED` event.
    let resolveFunction: () => void;
    const animationInitializedPromise = new Promise<void>((resolve) => {
      resolveFunction = resolve;
    });
    document.addEventListener(CrosLottieEvent.INITIALIZED, () => {
      resolveFunction();
    });

    // Creates and attaches the <move-confirmation-page> element to the DOM
    // tree.
    moveConfirmationPageApp =
        document.createElement('move-confirmation-page') as
        MoveConfirmationPageElement;
    container.appendChild(moveConfirmationPageApp);

    // Initialise dialog
    if (options.dialogSpecificArgs.moveConfirmationOneDriveDialogArgs) {
      await moveConfirmationPageApp.setDialogAttributes(
          1,
          options.dialogSpecificArgs.moveConfirmationOneDriveDialogArgs
              .operationType,
          CloudProvider.ONE_DRIVE);
    } else if (options.dialogSpecificArgs
                   .moveConfirmationGoogleDriveDialogArgs) {
      await moveConfirmationPageApp.setDialogAttributes(
          1,
          options.dialogSpecificArgs.moveConfirmationGoogleDriveDialogArgs
              .operationType,
          CloudProvider.GOOGLE_DRIVE);
    } else {
      assertNotReached();
    }

    // Ensure that the animation within the move confirmation page has been
    // initialized to avoid race conditions when the test exits.
    await animationInitializedPromise;
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
    moveConfirmationPageApp.$('.action-button').click();
    loadTimeData.resetForTesting();
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
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationGoogleDriveDialogArgs: {
          operationType: OperationType.kMove,
        },
      },
      officeMoveConfirmationShownForDrive: false,
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
          fileNames: ['text.docx'],
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogSpecificArgs: {
            moveConfirmationGoogleDriveDialogArgs: {
              operationType: OperationType.kMove,
            },
          },
          alwaysMoveOfficeFilesToDrive: false,
          alwaysMoveOfficeFilesToOneDrive: true,
          officeMoveConfirmationShownForDrive: true,
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
          fileNames: ['text.docx'],
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogSpecificArgs: {
            moveConfirmationGoogleDriveDialogArgs: {
              operationType: OperationType.kMove,
            },
          },
          alwaysMoveOfficeFilesToDrive: false,
          alwaysMoveOfficeFilesToOneDrive: true,
          officeMoveConfirmationShownForDrive: true,
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
          fileNames: ['text.docx'],
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogSpecificArgs: {
            moveConfirmationGoogleDriveDialogArgs: {
              operationType: OperationType.kMove,
            },
          },
          officeMoveConfirmationShownForDrive: false,
          officeMoveConfirmationShownForOneDrive: true,
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
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationOneDriveDialogArgs: {
          operationType: OperationType.kMove,
        },
      },
      officeMoveConfirmationShownForOneDrive: false,
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
          fileNames: ['text.docx'],
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogSpecificArgs: {
            moveConfirmationOneDriveDialogArgs: {
              operationType: OperationType.kMove,
            },
          },
          alwaysMoveOfficeFilesToDrive: true,
          alwaysMoveOfficeFilesToOneDrive: false,
          officeMoveConfirmationShownForOneDrive: true,
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
          fileNames: ['text.docx'],
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogSpecificArgs: {
            moveConfirmationOneDriveDialogArgs: {
              operationType: OperationType.kMove,
            },
          },
          alwaysMoveOfficeFilesToDrive: true,
          alwaysMoveOfficeFilesToOneDrive: false,
          officeMoveConfirmationShownForOneDrive: true,
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
          fileNames: ['text.docx'],
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogSpecificArgs: {
            moveConfirmationOneDriveDialogArgs: {
              operationType: OperationType.kMove,
            },
          },
          officeMoveConfirmationShownForOneDrive: false,
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
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationGoogleDriveDialogArgs: {
          operationType: OperationType.kMove,
        },
      },
      alwaysMoveOfficeFilesToDrive: true,
      alwaysMoveOfficeFilesToOneDrive: false,
      officeMoveConfirmationShownForDrive: true,
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
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationOneDriveDialogArgs: {
          operationType: OperationType.kMove,
        },
      },
      alwaysMoveOfficeFilesToDrive: false,
      alwaysMoveOfficeFilesToOneDrive: true,
      officeMoveConfirmationShownForOneDrive: true,
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
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationGoogleDriveDialogArgs: {
          operationType: OperationType.kMove,
        },
      },
      officeMoveConfirmationShownForDrive: true,
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
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationOneDriveDialogArgs: {
          operationType: OperationType.kMove,
        },
      },
      officeMoveConfirmationShownForOneDrive: true,
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
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationGoogleDriveDialogArgs: {
          operationType: OperationType.kMove,
        },
      },
      officeMoveConfirmationShownForDrive: true,
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
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationGoogleDriveDialogArgs: {
          operationType: OperationType.kCopy,
        },
      },
      officeMoveConfirmationShownForDrive: true,
    });
    // Title.
    const titleElement = moveConfirmationPageApp.$<HTMLElement>('#title')!;
    assertTrue(titleElement.innerText.includes('Copy'));

    // Button.
    const actionButton =
        moveConfirmationPageApp.$<HTMLElement>('.action-button')!;
    assertEquals('Copy and open', actionButton.innerText);
  });

  /**
   * Test that clicking the cancel button triggers the right
   * `respondWithUserActionAndClose` mojo request.
   */
  test('Cancel', async () => {
    await setUp({
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationGoogleDriveDialogArgs: {
          operationType: OperationType.kCopy,
        },
      },
      officeMoveConfirmationShownForDrive: true,
    });

    moveConfirmationPageApp.$('.cancel-button').click();
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kCancelGoogleDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });

  /**
   * Test that an Escape keydown triggers the right
   * `respondWithUserActionAndClose` mojo request.
   */
  test('Escape', async () => {
    await setUp({
      fileNames: ['text.docx'],
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogSpecificArgs: {
        moveConfirmationGoogleDriveDialogArgs: {
          operationType: OperationType.kCopy,
        },
      },
      officeMoveConfirmationShownForDrive: true,
    });

    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'Escape'}));
    await testProxy.handler.whenCalled('respondWithUserActionAndClose');
    assertEquals(
        1, testProxy.handler.getCallCount('respondWithUserActionAndClose'));
    assertDeepEquals(
        [UserAction.kCancelGoogleDrive],
        testProxy.handler.getArgs('respondWithUserActionAndClose'));
  });
});
