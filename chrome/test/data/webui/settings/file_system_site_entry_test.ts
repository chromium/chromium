// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FileSystemGrant, FileSystemSiteEntryElement, OriginFileSystemGrants} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

// clang-format on
suite('FileSystemSettings_EnablePersistentPermissions', function() {
  let testElement: FileSystemSiteEntryElement;

  suiteSetup(function() {
    CrSettingsPrefs.setInitialized();

    loadTimeData.overrideValues({
      showPersistentPermissions: true,
    });
  });

  // Initialize the file-system-site-list element.
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testElement = new FileSystemSiteEntryElement();
    document.body.appendChild(testElement);
  });

  test('FileSystemSiteListEntriesPopulated', function() {
    const origin: string = 'https://a.com/';
    const filePath1: string = 'a/b';
    const filePath2: string = 'a/b/c';
    const filePath3: string = 'e/f';
    const directoryFilePath1: string = 'g/h/';
    const directoryFilePath2: string = 'i/';

    const TEST_FILE_SYSTEM_FILE_WRITE_GRANT1: FileSystemGrant = {
      origin: origin,
      filePath: filePath1,
      displayName: filePath1,
      isDirectory: false,
    };
    const TEST_FILE_SYSTEM_FILE_WRITE_GRANT2: FileSystemGrant = {
      origin: origin,
      filePath: filePath2,
      displayName: filePath2,
      isDirectory: false,
    };
    const TEST_FILE_SYSTEM_FILE_READ_GRANT: FileSystemGrant = {
      origin: origin,
      filePath: filePath3,
      displayName: filePath3,
      isDirectory: false,
    };
    const TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT: FileSystemGrant = {
      origin: origin,
      filePath: directoryFilePath1,
      displayName: directoryFilePath1,
      isDirectory: true,
    };
    const TEST_FILE_SYSTEM_DIRECTORY_WRITE_GRANT: FileSystemGrant = {
      origin: origin,
      filePath: directoryFilePath2,
      displayName: directoryFilePath2,
      isDirectory: true,
    };
    const TEST_FILE_SYSTEM_GRANTS_PER_ORIGIN: OriginFileSystemGrants = {
      origin: origin,
      viewGrants: [
        TEST_FILE_SYSTEM_DIRECTORY_READ_GRANT,
        TEST_FILE_SYSTEM_FILE_READ_GRANT,
      ],
      editGrants: [
        TEST_FILE_SYSTEM_DIRECTORY_WRITE_GRANT,
        TEST_FILE_SYSTEM_FILE_WRITE_GRANT1,
        TEST_FILE_SYSTEM_FILE_WRITE_GRANT2,
      ],
    };
    testElement.grantsPerOrigin = TEST_FILE_SYSTEM_GRANTS_PER_ORIGIN;
    flush();

    // The dropdown button opens the dropdown list.
    testElement.$.dropdownButton.click();
    const collapseChild = testElement.$.collapseChild;

    assertTrue(collapseChild!.opened);
    flush();

    // Ensure that the `collapseChild` element is populated as expected.
    assertEquals(
        5,
        collapseChild!.querySelectorAll('file-system-site-entry-item').length);

    // The dropdown button closes the dropdown list if tapped when opened.
    testElement.$.dropdownButton.click();
    assertFalse(collapseChild!.opened);
  });
});
