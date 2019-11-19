// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for FocusOutlineManager. Runs in interactive_ui_tests
 * because it manipulates window focus.
 */

GEN_INCLUDE(['//chrome/test/data/webui/polymer_interactive_ui_test.js']);

// eslint-disable-next-line no-var
var FocusOutlineManagerTest = class extends PolymerInteractiveUITest {
  /** @override */
  get browsePreload() {
    return 'chrome://resources/html/cr/ui/focus_outline_manager.html';
  }
};

TEST_F('FocusOutlineManagerTest', 'AllJsTests', function() {
  suite('FocusOutlineManager', function() {
    const documentClassList = document.documentElement.classList;

    // Inspired by test_util.flushTasks(), but without the Polymer flush.
    function flushTasks() {
      return new Promise(function(resolve, reject) {
        window.setTimeout(resolve, 0);
      });
    }

    setup(function() {
      // Start with a focused element.
      document.body.innerHTML = '<input type="text">';
      document.querySelector('input').focus();

      // Create an instance of FocusOutlineManager.
      cr.ui.FocusOutlineManager.forDocument(document);
    });

    test('key event adds focus css class to document', function() {
      window.dispatchEvent(new KeyboardEvent('keydown', {key: 'a'}));
      assertTrue(documentClassList.contains('focus-outline-visible'));
    });

    test('mouse event removes focus css class from document', function() {
      document.body.dispatchEvent(new KeyboardEvent('keydown', {key: 'a'}));
      document.body.dispatchEvent(new MouseEvent('mousedown'));
      assertFalse(documentClassList.contains('focus-outline-visible'));
    });

    // Regression test for settings side navigation focus outline appearing
    // when a new window was opened on top. https://crbug.com/993677
    test('opening a new window does not add css class', async function() {
      document.body.dispatchEvent(new MouseEvent('mousedown'));
      assertFalse(documentClassList.contains('focus-outline-visible'));

      const newWindow = window.open('about:blank', '_blank', 'resizable');
      await flushTasks();
      assertFalse(documentClassList.contains('focus-outline-visible'));

      newWindow.close();
      await flushTasks();
      assertFalse(documentClassList.contains('focus-outline-visible'));
    });

    test('opening a new window does not remove css class', async function() {
      document.body.dispatchEvent(new KeyboardEvent('keydown', {key: 'a'}));
      assertTrue(documentClassList.contains('focus-outline-visible'));

      const newWindow = window.open('about:blank', '_blank', 'resizable');
      await flushTasks();
      assertTrue(documentClassList.contains('focus-outline-visible'));

      newWindow.close();
      await flushTasks();
      assertTrue(documentClassList.contains('focus-outline-visible'));
    });
  });

  mocha.run();
});
