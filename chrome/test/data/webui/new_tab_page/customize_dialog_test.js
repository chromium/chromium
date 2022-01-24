// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/lazy_load.js';

import {CustomizeDialogPage, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';

suite('NewTabPageCustomizeDialogTest', () => {
  /** @type {!CustomizeDialogElement} */
  let customizeDialog;

  /** @type {!TestBrowserProxy} */
  let handler;

  setup(() => {
    PolymerTest.clearBody();

    handler = installMock(
        newTabPage.mojom.PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(
            mock, new newTabPage.mojom.PageCallbackRouter()));
    handler.setResultFor('getMostVisitedSettings', Promise.resolve({
      customLinksEnabled: false,
      shortcutsVisible: false,
    }));
    handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [],
    }));

    customizeDialog = document.createElement('ntp-customize-dialog');
    document.body.appendChild(customizeDialog);
    return flushTasks();
  });

  test('creating customize dialog opens cr dialog', () => {
    // Assert.
    assertTrue(customizeDialog.$.dialog.open);
  });

  test('background page selected at start', () => {
    // Assert.
    const shownPages =
        customizeDialog.shadowRoot.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0].getAttribute('page-name'),
        CustomizeDialogPage.BACKGROUNDS);
  });

  test('selecting page shows page', () => {
    // Act.
    customizeDialog.selectedPage = CustomizeDialogPage.MODULES;

    // Assert.
    const shownPages =
        customizeDialog.shadowRoot.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0].getAttribute('page-name'), CustomizeDialogPage.MODULES);
  });

  test('selecting menu item shows page', async () => {
    // Act.
    customizeDialog.$.menu.querySelector('[page-name=themes]').click();
    await flushTasks();

    // Assert.
    const shownPages =
        customizeDialog.shadowRoot.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0].getAttribute('page-name'), CustomizeDialogPage.THEMES);
  });

  suite('scroll borders', () => {
    /**
     * @param {!HTMLElement} container
     * @private
     */
    async function testScrollBorders(container) {
      const assertHidden = el => {
        assertTrue(el.matches('[scroll-border]:not([show])'));
      };
      const assertShown = el => {
        assertTrue(el.matches('[scroll-border][show]'));
      };
      const {firstElementChild: top, lastElementChild: bottom} = container;
      const scrollableElement = top.nextSibling;
      const dialogBody =
          customizeDialog.shadowRoot.querySelector('div[slot=body]');
      const heightWithBorders = `${scrollableElement.scrollHeight + 2}px`;
      dialogBody.style.height = heightWithBorders;
      assertHidden(top);
      assertHidden(bottom);
      dialogBody.style.height = '50px';
      await waitAfterNextRender();
      assertHidden(top);
      assertShown(bottom);
      scrollableElement.scrollTop = 1;
      await waitAfterNextRender();
      assertShown(top);
      assertShown(bottom);
      scrollableElement.scrollTop = scrollableElement.scrollHeight;
      await waitAfterNextRender();
      assertShown(top);
      assertHidden(bottom);
      dialogBody.style.height = heightWithBorders;
      await waitAfterNextRender();
      assertHidden(top);
      assertHidden(bottom);
    }

    // Disabled for flakiness, see https://crbug.com/1066459.
    test.skip('menu', () => testScrollBorders(customizeDialog.$.menuContainer));
    test.skip(
        'pages', () => testScrollBorders(customizeDialog.$.pagesContainer));
  });

  suite('backgrounds', () => {
    setup(() => {
      customizeDialog.theme = {
        dailyRefreshCollectionId: 'landscape',
        backgroundImageUrl: {url: 'https://example.com/image.png'},
      };
    });

    test('daily refresh toggle in sync with theme', () => {
      assertFalse(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {id: 'landscape'};
      assertTrue(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {id: 'abstract'};
      assertFalse(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {id: 'landscape'};
      assertTrue(customizeDialog.$.refreshToggle.checked);
    });

    test('daily refresh toggle set to new value', () => {
      customizeDialog.$.backgrounds.selectedCollection = {id: 'abstract'};
      assertFalse(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.refreshToggle.click();
      assertTrue(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {id: 'landscape'};
      assertEquals(1, handler.getCallCount('setDailyRefreshCollectionId'));
    });

    test('clicking back', () => {
      customizeDialog.$.backgrounds.selectedCollection = {id: 'landscape'};
      customizeDialog.$.pages.scrollTop = 100;
      customizeDialog.shadowRoot.querySelector('.icon-arrow-back').click();
      assertEquals(customizeDialog.$.pages.scrollTop, 0);
    });

    test('clicking cancel', () => {
      customizeDialog.$.backgrounds.selectedCollection = {id: 'landscape'};
      assertTrue(customizeDialog.$.refreshToggle.checked);
      customizeDialog.shadowRoot.querySelector('.cancel-button').click();
      assertEquals(1, handler.getCallCount('revertBackgroundChanges'));
    });

    suite('clicking done', () => {
      function done() {
        customizeDialog.shadowRoot.querySelector('.action-button').click();
      }

      test('sets daily refresh', async () => {
        customizeDialog.$.backgrounds.selectedCollection = {id: 'abstract'};
        customizeDialog.$.refreshToggle.click();
        assertEquals(1, handler.getCallCount('setDailyRefreshCollectionId'));
        done();
        assertEquals(
            'abstract',
            await handler.whenCalled('setDailyRefreshCollectionId'));
      });
    });
  });
});
