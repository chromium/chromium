// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

suite('<app-management-metadata-view>', function() {
  let metadataView;
  let fakeHandler;

  const APP_ID = '1';

  setup(async function() {
    fakeHandler = setupFakeHandler();
    replaceStore();

    // Add an app, and make it the currently selected app.
    await fakeHandler.addApp(APP_ID);
    app_management.Store.getInstance().dispatch(
        app_management.actions.changePage(PageType.DETAIL, APP_ID));

    metadataView = document.createElement('app-management-metadata-view');
    replaceBody(metadataView);
  });

  test(
      'when app.isPinned is unknown, the pin to shelf toggle is not visible',
      async function() {
        await fakeHandler.changeApp(APP_ID, {isPinned: OptionalBool.kUnknown});

        // Check that the toggle is not visible.
        const toggle = metadataView.root.getElementById('pin-to-shelf-toggle');
        if (toggle) {
          expectTrue(isHidden(toggle));
        }
      });

  test(
      'clicking the pin to shelf toggle changes the isPinned field of the app',
      async function() {
        // Set app.isPinned to false.
        await fakeHandler.changeApp(APP_ID, {isPinned: OptionalBool.kFalse});

        const toggle = metadataView.root.getElementById('pin-to-shelf-toggle');

        // Check that the toggle is visible and is not checked.
        expectTrue(!!toggle && !isHidden(toggle));
        expectFalse(toggle.checked);

        // Toggle from false to true.
        toggle.click();
        await fakeHandler.flushPipesForTesting();

        // Check that the isPinned field of the app has changed.
        expectEquals(OptionalBool.kTrue, metadataView.app_.isPinned);

        // Check that the toggle is now checked.
        expectTrue(toggle.checked);

        // Toggle from true to false.
        toggle.click();
        await fakeHandler.flushPipesForTesting();

        // Check that the isPinned field of the app has changed.
        expectEquals(OptionalBool.kFalse, metadataView.app_.isPinned);

        // Check that the toggle is no longer checked.
        expectFalse(toggle.checked);
      });
});
