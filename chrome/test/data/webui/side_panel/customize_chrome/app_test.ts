// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://customize-chrome-side-panel.top-chrome/app.js';

import {AppElement} from 'chrome://customize-chrome-side-panel.top-chrome/app.js';
import {BackgroundCollection} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('AppTest', () => {
  let customizeChromeApp: AppElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    customizeChromeApp = document.createElement('customize-chrome-app');
    document.body.appendChild(customizeChromeApp);
  });

  test('app changes pages', async () => {
    const testCollection: BackgroundCollection = {
      id: 'test',
      label: 'test',
      previewImageUrl: {url: 'https://test.jpg'},
    };

    // Test initial page state.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('iron-selected'));

    // Send event for edit theme being clicked.
    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    // Current page should now be categories.
    assertTrue(customizeChromeApp.$.categoriesPage.classList.contains(
        'iron-selected'));

    // Send event for category selected.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new CustomEvent<BackgroundCollection>(
            'collection-select', {detail: testCollection}));
    // Current page should now be themes.
    assertTrue(
        customizeChromeApp.$.themesPage.classList.contains('iron-selected'));

    // Send event for theme selected.
    customizeChromeApp.$.themesPage.dispatchEvent(new Event('theme-select'));
    // Current page should now be overview.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('iron-selected'));

    // Set page back to themes and then go back a page.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new CustomEvent<BackgroundCollection>(
            'collection-select', {detail: testCollection}));
    customizeChromeApp.$.themesPage.dispatchEvent(new Event('back-click'));
    // Current page should now be categories.
    assertTrue(customizeChromeApp.$.categoriesPage.classList.contains(
        'iron-selected'));
    // Go back again.
    customizeChromeApp.$.categoriesPage.dispatchEvent(new Event('back-click'));
    // Current page should now be overview.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('iron-selected'));
  });
});
