// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import type {FileSystemGrant, FileSystemSiteDetailsElement, OriginFileSystemGrants} from 'chrome://settings/lazy_load.js';
import {SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import {loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

suite('FileSystemSettings_EnablePersistentPermissions_SiteDetails', function() {
  let testElement: FileSystemSiteDetailsElement;
  let browserProxy: TestSiteSettingsBrowserProxy;

  const origin: string = 'https://a.com/';
  const filePath: string = 'a/b';
  const directoryFilePath: string = 'g/h/';

  const TEST_FILE_SYSTEM_FILE_WRITE_GRANT: FileSystemGrant = {
    filePath: filePath,
    displayName: filePath,
    isDirectory: false,
  };
  const TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT: FileSystemGrant = {
    filePath: directoryFilePath,
    displayName: directoryFilePath,
    isDirectory: true,
  };
  const TEST_FILE_SYSTEM_GRANTS_PER_ORIGIN: OriginFileSystemGrants = {
    origin: origin,
    viewGrants: [
      TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT,
    ],
    editGrants: [
      TEST_FILE_SYSTEM_FILE_WRITE_GRANT,
    ],
  };
  const FILE_SYSTEM_GRANTS: OriginFileSystemGrants[] =
      [TEST_FILE_SYSTEM_GRANTS_PER_ORIGIN];
  const FILE_SYSTEM_GRANTS_EMPTY: OriginFileSystemGrants[] = [];

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enablePersistentPermissions: true,
    });
  });

  // Initialize the file-system-site-details element.
  setup(async function() {
    browserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(browserProxy);
    browserProxy.setFileSystemGrants(FILE_SYSTEM_GRANTS);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = document.createElement('file-system-site-details');
    document.body.appendChild(testElement);
    const params = new URLSearchParams();
    params.append('site', origin);
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS, params);
    await browserProxy.whenCalled('getFileSystemGrants');
    await microtasksFinished();
  });

  test('FileSystemSiteDetails_FileSystemSiteEntryItemsPopulated', function() {
    const fileSystemSiteDetailsElements =
        testElement.shadowRoot.querySelectorAll('file-system-site-entry-item');
    assertEquals(fileSystemSiteDetailsElements.length, 2);
  });

  test('FileSystemSiteDetails_NoGrants', async function() {
    // Simulate an origin that previously had permission grants, and now has
    // none due to a change in chooser permissions being updated.
    assertEquals(
        routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS,
        Router.getInstance().getCurrentRoute());
    browserProxy.setFileSystemGrants(FILE_SYSTEM_GRANTS_EMPTY);
    webUIListenerCallback(
        'contentSettingChooserPermissionChanged', 'file-system-write');
    await browserProxy.whenCalled('getFileSystemGrants');
    await microtasksFinished();
    assertEquals(
        Router.getInstance().getCurrentRoute(),
        routes.SITE_SETTINGS_FILE_SYSTEM_WRITE);
  });

  test('FileSystemSiteDetails_NoRouteHijackWhenInactive', async function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
    await microtasksFinished();
    assertEquals(
        routes.SITE_SETTINGS_ALL, Router.getInstance().getCurrentRoute());
    browserProxy.resetResolver('getFileSystemGrants');

    browserProxy.setFileSystemGrants(FILE_SYSTEM_GRANTS_EMPTY);
    webUIListenerCallback(
        'contentSettingChooserPermissionChanged', 'file-system-write');
    await microtasksFinished();

    assertEquals(
        routes.SITE_SETTINGS_ALL, Router.getInstance().getCurrentRoute());
    assertEquals(0, browserProxy.getCallCount('getFileSystemGrants'));
  });

  test(
      'FileSystemSiteDetails_InvalidOriginNavigatesToPreviousRoute',
      async function() {
        const subpage = testElement.shadowRoot.querySelector('settings-subpage');
        assertTrue(subpage !== null);
        assertEquals(origin, subpage.pageTitle);

        Router.getInstance().navigateTo(routes.SITE_SETTINGS_ALL);
        await microtasksFinished();
        assertEquals(
            routes.SITE_SETTINGS_ALL, Router.getInstance().getCurrentRoute());
        browserProxy.resetResolver('isOriginValid');
        browserProxy.resetResolver('getFileSystemGrants');
        browserProxy.setIsOriginValid(false);

        const whenPopState = eventToPromise('popstate', window);
        const params = new URLSearchParams();
        params.append('site', 'invalid-origin');
        Router.getInstance().navigateTo(
            routes.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS, params);
        await browserProxy.whenCalled('isOriginValid');
        await whenPopState;
        await microtasksFinished();

        assertEquals(
            routes.SITE_SETTINGS_ALL, Router.getInstance().getCurrentRoute());
        assertEquals(0, browserProxy.getCallCount('getFileSystemGrants'));
        assertEquals(origin, subpage.pageTitle);
      });
});
