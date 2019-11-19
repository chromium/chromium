// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-options-dialog. */
import {OptionsDialogMaxHeight, OptionsDialogMinWidth, Service} from 'chrome://extensions/extensions.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {eventToPromise} from '../test_util.m.js';

window.extension_options_dialog_tests = {};
extension_options_dialog_tests.suiteName = 'ExtensionOptionsDialogTests';
/** @enum {string} */
extension_options_dialog_tests.TestNames = {
  Layout: 'Layout',
};

suite(extension_options_dialog_tests.suiteName, function() {
  /** @type {ExtensionsOptionsDialogElement} */
  let optionsDialog;

  /** @type {chrome.developerPrivate.ExtensionInfo} */
  let data;

  setup(function() {
    PolymerTest.clearBody();
    optionsDialog = document.createElement('extensions-options-dialog');
    document.body.appendChild(optionsDialog);

    const service = Service.getInstance();
    return service.getExtensionsInfo().then(function(info) {
      assertEquals(1, info.length);
      data = info[0];
    });
  });

  function isDialogVisible() {
    const dialogElement = optionsDialog.$.dialog.getNative();
    const rect = dialogElement.getBoundingClientRect();
    return rect.width * rect.height > 0;
  }

  test(assert(extension_options_dialog_tests.TestNames.Layout), function() {
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

          assertEquals(
              data.name,
              assert(optionsDialog.$$('#icon-and-name-wrapper span'))
                  .textContent.trim());
        });
  });
});
