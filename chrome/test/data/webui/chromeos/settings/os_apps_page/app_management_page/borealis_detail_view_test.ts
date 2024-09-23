// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementBorealisDetailViewElement, AppManagementPinToShelfItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, AppManagementToggleRowElement, CrToggleElement, Router, updateSelectedAppId} from 'chrome://os-settings/os_settings.js';
import {App, AppType, Permission, PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {createBoolPermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {FakePageHandler} from '../../app_management/fake_page_handler.js';
import {getPermissionCrToggleByType, getPermissionToggleByType, replaceBody, replaceStore, setupFakeHandler} from '../../app_management/test_util.js';

suite('<app-management-borealis-detail-view>', () => {
  let borealisDetailView: AppManagementBorealisDetailViewElement;
  let fakeHandler: FakePageHandler;

  const BOREALIS_CLIENT_APP_ID = 'epfhbkiklgmlkhfpbcdleadnhcfdjfmo';

  function getPermissionBoolByType(permissionType: PermissionTypeIndex):
      boolean {
    return getPermissionValueBool(
        borealisDetailView.get('app_'), permissionType);
  }

  async function clickToggle(permissionType: PermissionTypeIndex):
      Promise<void> {
    const toggleRow =
        getPermissionToggleByType(borealisDetailView, permissionType) as
        AppManagementToggleRowElement;
    toggleRow.click();
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

    const permissions: {[key in PermissionType]?: Permission} = {};
    const permissionTypes = [PermissionType.kMicrophone];
    for (const permissionType of permissionTypes) {
      permissions[permissionType] = createBoolPermission(
          permissionType, true /*permission_value*/, false /*is_managed*/);
    }

    // Add main app, and make it the currently selected app.
    const mainOptions = {
      type: AppType.kBorealis,
      permissions: permissions,
    };
    const mainApp =
        await fakeHandler.addApp(BOREALIS_CLIENT_APP_ID, mainOptions);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(mainApp.id));
    borealisDetailView =
        document.createElement('app-management-borealis-detail-view');
    replaceBody(borealisDetailView);
  });

  teardown(() => {
    borealisDetailView.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('App is rendered correctly', () => {
    assertEquals(
        AppManagementStore.getInstance().data.selectedAppId,
        borealisDetailView.get('app_').id);
  });

  // TODO(b/270728282) - remove "as" cast once getPermissionCrToggleByType()
  // becomes a TS function.
  test('Toggle permissions', async () => {
    const checkToggle = async (permissionType: PermissionTypeIndex) => {
      assertTrue(getPermissionBoolByType(permissionType));
      let toggle = getPermissionCrToggleByType(
                       borealisDetailView, permissionType) as CrToggleElement;
      assertTrue(toggle.checked);

      // Toggle off.
      await clickToggle(permissionType);
      assertFalse(getPermissionBoolByType(permissionType));
      toggle = getPermissionCrToggleByType(
                   borealisDetailView, permissionType) as CrToggleElement;
      assertFalse(toggle.checked);

      // Toggle on.
      await clickToggle(permissionType);
      assertTrue(getPermissionBoolByType(permissionType));
      toggle = getPermissionCrToggleByType(
                   borealisDetailView, permissionType) as CrToggleElement;
      assertTrue(toggle.checked);
    };

    await checkToggle('kMicrophone');
  });

  test('Pin to shelf toggle', async () => {
    const pinToShelfItem =
        borealisDetailView.shadowRoot!
            .querySelector<AppManagementPinToShelfItemElement>(
                '#pinToShelfSetting');
    assertTrue(!!pinToShelfItem);
    const toggleRow =
        pinToShelfItem.shadowRoot!.querySelector<AppManagementToggleRowElement>(
            '#toggleRow');
    assertTrue(!!toggleRow);
    const toggle = toggleRow.$.toggle;

    let selectedAppId = getSelectedAppFromStore();
    assertTrue(!!selectedAppId);
    assertFalse(toggle.checked);
    assertEquals(toggle.checked, selectedAppId.isPinned);
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertTrue(toggle.checked);
    selectedAppId = getSelectedAppFromStore();
    assertTrue(!!selectedAppId);
    assertEquals(toggle.checked, selectedAppId.isPinned);
    pinToShelfItem.click();
    await fakeHandler.flushPipesForTesting();
    assertFalse(toggle.checked);
    selectedAppId = getSelectedAppFromStore();
    assertTrue(!!selectedAppId);
    assertEquals(toggle.checked, selectedAppId.isPinned);
  });

  test('Permission info links are correct', async () => {
    assertTrue(!!borealisDetailView.shadowRoot!.querySelector('#mainLink'));
    assertNull(borealisDetailView.shadowRoot!.querySelector('#borealisLink'));

    // Add borealis (non main) app. Note that any tests after this will
    // have the borealis app selected as default.
    const options = {
      type: AppType.kBorealis,
    };
    const app = await fakeHandler.addApp('foo', options);
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(app.id));
    await fakeHandler.flushPipesForTesting();
    assertNull(borealisDetailView.shadowRoot!.querySelector('#mainLink'));
    assertTrue(!!borealisDetailView.shadowRoot!.querySelector('#borealisLink'));

    // Check that link directs to main app page.
    const link = borealisDetailView.shadowRoot!.querySelector('#borealisLink');
    assertTrue(!!link);
    const anchorTag = link.shadowRoot!.querySelector('a');
    assertTrue(!!anchorTag);
    const localizedLinkPromise = eventToPromise('link-clicked', link);
    anchorTag.click();
    await Promise.all([localizedLinkPromise, flushTasks()]);
    await fakeHandler.flushPipesForTesting();
    assertEquals(
        Router.getInstance().getQueryParameters().get('id'),
        BOREALIS_CLIENT_APP_ID);
  });
});
