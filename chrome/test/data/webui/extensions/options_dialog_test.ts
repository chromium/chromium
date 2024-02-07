// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-options-dialog. */
import 'chrome://extensions/extensions.js';

import type {ExtensionsOptionsDialogElement} from 'chrome://extensions/extensions.js';
import {OptionsDialogMaxHeight, OptionsDialogMinWidth, Service} from 'chrome://extensions/extensions.js';
import {assertEquals, assertFalse, assertGE, assertLE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('ExtensionOptionsDialogTests', function() {
  let optionsDialog: ExtensionsOptionsDialogElement;
  let data: chrome.developerPrivate.ExtensionInfo;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    optionsDialog = document.createElement('extensions-options-dialog');
    document.body.appendChild(optionsDialog);

    const service = Service.getInstance();
    return service.getExtensionsInfo().then(function(info) {
      assertEquals(1, info.length);
      data = info[0]!;
    });
  });

  function isDialogVisible(): boolean {
    const dialogElement = optionsDialog.$.dialog.getNative();
    const rect = dialogElement.getBoundingClientRect();
    return rect.width * rect.height > 0;
  }

  test('Layout', function() {
    // Try showing the dialog.
    assertFalse(isDialogVisible());
    optionsDialog.show(data);
    return eventToPromise('cr-dialog-open', optionsDialog)
        .then(() => {
          // Wait more than 50ms for the debounced size update.
          return new Promise(r => setTimeout(r, 100));
        })
        .then(() => {
          assertTrue(isDialogVisible());

          const dialogElement = optionsDialog.$.dialog.getNative();
          const rect = dialogElement.getBoundingClientRect();
          assertGE(rect.width, OptionsDialogMinWidth);
          assertLE(rect.height, OptionsDialogMaxHeight);
          // This is the header height with default font size.
          assertGE(rect.height, 68);
          const nameElement = optionsDialog.shadowRoot!.querySelector(
              '#icon-and-name-wrapper span');
          assertTrue(!!nameElement);
          assertEquals(data.name, nameElement.textContent!.trim());
        });
  });
});
