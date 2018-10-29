// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extension-options-dialog. */
cr.define('extension_options_dialog_tests', function() {
  /** @enum {string} */
  const TestNames = {
    Layout: 'Layout',
  };

  const suiteName = 'ExtensionOptionsDialogTests';

  suite(suiteName, function() {
    /** @type {extensions.OptionsDialog} */
    let optionsDialog;

    /** @type {chrome.developerPrivate.ExtensionInfo} */
    let data;

    setup(function() {
      PolymerTest.clearBody();
      optionsDialog = new extensions.OptionsDialog();
      document.body.appendChild(optionsDialog);

      const service = extensions.Service.getInstance();
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

    test(assert(TestNames.Layout), function() {
      // Try showing the dialog.
      assertFalse(isDialogVisible());
      optionsDialog.show(data);
      return test_util.eventToPromise('cr-dialog-open', optionsDialog)
          .then(function() {
            // The dialog size is set asynchronously (see onpreferredsizechanged
            // in options_dialog.js) so wait one frame.
            requestAnimationFrame(function() {
              assertTrue(isDialogVisible());

              const dialogElement = optionsDialog.$.dialog.getNative();
              const rect = dialogElement.getBoundingClientRect();
              assertGE(rect.width, extensions.OptionsDialogMinWidth);
              assertLE(rect.height, extensions.OptionsDialogMaxHeight);
              // This is the header height with default font size.
              assertGE(rect.height, 68);

              assertEquals(
                  data.name,
                  assert(optionsDialog.$$('#icon-and-name-wrapper span'))
                      .textContent.trim());
            });
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
