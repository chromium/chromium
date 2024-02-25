// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppManagementPinToShelfItemElement, AppManagementPwaDetailViewElement, AppManagementSubAppsItemElement} from 'chrome://os-settings/lazy_load.js';
import {AppManagementStore, AppManagementToggleRowElement, CrToggleElement, Router, updateSelectedAppId, updateSubAppToParentAppId} from 'chrome://os-settings/os_settings.js';
import {App, InstallReason} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PermissionTypeIndex} from 'chrome://resources/cr_components/app_management/permission_constants.js';
import {getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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

  async function selectPageFor(appId: string) {
    AppManagementStore.getInstance().dispatch(updateSelectedAppId(appId));
    await fakeHandler.flushPipesForTesting();
  }

  async function addSubApp(title: string, parentApp: App): Promise<string> {
    const subAppOptions = {
      installReason: InstallReason.kSubApp,
    };
    const sub_app = await fakeHandler.addApp(title, subAppOptions);
    AppManagementStore.getInstance().dispatch(
        updateSubAppToParentAppId(sub_app.id, parentApp.id));
    return sub_app.id;
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
    Router.getInstance().resetRouteForTesting();
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

  test('Show list of sub apps correctly', async () => {
    const parentApp = await fakeHandler.addApp();
    const subAppId = await addSubApp('Sub1', parentApp);
    await addSubApp('Sub2', parentApp);

    const subAppsItem: AppManagementSubAppsItemElement =
        pwaDetailView.shadowRoot!.querySelector('#subAppsItem')!;

    // Default app is shown, has neither parents nor sub apps.
    assertEquals(
        0, subAppsItem.subApps.length, 'list of sub apps is not empty');
    assertTrue(subAppsItem.hidden, 'list of sub apps should be hidden');

    await selectPageFor(parentApp.id);
    assertEquals(
        2, subAppsItem.subApps.length, 'list of sub apps should show two apps');
    assertFalse(subAppsItem.hidden, 'list of sub apps should not be hidden');

    await selectPageFor(subAppId);
    assertEquals(
        0, subAppsItem.subApps.length, 'list of sub apps is not empty');
    assertTrue(subAppsItem.hidden, 'list of sub apps should be hidden');
  });

  test('Show sub and parent app permission explanations', async () => {
    const parentApp = await fakeHandler.addApp();
    const subAppId = await addSubApp('Sub1', parentApp);
    await addSubApp('Sub2', parentApp);

    // Navigate through parent and sub app setting pages so all elements get
    // created.
    await selectPageFor(parentApp.id);
    await selectPageFor(subAppId);
    await selectPageFor(defaultAppId);

    const permissionHeading =
        pwaDetailView.shadowRoot!.querySelector('#permissionHeading')!;
    const ParentAppPermissionExplanation =
        permissionHeading.shadowRoot!.querySelector(
            '#parentAppPermissionExplanation')!;
    const SubAppPermissionExplanation =
        permissionHeading.shadowRoot!.querySelector(
            '#subAppPermissionExplanation')!;

    const assertParentAppExplanationShown = (shown: boolean) => {
      assertEquals(
          ParentAppPermissionExplanation.checkVisibility(), shown,
          'permission explanation for parent app should ' +
              (shown ? '' : 'not ') + 'be shown');
    };

    const assertSubAppExplanationShown = (shown: boolean) => {
      assertEquals(
          SubAppPermissionExplanation.checkVisibility(), shown,
          'permission explanation for sub app should ' + (shown ? '' : 'not ') +
              'be shown');
    };


    // Default app is shown, has neither parents nor sub apps.
    assertParentAppExplanationShown(false);
    assertSubAppExplanationShown(false);

    await selectPageFor(parentApp.id);
    assertParentAppExplanationShown(true);
    assertSubAppExplanationShown(false);

    await selectPageFor(subAppId);
    assertParentAppExplanationShown(false);
    assertSubAppExplanationShown(true);

    // Explanation is hidden again when navigating to regular app.
    await selectPageFor(defaultAppId);
    assertParentAppExplanationShown(false);
    assertSubAppExplanationShown(false);
  });

  test(
      'Manage permissions link on sub app page directs to parent app settings page',
      async () => {
        const parentApp = await fakeHandler.addApp();
        const subAppId = await addSubApp('Sub1', parentApp);
        await selectPageFor(subAppId);

        const permissionHeading =
            pwaDetailView.shadowRoot!.querySelector('#permissionHeading')!;
        const link = permissionHeading.shadowRoot!.querySelector(
            '#subAppPermissionExplanation')!;
        const anchorTag = link.shadowRoot!.querySelector('a')!;
        const localizedLinkPromise = eventToPromise('link-clicked', link);

        anchorTag.click();
        await Promise.all([localizedLinkPromise, flushTasks()]);
        await fakeHandler.flushPipesForTesting();

        assertEquals(
            Router.getInstance().getQueryParameters().get('id'), parentApp.id);
      });

  test('Permission toggles disabled on sub app page', async () => {
    const parentApp = await fakeHandler.addApp();
    const subAppId = await addSubApp('Sub1', parentApp);
    await addSubApp('Sub2', parentApp);
    await selectPageFor(subAppId);

    const checkToggleDisabled = (permissionType: PermissionTypeIndex) => {
      assertTrue(getPermissionBoolByType(permissionType));
      assertTrue(
          (getPermissionCrToggleByType(pwaDetailView, permissionType) as
           CrToggleElement)
              .disabled,
          'permission toggle should be disabled on sub app setting page');
    };

    checkToggleDisabled('kLocation');
    checkToggleDisabled('kCamera');
    checkToggleDisabled('kMicrophone');
  });
});
