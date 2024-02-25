// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementPinToShelfItemElement, AppManagementPluginVmDetailViewElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, AppManagementToggleRowElement, CrButtonElement, CrToggleElement, PluginVmBrowserProxyImpl, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {App, AppType, Permission, PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createBoolPermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {getPermissionCrToggleByType, getPermissionToggleByType, replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

import {TestPluginVmBrowserProxy} from './test_plugin_vm_browser_proxy.js';

// TODO(b/270728282) - remove "as" cast once getPermissionCrToggleByType()
// becomes a TS function.
suite('<app-management-plugin-vm-detail-view>', () => {
  let pluginVmDetailView: AppManagementPluginVmDetailViewElement;
  let fakeHandler: FakePageHandler;
  let pluginVmBrowserProxy: TestPluginVmBrowserProxy;
  let appId: string;

  function getPermissionBoolByType(permissionType: PermissionTypeIndex):
      boolean {
    return getPermissionValueBool(
        pluginVmDetailView.get('app_'), permissionType);
  }

  function isCrToggleChecked(permissionType: PermissionTypeIndex): boolean {
    return (getPermissionCrToggleByType(pluginVmDetailView, permissionType) as
            CrToggleElement)
        .checked;
  }

  async function clickToggle(permissionType: PermissionTypeIndex):
      Promise<void> {
    const toggleRow =
        getPermissionToggleByType(pluginVmDetailView, permissionType) as
        AppManagementToggleRowElement;
    toggleRow.click();
    // It appears that we need to await twice so that camera/mic toggles can
    // get back from `isRelaunchNeededForNewPermissions()` and commit the
    // change.
    await fakeHandler.flushPipesForTesting();
    await fakeHandler.flushPipesForTesting();
  }

  function getSelectedAppFromStore(): App {
    const storeData = AppManagementStore.getInstance().data;
    assertTrue(!!storeData);
    const selectedAppId = storeData.selectedAppId;
    assertTrue(!!selectedAppId);
    const selectedApp = storeData.apps[selectedAppId];
    assertTrue(!!selectedApp);
    return selectedApp;
  }

  async function checkAndAcceptDialog(textId: string): Promise<void> {
    const dialogBody = pluginVmDetailView.shadowRoot!.querySelector(
        'cr-dialog div[slot="body"]');
    assertTrue(!!dialogBody);
    assertEquals(dialogBody.textContent, loadTimeData.getString(textId));
    const button =
        pluginVmDetailView.shadowRoot!.querySelector<CrButtonElement>(
            'cr-dialog cr-button.action-button');
    assertTrue(!!button);
    button.click();
    await fakeHandler.flushPipesForTesting();
  }

  async function checkAndCancelDialog(
      textId: string, cancelByEsc: boolean): Promise<void> {
    const dialogBody = pluginVmDetailView.shadowRoot!.querySelector(
        'cr-dialog div[slot="body"]');
    assertTrue(!!dialogBody);
    assertEquals(dialogBody.textContent, loadTimeData.getString(textId));
    if (cancelByEsc) {
      // When <esc> is used to cancel the button, <cr-dialog> will fire a
      // "cancel" event.
      const dialog = pluginVmDetailView.shadowRoot!.querySelector(`cr-dialog`);
      assertTrue(!!dialog);
      dialog.dispatchEvent(new Event('cancel'));
    } else {
      const button =
          pluginVmDetailView.shadowRoot!.querySelector<CrButtonElement>(
              'cr-dialog cr-button.cancel-button');
      assertTrue(!!button);
      button.click();
    }
    await fakeHandler.flushPipesForTesting();
  }

  suiteSetup(() => {
    pluginVmBrowserProxy = new TestPluginVmBrowserProxy();
    PluginVmBrowserProxyImpl.setInstanceForTesting(pluginVmBrowserProxy);
  });

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    loadTimeData.overrideValues({
      showPluginVmCameraPermissions: true,
      showPluginVmMicrophonePermissions: true,
    });

    const permissions: Partial<Record<PermissionType, Permission>> = {};
    const permissionTypes = [
      PermissionType.kPrinting,
      PermissionType.kCamera,
      PermissionType.kMicrophone,
    ];
    for (const permissionType of permissionTypes) {
      permissions[permissionType] =
          createBoolPermission(permissionType, true, false /*is_managed*/);
    }

    pluginVmBrowserProxy.setPluginVmRunning(false);

    // Add an app, and make it the currently selected app.
    const options = {
      type: AppType.kPluginVm,
      permissions: permissions,
    };
    const app = await fakeHandler.addApp('', options);
    appId = app.id;
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(appId));

    pluginVmDetailView =
        document.createElement('app-management-plugin-vm-detail-view');
    replaceBody(pluginVmDetailView);
    await fakeHandler.flushPipesForTesting();
  });

  teardown(() => {
    pluginVmDetailView.remove();
    pluginVmBrowserProxy.reset();
  });

  test('App is rendered correctly', () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        pluginVmDetailView.get('app_').id);
  });

  // The testing browser proxy return false by default in
  // `isRelaunchNeededForNewPermissions()`, so camera and microphone toggles
  // will not trigger the dialog.
  ['kCamera', 'kMicrophone', 'kPrinting'].forEach(
      (type) => test(`Toggle ${type} without dialogs`, async () => {
        const permissionType: PermissionTypeIndex = type as PermissionTypeIndex;
        assertTrue(getPermissionBoolByType(permissionType));
        assertTrue(isCrToggleChecked(permissionType));

        // Toggle off.
        await clickToggle(permissionType);
        assertNull(pluginVmDetailView.shadowRoot!.querySelector('cr-dialog'));
        assertFalse(getPermissionBoolByType(permissionType));
        assertFalse(isCrToggleChecked(permissionType));

        // Toggle on.
        await clickToggle(permissionType);
        assertNull(pluginVmDetailView.shadowRoot!.querySelector('cr-dialog'));
        assertTrue(getPermissionBoolByType(permissionType));
        assertTrue(isCrToggleChecked(permissionType));
      }));

  [{type: 'kCamera', dialogTextId: 'pluginVmPermissionDialogCameraLabel'}, {
    type: 'kMicrophone',
    dialogTextId: 'pluginVmPermissionDialogMicrophoneLabel',
  }]
      .forEach(
          ({type, dialogTextId}) => [true, false].forEach(
              (cancelByEsc) => test(
                  `Toggle ${type} with dialogs (${cancelByEsc})`, async () => {
                    const permissionType = type as PermissionTypeIndex;
                    pluginVmBrowserProxy.setPluginVmRunning(true);

                    assertTrue(getPermissionBoolByType(permissionType));
                    assertTrue(isCrToggleChecked(permissionType));

                    // Toggle off and cancel the dialog
                    await clickToggle(permissionType);
                    assertTrue(getPermissionBoolByType(permissionType));
                    await checkAndCancelDialog(dialogTextId, cancelByEsc);
                    // No relaunch, and permission should not be changed.
                    assertEquals(
                        0,
                        pluginVmBrowserProxy.getCallCount('relaunchPluginVm'));
                    assertTrue(getPermissionBoolByType(permissionType));
                    assertTrue(isCrToggleChecked(permissionType));

                    // Toggle off again and accept the dialog
                    await clickToggle(permissionType);
                    assertTrue(getPermissionBoolByType(permissionType));
                    await checkAndAcceptDialog(dialogTextId);
                    // Relaunch, and permission should be changed.
                    assertEquals(
                        1,
                        pluginVmBrowserProxy.getCallCount('relaunchPluginVm'));
                    assertFalse(getPermissionBoolByType(permissionType));
                    assertFalse(isCrToggleChecked(permissionType));

                    // Toggle on and cancel the dialog
                    await clickToggle(permissionType);
                    assertFalse(getPermissionBoolByType(permissionType));
                    await checkAndCancelDialog(dialogTextId, cancelByEsc);
                    // No relaunch, and permission should not be changed.
                    assertEquals(
                        1,
                        pluginVmBrowserProxy.getCallCount('relaunchPluginVm'));
                    assertFalse(getPermissionBoolByType(permissionType));
                    assertFalse(isCrToggleChecked(permissionType));

                    // Toggle on again and accept the dialog
                    await clickToggle(permissionType);
                    assertFalse(getPermissionBoolByType(permissionType));
                    await checkAndAcceptDialog(dialogTextId);
                    // Relaunch, and permission should be changed.
                    assertEquals(
                        2,
                        pluginVmBrowserProxy.getCallCount('relaunchPluginVm'));
                    assertTrue(getPermissionBoolByType(permissionType));
                    assertTrue(isCrToggleChecked(permissionType));
                  })));

  test('Pin to shelf toggle', async () => {
    const pinToShelfItem =
        pluginVmDetailView.shadowRoot!
            .querySelector<AppManagementPinToShelfItemElement>(
                '#pinToShelfSetting');
    assertTrue(!!pinToShelfItem);
    const toggleRow =
        pinToShelfItem.shadowRoot!.querySelector<AppManagementToggleRowElement>(
            '#toggleRow');
    assertTrue(!!toggleRow);
    const toggle = toggleRow.$.toggle;

    assertFalse(toggle.checked);
    assertEquals(toggle.checked, getSelectedAppFromStore().isPinned);
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertTrue(toggle.checked);
    assertEquals(toggle.checked, getSelectedAppFromStore().isPinned);
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(toggle.checked);
    assertEquals(toggle.checked, getSelectedAppFromStore().isPinned);
  });
});
