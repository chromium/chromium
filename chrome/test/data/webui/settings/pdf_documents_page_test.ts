// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {PdfDocumentsPageElement} from 'chrome://settings/lazy_load.js';
import {loadTimeData, Router, routes} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('PdfDocumentsPage', function() {
  let page: PdfDocumentsPageElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-pdf-documents-page');
    page.setAttribute('route-path', routes.SITE_SETTINGS_PDF_DOCUMENTS.path);
    document.body.appendChild(page);
    return flushTasks();
  });

  // Test that the tab title is updated when the subpage becomes active.
  test('UpdatesTitle', async function() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_PDF_DOCUMENTS);
    await flushTasks();
    assertEquals(
        loadTimeData.getStringF(
            'settingsAltPageTitle',
            loadTimeData.getString('siteSettingsPdfDocuments')),
        document.title);
  });
});
