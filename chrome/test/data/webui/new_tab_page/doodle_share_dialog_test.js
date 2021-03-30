// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';

suite('NewTabPageDoodleShareDialogTest', () => {
  /** @type {!DoodleShareDialogElement} */
  let doodleShareDialog;

  /**
   * @implements {WindowProxy}
   * @extends {TestBrowserProxy}
   */
  let windowProxy;

  setup(() => {
    PolymerTest.clearBody();

    windowProxy = TestBrowserProxy.fromClass(WindowProxy);
    WindowProxy.setInstance(windowProxy);

    doodleShareDialog = document.createElement('ntp-doodle-share-dialog');
    document.body.appendChild(doodleShareDialog);
  });

  test('creating doodle share dialog opens cr dialog', () => {
    // Assert.
    assertTrue(doodleShareDialog.$.dialog.open);
  });

  test('setting title, url shows title, url', () => {
    // Act.
    doodleShareDialog.title = 'foo';
    doodleShareDialog.url = {url: 'https://bar.com'};

    // Assert.
    assertEquals(doodleShareDialog.$.title.innerText, 'foo');
    assertEquals(doodleShareDialog.$.url.value, 'https://bar.com');
  });

  const testParams = [
    {
      label: 'Facebook',
      buttonId: 'facebookButton',
      url: 'https://www.facebook.com/dialog/share' +
          '?app_id=738026486351791' +
          `&href=${encodeURIComponent('https://bar.com')}` +
          `&hashtag=${encodeURIComponent('#GoogleDoodle')}`,
    },
    {
      label: 'Twitter',
      buttonId: 'twitterButton',
      url: 'https://twitter.com/intent/tweet' +
          `?text=${encodeURIComponent('foo\nhttps://bar.com')}`,
    },
  ];

  testParams.forEach(({label, buttonId, url}) => {
    test(`clicking ${label} opens ${label}`, async () => {
      // Arrange.
      doodleShareDialog.title = 'foo';
      doodleShareDialog.url = {url: 'https://bar.com'};

      // Act.
      doodleShareDialog.$[buttonId].click();

      // Assert.
      const openedUrl = await windowProxy.whenCalled('open');
      assertEquals(openedUrl, url);
    });
  });

  test(`clicking email navigates to email`, async () => {
    // Arrange.
    doodleShareDialog.title = 'foo';
    doodleShareDialog.url = {url: 'https://bar.com'};

    // Act.
    doodleShareDialog.$.emailButton.click();

    // Assert.
    const navigateUrl = await windowProxy.whenCalled('navigate');
    assertEquals(
        navigateUrl,
        `mailto:?subject=foo&body=${encodeURIComponent('https://bar.com')}`);
  });

  test('clicking done closes dialog', async () => {
    // Act.
    doodleShareDialog.$.doneButton.click();

    // Assert.
    assertFalse(doodleShareDialog.$.dialog.open);
  });
});
