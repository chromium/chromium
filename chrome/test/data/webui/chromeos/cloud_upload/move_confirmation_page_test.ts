// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://cloud-upload/move_confirmation_page.js';

import {DialogPage, UserAction} from 'chrome://cloud-upload/cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from 'chrome://cloud-upload/cloud_upload_browser_proxy.js';
import {CloudProvider, MoveConfirmationPageElement} from 'chrome://cloud-upload/move_confirmation_page.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

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
    container.innerHTML = '';
    testProxy.handler.reset();
  });

  /**
   * Test that the checkbox does not appear if the move confirmation page has
   * not been shown before for Google Drive when the cloud provider is Google
   * Drive. Test that clicking the action button calls the right mojo request.
   */
  test('No checkbox before first move confirmation for Drive', async () => {
    await setUp({
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
      officeMoveConfirmationShownForDrive: false,
    });
    await moveConfirmationPageApp.setCloudProvider(CloudProvider.GOOGLE_DRIVE);
    const checkbox =
        moveConfirmationPageApp.$<CrCheckboxElement>('#always-move-checkbox');
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

    // Check that the |setOfficeMoveConfirmationShownForDriveTrue| mojo request
    // is called.
    assertEquals(
        1,
        testProxy.handler.getCallCount(
            'setOfficeMoveConfirmationShownForDriveTrue'));
    assertEquals(
        0,
        testProxy.handler.getCallCount(
            'setOfficeMoveConfirmationShownForOneDriveTrue'));
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
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
          officeMoveConfirmationShownForDrive: true,
        });
        await moveConfirmationPageApp.setCloudProvider(
            CloudProvider.GOOGLE_DRIVE);
        const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
            '#always-move-checkbox');
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
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
          officeMoveConfirmationShownForDrive: true,
        });
        await moveConfirmationPageApp.setCloudProvider(
            CloudProvider.GOOGLE_DRIVE);
        const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
            '#always-move-checkbox');
        assertTrue(checkbox !== null);

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
      'No checkbox before first move confirmation for Drive but after first ' +
          'move confirmation for OneDrive',
      async () => {
        await setUp({
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationGoogleDrive,
          officeMoveConfirmationShownForDrive: false,
          officeMoveConfirmationShownForOneDrive: true,
        });
        await moveConfirmationPageApp.setCloudProvider(
            CloudProvider.GOOGLE_DRIVE);
        const hasCheckbox = moveConfirmationPageApp.$<CrCheckboxElement>(
                                '#always-move-checkbox') !== null;
        assertFalse(hasCheckbox);
      });

  /**
   * Test that the checkbox does not appear if the move confirmation page has
   * not been shown before for OneDrive when the cloud provider is OneDrive.
   * Test that clicking the action button calls the right mojo request.
   */
  test('No checkbox before first move confirmation for OneDrive', async () => {
    await setUp({
      officeWebAppInstalled: true,
      installOfficeWebAppResult: true,
      odfsMounted: true,
      dialogPage: DialogPage.kMoveConfirmationOneDrive,
      officeMoveConfirmationShownForOneDrive: false,
    });
    await moveConfirmationPageApp.setCloudProvider(CloudProvider.ONE_DRIVE);
    const hasCheckbox = moveConfirmationPageApp.$<CrCheckboxElement>(
                            '#always-move-checkbox') !== null;
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

    // Check that the |setOfficeMoveConfirmationShownForOneDriveTrue| mojo
    // request is called.
    assertEquals(
        0,
        testProxy.handler.getCallCount(
            'setOfficeMoveConfirmationShownForDriveTrue'));
    assertEquals(
        1,
        testProxy.handler.getCallCount(
            'setOfficeMoveConfirmationShownForOneDriveTrue'));
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
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationOneDrive,
          officeMoveConfirmationShownForOneDrive: true,
        });
        await moveConfirmationPageApp.setCloudProvider(CloudProvider.ONE_DRIVE);
        const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
            '#always-move-checkbox');
        assertTrue(checkbox !== null);

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
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationOneDrive,
          officeMoveConfirmationShownForOneDrive: true,
        });
        await moveConfirmationPageApp.setCloudProvider(CloudProvider.ONE_DRIVE);
        const checkbox = moveConfirmationPageApp.$<CrCheckboxElement>(
            '#always-move-checkbox');
        assertTrue(checkbox !== null);

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
          'first move confirmation for Drive',
      async () => {
        await setUp({
          officeWebAppInstalled: true,
          installOfficeWebAppResult: true,
          odfsMounted: true,
          dialogPage: DialogPage.kMoveConfirmationOneDrive,
          officeMoveConfirmationShownForOneDrive: false,
          officeMoveConfirmationShownForDrive: true,
        });
        await moveConfirmationPageApp.setCloudProvider(CloudProvider.ONE_DRIVE);
        const hasCheckbox = moveConfirmationPageApp.$<CrCheckboxElement>(
                                '#always-move-checkbox') !== null;
        assertFalse(hasCheckbox);
      });
});
