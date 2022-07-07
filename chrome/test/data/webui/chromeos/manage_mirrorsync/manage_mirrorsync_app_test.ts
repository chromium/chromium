// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://manage-mirrorsync/components/manage_mirrorsync.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<manage-mirrorsync>', () => {
  /* Holds the <manage-mirrorsync> app */
  let appHolder: HTMLDivElement;
  /* The <manage-mirrorsync> app, this gets cleared before every test */
  let manageMirrorSyncApp: HTMLElement;

  /**
   * Runs prior to all the tests running, attaches a div to enable isolated
   * clearing and attaching of the web component.
   */
  suiteSetup(() => {
    appHolder = document.createElement('div');
    document.body.appendChild(appHolder);
  });

  /**
   * Runs before every test. Ensures the DOM is clear of any existing
   * <manage-mirrorsync> components.
   */
  setup(() => {
    appHolder.innerHTML = '';
    manageMirrorSyncApp = document.createElement('manage-mirrorsync');
    document.body.appendChild(manageMirrorSyncApp);
  });

  /**
   * Runs after every test. Removes all elements from the <div> added to hold
   * the <manage-mirrorsync> component.
   */
  teardown(() => {
    appHolder.innerHTML = '';
  });

  /**
   * Helper function to run a querySelector over the MirrorSyncApp shadowRoot
   * and assert non-nullness.
   */
  function queryMirrorSyncShadowRoot(selector: string): HTMLElement|null {
    return manageMirrorSyncApp!.shadowRoot!.querySelector(selector);
  }

  /**
   * Returns the <folder-selector> element on the page.
   */
  function getFolderSelector(): HTMLElement {
    return queryMirrorSyncShadowRoot('folder-selector')!;
  }

  test(
      'checking individual folder selection shows folder hierarchy',
      async () => {
        // <folder-selection> should be hidden on startup.
        assertTrue(getFolderSelector().hidden);

        // Click the "Sync selected files or folders" button to show the folder
        // hierarchy web component.
        queryMirrorSyncShadowRoot('#selected')!.click();

        // Ensure the <folder-selector> element is visible after pressing the
        // checkbox.
        assertFalse(getFolderSelector().hidden);
      });
});
