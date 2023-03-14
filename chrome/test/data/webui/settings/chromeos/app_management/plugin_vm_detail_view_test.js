// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

import {PluginVmBrowserProxyImpl, AppManagementStore, updateSelectedAppId} from 'chrome://os-settings/chromeos/os_settings.js';
import {createBoolPermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {convertOptionalBoolToBool, getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {TestPluginVmBrowserProxy} from './test_plugin_vm_browser_proxy.js';
import {setupFakeHandler, replaceStore, replaceBody, getPermissionCrToggleByType, getPermissionToggleByType} from './test_util.js';
import {AppType, OptionalBool, PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';

suite('<app-management-plugin-vm-detail-view>', function() {
  /** @enum {number} */
  const Permissions = {
    CAMERA: 0,
    MICROPHONE: 1,
  };

  let pluginVmDetailView;
  let fakeHandler;
  /** @type {?TestPluginVmBrowserProxy} */
  let pluginVmBrowserProxy = null;
  let appId;

  function getPermissionBoolByType(permissionType) {
    return getPermissionValueBool(pluginVmDetailView.app_, permissionType);
  }

  function isCrToggleChecked(permissionType) {
    return getPermissionCrToggleByType(pluginVmDetailView, permissionType)
        .checked;
  }

  async function clickToggle(permissionType) {
    getPermissionToggleByType(pluginVmDetailView, permissionType).click();
    // It appears that we need to await twice so that camera/mic toggles can
    // get back from `isRelaunchNeededForNewPermissions()` and commit the
    // change.
    await fakeHandler.flushPipesForTesting();
    await fakeHandler.flushPipesForTesting();
  }

  function getSelectedAppFromStore() {
    const storeData = AppManagementStore.getInstance().data;
    return storeData.apps[storeData.selectedAppId];
  }

  /**
   * @param {boolean} expectedCameraState
   * @param {!HTMLElement} cameraToggle
   * @param {boolean} expectedMicrophoneState
   * @param {!HTMLElement} microphoneToggle
   */
  function assertToggleStates(
      expectedCameraState, cameraToggle, expectedMicrophoneState,
      microphoneToggle) {
    assertEquals(expectedCameraState, cameraToggle.checked);
    assertEquals(expectedMicrophoneState, microphoneToggle.checked);
  }

  /**
   * @param {boolean} expectedCameraState
   * @param {boolean} expectedMicrophoneState
   */
  function assertPermissions(expectedCameraState, expectedMicrophoneState) {
    assertEquals(
        expectedCameraState,
        pluginVmBrowserProxy.permissions[Permissions.CAMERA]);
    assertEquals(
        expectedMicrophoneState,
        pluginVmBrowserProxy.permissions[Permissions.MICROPHONE]);
  }

  async function checkAndAcceptDialog(textId) {
    assertEquals(
        pluginVmDetailView.shadowRoot
            .querySelector('cr-dialog div[slot="body"]')
            .textContent,
        loadTimeData.getString(textId));
    pluginVmDetailView.shadowRoot
        .querySelector('cr-dialog cr-button.action-button')
        .click();
    await fakeHandler.flushPipesForTesting();
  }

  async function checkAndCancelDialog(textId, cancelByEsc) {
    assertEquals(
        pluginVmDetailView.shadowRoot
            .querySelector('cr-dialog div[slot="body"]')
            .textContent,
        loadTimeData.getString(textId));
    if (cancelByEsc) {
      // When <esc> is used to cancel the button, <cr-dialog> will fire a
      // "cancel" event.
      pluginVmDetailView.shadowRoot.querySelector(`cr-dialog`)
          .dispatchEvent(new Event('cancel'));
    } else {
      pluginVmDetailView.shadowRoot
          .querySelector(`cr-dialog cr-button.cancel-button`)
          .click();
    }
    await fakeHandler.flushPipesForTesting();
  }

  /**
   * @param {boolean} booleanToChange
   * @return {OptionalBool}
   */
  function booleanToOptionalBool(booleanToChange) {
    return booleanToChange ? Bool.kTrue : Bool.kFalse;
  }

  setup(async function() {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    PluginVmBrowserProxyImpl.setInstanceForTesting(pluginVmBrowserProxy);
    fakeHandler = setupFakeHandler();
    replaceStore();

    loadTimeData.overrideValues({
      showPluginVmCameraPermissions: true,
      showPluginVmMicrophonePermissions: true,
    });

    const permissions = {};
    const permissionTypes = [
      PermissionType.kPrinting,
      PermissionType.kCamera,
      PermissionType.kMicrophone,
    ];
    for (const permissionType of permissionTypes) {
      permissions[permissionType] =
          createBoolPermission(permissionType, true, false /*is_managed*/);
    }

    pluginVmBrowserProxy.pluginVmRunning = false;

    // Add an app, and make it the currently selected app.
    const options = {
      type: AppType.kPluginVm,
      permissions: permissions,
    };
    const app = await fakeHandler.addApp(null, options);
    appId = app.id;
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(appId));

    pluginVmDetailView =
        document.createElement('app-management-plugin-vm-detail-view');
    replaceBody(pluginVmDetailView);
    await fakeHandler.flushPipesForTesting();
  });

  test('App is rendered correctly', function() {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        pluginVmDetailView.app_.id);
  });

  // The testing browser proxy return false by default in
  // `isRelaunchNeededForNewPermissions()`, so camera and microphone toggles
  // will not trigger the dialog.
  ['kCamera', 'kMicrophone', 'kPrinting'].forEach(
      (permissionType) =>
          test(`Toggle ${permissionType} without dialogs`, async function() {
            assertTrue(getPermissionBoolByType(permissionType));
            assertTrue(isCrToggleChecked(permissionType));

            // Toggle off.
            await clickToggle(permissionType);
            assertFalse(
                !!pluginVmDetailView.shadowRoot.querySelector('cr-dialog'));
            assertFalse(getPermissionBoolByType(permissionType));
            assertFalse(isCrToggleChecked(permissionType));

            // Toggle on.
            await clickToggle(permissionType);
            assertFalse(
                !!pluginVmDetailView.shadowRoot.querySelector('cr-dialog'));
            assertTrue(getPermissionBoolByType(permissionType));
            assertTrue(isCrToggleChecked(permissionType));
          }));

  [['kCamera', 'pluginVmPermissionDialogCameraLabel'],
   ['kMicrophone', 'pluginVmPermissionDialogMicrophoneLabel']]
      .forEach(
          ([permissionType, dialogTextId]) => [true, false].forEach(
              (cancelByEsc) => test(
                  `Toggle ${permissionType} with dialogs (${cancelByEsc})`,
                  async function() {
                    pluginVmBrowserProxy.pluginVmRunning = true;

                    assertTrue(getPermissionBoolByType(permissionType));
                    assertTrue(isCrToggleChecked(permissionType));

                    // Toggle off and cancel the dialog
                    await clickToggle(permissionType);
                    assertTrue(getPermissionBoolByType(permissionType));
                    await checkAndCancelDialog(dialogTextId, cancelByEsc);
                    // No relaunch, and permission should not be changed.
                    assertEquals(
                        pluginVmBrowserProxy.getCallCount('relaunchPluginVm'),
                        0);
                    assertTrue(getPermissionBoolByType(permissionType));
                    assertTrue(isCrToggleChecked(permissionType));

                    // Toggle off again and accept the dialog
                    await clickToggle(permissionType);
                    assertTrue(getPermissionBoolByType(permissionType));
                    await checkAndAcceptDialog(dialogTextId);
                    // Relaunch, and permission should be changed.
                    assertEquals(
                        pluginVmBrowserProxy.getCallCount('relaunchPluginVm'),
                        1);
                    assertFalse(getPermissionBoolByType(permissionType));
                    assertFalse(isCrToggleChecked(permissionType));

                    // Toggle on and cancel the dialog
                    await clickToggle(permissionType);
                    assertFalse(getPermissionBoolByType(permissionType));
                    await checkAndCancelDialog(dialogTextId, cancelByEsc);
                    // No relaunch, and permission should not be changed.
                    assertEquals(
                        pluginVmBrowserProxy.getCallCount('relaunchPluginVm'),
                        1);
                    assertFalse(getPermissionBoolByType(permissionType));
                    assertFalse(isCrToggleChecked(permissionType));

                    // Toggle on again and accept the dialog
                    await clickToggle(permissionType);
                    assertFalse(getPermissionBoolByType(permissionType));
                    await checkAndAcceptDialog(dialogTextId);
                    // Relaunch, and permission should be changed.
                    assertEquals(
                        pluginVmBrowserProxy.getCallCount('relaunchPluginVm'),
                        2);
                    assertTrue(getPermissionBoolByType(permissionType));
                    assertTrue(isCrToggleChecked(permissionType));
                  })));

  test('Pin to shelf toggle', async function() {
    const pinToShelfItem = pluginVmDetailView.$.pinToShelfSetting;
    const toggle = pinToShelfItem.$.toggleRow.$.toggle;

    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertTrue(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(toggle.checked);
    assertEquals(
        toggle.checked,
        convertOptionalBoolToBool(getSelectedAppFromStore().isPinned));
  });
});
