// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BackgroundSelectionType, BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks, isVisible, waitAfterNextRender} from 'chrome://test/test_util.m.js';

suite('NewTabPageCustomizeDialogTest', () => {
  /** @type {!CustomizeDialogElement} */
  let customizeDialog;

  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    testProxy.handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    testProxy.handler.setResultFor('getBackgroundImages', Promise.resolve({
      images: [],
    }));
    BrowserProxy.instance_ = testProxy;

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
    assertEquals(shownPages[0].getAttribute('page-name'), 'backgrounds');
  });

  test('selecting menu item shows page', async () => {
    // Act.
    customizeDialog.$.menu.querySelector('[page-name=themes]').click();
    await flushTasks();

    // Assert.
    const shownPages =
        customizeDialog.shadowRoot.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(shownPages[0].getAttribute('page-name'), 'themes');
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
      assertFalse(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {id: 'abstract'};
      assertTrue(customizeDialog.$.refreshToggle.checked);
    });

    test('selecting image clears toggle', () => {
      customizeDialog.$.backgrounds.selectedCollection = {id: 'landscape'};
      assertTrue(customizeDialog.$.refreshToggle.checked);
      customizeDialog.backgroundSelection = {
        type: BackgroundSelectionType.IMAGE,
        image: {imageUrl: {url: 'https://example.com/other.png'}},
      };
      assertFalse(customizeDialog.$.refreshToggle.checked);
    });

    test('clicking cancel', () => {
      customizeDialog.backgroundSelection = {
        type: BackgroundSelectionType.IMAGE,
        image: {
          attribution1: '1',
          attribution2: '2',
          attributionUrl: {url: 'https://example.com'},
          imageUrl: {url: 'https://example.com/other.png'}
        },
      };
      customizeDialog.shadowRoot.querySelector('.cancel-button').click();
      assertDeepEquals(
          {type: BackgroundSelectionType.NO_SELECTION},
          customizeDialog.backgroundSelection);
    });

    suite('clicking done', () => {
      function done() {
        customizeDialog.shadowRoot.querySelector('.action-button').click();
      }

      test('applies selected image', async () => {
        customizeDialog.backgroundSelection = {
          type: BackgroundSelectionType.IMAGE,
          image: {
            attribution1: '1',
            attribution2: '2',
            attributionUrl: {url: 'https://example.com'},
            imageUrl: {url: 'https://example.com/other.png'}
          },
        };
        done();
        const [attribution1, attribution2, attributionUrl, imageUrl] =
            await testProxy.handler.whenCalled('setBackgroundImage');
        assertEquals('1', attribution1);
        assertEquals('2', attribution2);
        assertEquals('https://example.com', attributionUrl.url);
        assertEquals('https://example.com/other.png', imageUrl.url);
        assertDeepEquals(
            {
              type: BackgroundSelectionType.IMAGE,
              image: {
                attribution1: '1',
                attribution2: '2',
                attributionUrl: {url: 'https://example.com'},
                imageUrl: {url: 'https://example.com/other.png'}
              },
            },
            customizeDialog.backgroundSelection);
      });

      test('sets daily refresh', async () => {
        customizeDialog.$.backgrounds.selectedCollection = {id: 'abstract'};
        customizeDialog.$.refreshToggle.click();
        done();
        assertEquals(
            'abstract',
            await testProxy.handler.whenCalled('setDailyRefreshCollectionId'));
        assertDeepEquals(
            {
              type: BackgroundSelectionType.DAILY_REFRESH,
              dailyRefreshCollectionId: 'abstract',
            },
            customizeDialog.backgroundSelection);
      });

      test('clears daily refresh', async () => {
        customizeDialog.$.backgrounds.selectedCollection = {id: 'landscape'};
        customizeDialog.$.refreshToggle.click();
        done();
        await testProxy.handler.whenCalled('setNoBackgroundImage');
      });

      test('set no background', async () => {
        customizeDialog.backgroundSelection = {
          type: BackgroundSelectionType.NO_BACKGROUND,
        };
        done();
        await testProxy.handler.whenCalled('setNoBackgroundImage');
        assertDeepEquals(
            {type: BackgroundSelectionType.NO_BACKGROUND},
            customizeDialog.backgroundSelection);
      });
    });
  });
});
