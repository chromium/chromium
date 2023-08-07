// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementPinToShelfItemElement, AppManagementPwaDetailViewElement, AppManagementSubAppsItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, updateSelectedAppId, updateSubAppToParentAppId} from 'chrome://os-settings/os_settings.js';
import {App, InstallReason} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {AppManagementToggleRowElement} from 'chrome://resources/cr_components/app_management/toggle_row.js';
import {convertOptionalBoolToBool, getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {getPermissionCrToggleByType, getPermissionToggleByType, replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-pwa-detail-view>', () => {
  let pwaDetailView: AppManagementPwaDetailViewElement;
  let fakeHandler: FakePageHandler;
  let defaultAppId: string;

  function getPermissionBoolByType(permissionType: PermissionTypeIndex):
      boolean {
    return getPermissionValueBool(pwaDetailView.get('app_'), permissionType);
  }

  async function clickToggle(permissionType: PermissionTypeIndex):
      Promise<void> {
    (getPermissionToggleByType(pwaDetailView, permissionType) as
     AppManagementToggleRowElement)
        .click();
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

  setup(async () => {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Add an app, and make it the currently selected app.
    const app = await fakeHandler.addApp();
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));
    defaultAppId = app.id;

    pwaDetailView = document.createElement('app-management-pwa-detail-view');
    replaceBody(pwaDetailView);
  });

  teardown(() => {
    pwaDetailView.remove();
  });

  test('App is rendered correctly', () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        pwaDetailView.get('app_').id);
  });

  // TODO(b/270728282) - remove "as" cast once getPermissionCrToggleByType()
  // becomes a TS function.
  test('toggle permissions', async () => {
    const checkToggle = async (permissionType: PermissionTypeIndex) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue((getPermissionCrToggleByType(pwaDetailView, permissionType) as
                  CrToggleElement)
                     .checked);

      // Toggle off.
      await clickToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      assertFalse((getPermissionCrToggleByType(pwaDetailView, permissionType) as
                   CrToggleElement)
                      .checked);

      // Toggle on.
      await clickToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue((getPermissionCrToggleByType(pwaDetailView, permissionType) as
                  CrToggleElement)
                     .checked);
    };

    await checkToggle('kNotifications');
    await checkToggle('kLocation');
    await checkToggle('kCamera');
    await checkToggle('kMicrophone');
  });

  test('Pin to shelf toggle', async () => {
    const pinToShelfItem =
        pwaDetailView.shadowRoot!
            .querySelector<AppManagementPinToShelfItemElement>(
                '#pinToShelfSetting');
    assertTrue(!!pinToShelfItem);
    const toggleRow =
        pinToShelfItem.shadowRoot!.querySelector<AppManagementToggleRowElement>(
            '#toggleRow');
    assertTrue(!!toggleRow);
    const toggle = toggleRow.$.toggle;

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

  test('Show sub apps correctly', async () => {
    const subAppOptions = {
      installReason: InstallReason.kSubApp,
    };
    const sub1 = await fakeHandler.addApp('Sub1', subAppOptions);
    const sub2 = await fakeHandler.addApp('Sub2', subAppOptions);
    const parent = await fakeHandler.addApp();
    AppManagementStore.getInstance().dispatch(
        updateSubAppToParentAppId(sub1.id, parent.id));
    AppManagementStore.getInstance().dispatch(
        updateSubAppToParentAppId(sub2.id, parent.id));
    await fakeHandler.flushPipesForTesting();

    // Navigate through parent and sub app setting pages so all elements get
    // created.
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(parent.id));
    await fakeHandler.flushPipesForTesting();
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(sub1.id));
    await fakeHandler.flushPipesForTesting();
    AppManagementStore.getInstance().dispatch(
        updateSelectedAppId(defaultAppId));
    await fakeHandler.flushPipesForTesting();

    const subAppsItem: AppManagementSubAppsItemElement =
        pwaDetailView.shadowRoot!.querySelector('#subAppsItem')!;
    const permissionHeading =
        pwaDetailView.shadowRoot!.querySelector('#permissionHeading')!;
    const ParentAppPermissionExplanation =
        permissionHeading.shadowRoot!.querySelector(
            '#parentAppPermissionExplanation')!;
    const SubAppPermissionExplanation =
        permissionHeading.shadowRoot!.querySelector(
            '#subAppPermissionExplanation')!;

    const assertParentAppExplanationShown = (shown: boolean) => {
      const not = shown ? '' : 'not ';
      assertEquals(
          ParentAppPermissionExplanation.checkVisibility(), shown,
          'permission explanation for parent app should ' + not + 'be shown');
    };

    const assertSubAppExplanationShown = (shown: boolean) => {
      const not = shown ? '' : 'not ';
      assertEquals(
          SubAppPermissionExplanation.checkVisibility(), shown,
          'permission explanation for sub app should ' + not + 'be shown');
    };

    const checkToggleDisabled = (permissionType: PermissionTypeIndex) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(
          (getPermissionCrToggleByType(pwaDetailView, permissionType) as
           CrToggleElement)
              .disabled,
          'permission toggle should be disabled on sub app setting page');
    };

    // Default app is shown, has neither parents nor sub apps.
    assertEquals(
        0, subAppsItem.subApps.length, 'list of sub apps is not empty');
    assertTrue(subAppsItem.hidden, 'list of sub apps should be hidden');
    assertParentAppExplanationShown(false);
    assertSubAppExplanationShown(false);

    // Parent app with two sub apps gets selected.
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(parent.id));
    await fakeHandler.flushPipesForTesting();

    assertEquals(
        2, subAppsItem.subApps.length, 'list of sub apps should show two apps');
    assertFalse(subAppsItem.hidden, 'list of sub apps should not be hidden');
    assertParentAppExplanationShown(true);
    assertSubAppExplanationShown(false);

    // Select a sub app, has one parent and no sub apps of its own.
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(sub1.id));
    await fakeHandler.flushPipesForTesting();

    assertEquals(
        0, subAppsItem.subApps.length, 'list of sub apps is not empty');
    assertTrue(subAppsItem.hidden, 'list of sub apps should be hidden');
    assertParentAppExplanationShown(false);
    assertSubAppExplanationShown(true);

    checkToggleDisabled('kLocation');
    checkToggleDisabled('kCamera');
    checkToggleDisabled('kMicrophone');
  });
});
