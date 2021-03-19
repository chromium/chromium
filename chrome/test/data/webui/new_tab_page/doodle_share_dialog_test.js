// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';

suite('NewTabPageDoodleShareDialogTest', () => {
  /** @type {!DoodleShareDialogElement} */
  let doodleShareDialog;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.setInstance(testProxy);

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
      const openedUrl = await testProxy.whenCalled('open');
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
    const navigateUrl = await testProxy.whenCalled('navigate');
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
