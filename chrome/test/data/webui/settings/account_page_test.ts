// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsAccountPageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, Router, routes, SignedInState, StatusAction} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';


suite('AccountPageTests', function() {
  let accountSettingsPage: SettingsAccountPageElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: true});

    accountSettingsPage = createSettingsAccountPageElement();
    Router.getInstance().navigateTo(routes.ACCOUNT);

    return microtasksFinished();
  });

  function createSettingsAccountPageElement(): SettingsAccountPageElement {
    const element = document.createElement('settings-account-page');
    element.syncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    document.body.appendChild(element);
    return element;
  }

  // Tests that all elements are visible.
  test('ShowCorrectRows', function() {
    assertEquals(routes.ACCOUNT, Router.getInstance().getCurrentRoute());

    assertTrue(!!accountSettingsPage.shadowRoot!.querySelector(
        'settings-sync-account-control'));
    assertTrue(!!accountSettingsPage.shadowRoot!.querySelector(
        'settings-sync-controls'));
  });

  // Tests that we navigate back to the people page if the user is not signed
  // in.
  test('accountSettingsPageUnavailableWhenNotSignedIn', async function() {
    accountSettingsPage.syncStatus = {
      signedInState: SignedInState.SIGNED_OUT,
      statusAction: StatusAction.NO_ACTION,
    };
    await microtasksFinished();

    assertEquals(routes.PEOPLE, Router.getInstance().getCurrentRoute());
  });

  // Tests that we navigate back to the people page if the flag is disabled.
  test('accountSettingsPageUnavailableWithoutFlag', async function() {
    loadTimeData.overrideValues({replaceSyncPromosWithSignInPromos: false});

    // Recreate the element with the flag disabled and trigger a call to
    // `syncStatusChanged_`.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    accountSettingsPage = createSettingsAccountPageElement();
    Router.getInstance().navigateTo(routes.ACCOUNT);
    accountSettingsPage.syncStatus = {
      signedInState: SignedInState.SIGNED_IN,
      statusAction: StatusAction.NO_ACTION,
    };
    await microtasksFinished();

    assertEquals(routes.PEOPLE, Router.getInstance().getCurrentRoute());
  });
});
