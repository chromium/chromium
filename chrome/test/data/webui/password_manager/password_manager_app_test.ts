// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {OpenWindowProxyImpl, Page, PasswordManagerAppElement, PasswordManagerImpl, Router, UrlParam} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createCredentialGroup, createPasswordEntry, makePasswordManagerPrefs} from './test_util.js';

suite('PasswordManagerAppTest', function() {
  let app: PasswordManagerAppElement;

  let openWindowProxy: TestOpenWindowProxy;
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    app = document.createElement('password-manager-app');
    document.body.appendChild(app);
    app.setNarrowForTesting(false);
    return flushTasks();
  });

  test('check layout', function() {
    assertTrue(isVisible(app));
    assertTrue(isVisible(app.$.toolbar));
    assertTrue(isVisible(app.$.content));
    assertTrue(isVisible(app.$.sidebar));
  });

  test('UI search box updates URL parameters', function() {
    app.$.toolbar.$.mainToolbar.getSearchField().setValue('hello');

    assertEquals(
        'hello',
        String(Router.getInstance().currentRoute.queryParameters.get(
            UrlParam.SEARCH_TERM)));
  });

  test('URL parameters update UI search box', function() {
    const query = new URLSearchParams();
    query.set(UrlParam.SEARCH_TERM, 'test');
    Router.getInstance().updateRouterParams(query);
    assertEquals(
        'test', app.$.toolbar.$.mainToolbar.getSearchField().getValue());
  });

  [Page.PASSWORDS, Page.CHECKUP, Page.SETTINGS].forEach(
      page => test(`Clicking ${page} in the sidebar`, function() {
        const element =
            app.$.sidebar.shadowRoot!.querySelector<HTMLElement>(`#${page}`)!;
        element.click();
        const ironItem =
            app.$.sidebar.shadowRoot!.querySelector<HTMLElement>(`#${page}`)!;
        assertTrue(ironItem.classList.contains('iron-selected'));
        if (page === Page.CHECKUP) {
          assertEquals(
              'true',
              String(Router.getInstance().currentRoute.queryParameters.get(
                  UrlParam.START_CHECK)));
        }
      }));

  test('app drawer', async () => {
    assertEquals(null, app.shadowRoot!.querySelector('#drawerSidebar'));
    assertFalse(!!app.$.drawer.open);

    const drawerOpened = eventToPromise('cr-drawer-opened', app.$.drawer);
    app.$.drawer.openDrawer();
    flush();

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(app.$.drawer.open);
    assertTrue(!!app.shadowRoot!.querySelector('#drawerSidebar'));

    await drawerOpened;
    const drawerClosed = eventToPromise('close', app.$.drawer);
    app.$.drawer.cancel();

    await drawerClosed;
    // Drawer is closed, but menu is still stamped so
    // its contents remain visible as the drawer slides
    // out.
    assertTrue(!!app.shadowRoot!.querySelector('#drawerSidebar'));
  });

  test('drawer hides when enough space', async () => {
    app.setNarrowForTesting(true);

    assertEquals(null, app.shadowRoot!.querySelector('#drawerSidebar'));
    assertFalse(!!app.$.drawer.open);

    const drawerOpened = eventToPromise('cr-drawer-opened', app.$.drawer);
    app.$.drawer.openDrawer();
    flush();

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(app.$.drawer.open);
    assertTrue(!!app.shadowRoot!.querySelector('#drawerSidebar'));

    await drawerOpened;
    const drawerClosed = eventToPromise('close', app.$.drawer);
    app.setNarrowForTesting(false);

    await drawerClosed;
    // Drawer is closed, but menu is still stamped so
    // its contents remain visible as the drawer slides
    // out.
    assertTrue(!!app.shadowRoot!.querySelector('#drawerSidebar'));
  });

  test('Search navigates to Passwords and updates URL parameters', function() {
    const query = new URLSearchParams();
    query.set(UrlParam.START_CHECK, 'true');
    Router.getInstance().navigateTo(Page.CHECKUP, null, query);

    app.$.toolbar.$.mainToolbar.getSearchField().setValue('hello');

    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
    assertEquals(
        'hello',
        String(Router.getInstance().currentRoute.queryParameters.get(
            UrlParam.SEARCH_TERM)));
    assertFalse(Router.getInstance().currentRoute.queryParameters.has(
        UrlParam.START_CHECK));
  });

  test('Test help button', async function() {
    const button =
        app.$.toolbar.shadowRoot!.querySelector<HTMLElement>('#helpButton');
    assertTrue(!!button);
    button.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('passwordManagerLearnMoreURL'));
  });

  test('Test password removal toast', async () => {
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0, username: 'test1'}),
      ],
    });
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    await flushTasks();

    assertFalse(app.$.removalToast.open);
    const detailsSection =
        app.shadowRoot!.querySelector('password-details-section');
    assertTrue(!!detailsSection);

    detailsSection.dispatchEvent(new CustomEvent('password-removed', {
      bubbles: true,
      composed: true,
      detail: {
        removedFromStores: chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
      },
    }));

    assertTrue(app.$.removalToast.open);
    const undoButton =
        app.shadowRoot!.querySelector<HTMLElement>('#undo-removal');
    assertTrue(!!undoButton);
    assertFalse(undoButton.hidden);
    undoButton.click();

    await passwordManager.whenCalled('undoRemoveSavedPasswordOrException');
  });

  test('Test passkey removal toast', async () => {
    const group = createCredentialGroup({
      name: 'test.com',
      credentials: [
        createPasswordEntry({id: 0, username: 'test1', isPasskey: true}),
      ],
    });
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, group);

    await flushTasks();

    assertFalse(app.$.removalToast.open);
    const detailsSection =
        app.shadowRoot!.querySelector('password-details-section');
    assertTrue(!!detailsSection);

    detailsSection.dispatchEvent(new CustomEvent('passkey-removed', {
      bubbles: true,
      composed: true,
    }));

    assertTrue(app.$.removalToast.open);

    // The undo button should be hidden for passkeys.
    const undoButton =
        app.shadowRoot!.querySelector<HTMLElement>('#undo-removal');
    assertTrue(!!undoButton);
    assertTrue(undoButton.hidden);
  });

  test('import can be triggered from empty state', async function() {
    // This is done to avoid flakiness.
    Router.getInstance().navigateTo(Page.PASSWORDS);
    await flushTasks();

    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);

    const passwordsSection = app.shadowRoot!.querySelector('passwords-section');
    assertTrue(!!passwordsSection);
    passwordsSection.prefs = makePasswordManagerPrefs();
    await flushTasks();
    const importLink = passwordsSection.$.importPasswords.querySelector('a');
    assertTrue(!!importLink);

    // Should redirect ot Settings page.
    importLink.click();
    await flushTasks();

    assertEquals(Page.SETTINGS, Router.getInstance().currentRoute.page);
    const settingsSection = app.shadowRoot!.querySelector('settings-section');
    assertTrue(!!settingsSection);
    settingsSection.prefs = makePasswordManagerPrefs();
    await flushTasks();

    const importer =
        settingsSection.shadowRoot!.querySelector('passwords-importer');
    assertTrue(!!importer);

    const spinner = importer.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);
  });
});
