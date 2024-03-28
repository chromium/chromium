// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {FileSystemGrant, FileSystemSiteEntryItemElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

// clang-format on

suite(
    'FileSystemSettings_EnablePersistentPermissions_SiteEntryItem', function() {
      let testElement: FileSystemSiteEntryItemElement;
      const directoryFilePath: string = 'a/';
      const TEST_FILE_SYSTEM_DIRECTORY_GRANT: FileSystemGrant = {
        filePath: directoryFilePath,
        displayName: directoryFilePath,
        isDirectory: true,
      };
      const filePath: string = 'a/b';
      const TEST_FILE_SYSTEM_FILE_GRANT: FileSystemGrant = {
        filePath: filePath,
        displayName: filePath,
        isDirectory: false,
      };

      suiteSetup(function() {
        CrSettingsPrefs.setInitialized();

        loadTimeData.overrideValues({
          showPersistentPermissions: true,
        });
      });

      // Initialize the file-system-site-entry-item element for a directory
      // grant.
      setup(function() {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        testElement = document.createElement('file-system-site-entry-item');
        document.body.appendChild(testElement);
      });

      test('FileSystemSiteListEntryItemsPopulated_DirectoryGrant', function() {
        testElement.grant = TEST_FILE_SYSTEM_DIRECTORY_GRANT;
        flush();
        const directoryGrantDisplayName =
            testElement.shadowRoot!.querySelector('.display-name');
        assertTrue(!!directoryGrantDisplayName);
        const icon = testElement.shadowRoot!.querySelector('#fileTypeIcon');
        assertTrue(!!icon);
        assertTrue(icon.classList.contains('icon-folder-open'));
      });

      test('FileSystemSiteListEntryItemsPopulated_FileGrant', function() {
        testElement.grant = TEST_FILE_SYSTEM_FILE_GRANT;
        flush();
        const fileGrantDisplayName =
            testElement.shadowRoot!.querySelector('.display-name');
        assertTrue(!!fileGrantDisplayName);
        const icon = testElement.shadowRoot!.querySelector('#fileTypeIcon');
        assertTrue(!!icon);
        assertTrue(icon.classList.contains('icon-file'));
      });

      test(
          'FileSystemSiteListEntryItemRemoveIndiviudalPermissionGrant',
          async function() {
            testElement.grant = TEST_FILE_SYSTEM_FILE_GRANT;
            flush();
            const whenFired = eventToPromise('revoke-grant', testElement);
            testElement.$.removeGrant.click();
            const permissionRemovedEvent = await whenFired;
            const {filePath} = permissionRemovedEvent.detail;
            assertEquals(TEST_FILE_SYSTEM_FILE_GRANT.filePath, filePath);
          });
    });
