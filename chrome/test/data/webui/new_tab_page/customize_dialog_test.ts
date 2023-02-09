// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/lazy_load.js';

import {CustomizeDialogElement} from 'chrome://new-tab-page/lazy_load.js';
import {CustomizeDialogPage, NewTabPageProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {createBackgroundImage, createTheme, installMock} from './test_support.js';

suite('NewTabPageCustomizeDialogTest', () => {
  let customizeDialog: CustomizeDialogElement;
  let handler: TestMock<PageHandlerRemote>;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    handler = installMock(
        PageHandlerRemote,
        (mock: PageHandlerRemote) =>
            NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
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
    handler.setResultFor('getModulesIdNames', Promise.resolve({
      data: [],
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
        customizeDialog.shadowRoot!.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('page-name'),
        CustomizeDialogPage.BACKGROUNDS);
  });

  test('selecting page shows page', () => {
    // Act.
    customizeDialog.selectedPage = CustomizeDialogPage.MODULES;

    // Assert.
    const shownPages =
        customizeDialog.shadowRoot!.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('page-name'), CustomizeDialogPage.MODULES);
  });

  test('selecting menu item shows page', async () => {
    // Act.
    customizeDialog.$.menu.querySelector<HTMLElement>(
                              '[page-name=themes]')!.click();
    await flushTasks();

    // Assert.
    const shownPages =
        customizeDialog.shadowRoot!.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(
        shownPages[0]!.getAttribute('page-name'), CustomizeDialogPage.THEMES);
  });

  suite('scroll borders', () => {
    async function testScrollBorders(container: HTMLElement) {
      function assertHidden(el: HTMLElement) {
        assertTrue(el.matches('[scroll-border]:not([show])'));
      }

      function assertShown(el: HTMLElement) {
        assertTrue(el.matches('[scroll-border][show]'));
      }

      const top = container.firstElementChild as HTMLElement;
      const bottom = container.lastElementChild as HTMLElement;
      const scrollableElement = top.nextSibling as HTMLElement;
      const dialogBody = customizeDialog.shadowRoot!.querySelector<HTMLElement>(
          'div[slot=body]')!;
      const heightWithBorders = `${scrollableElement.scrollHeight + 2}px`;
      dialogBody.style.height = heightWithBorders;
      assertHidden(top);
      assertHidden(bottom);
      dialogBody.style.height = '50px';
      await waitAfterNextRender(container);
      assertHidden(top);
      assertShown(bottom);
      scrollableElement.scrollTop = 1;
      await waitAfterNextRender(container);
      assertShown(top);
      assertShown(bottom);
      scrollableElement.scrollTop = scrollableElement.scrollHeight;
      await waitAfterNextRender(container);
      assertShown(top);
      assertHidden(bottom);
      dialogBody.style.height = heightWithBorders;
      await waitAfterNextRender(container);
      assertHidden(top);
      assertHidden(bottom);
    }

    // Disabled for flakiness, see https://crbug.com/1066459.
    test.skip('menu', () => testScrollBorders(customizeDialog.$.menu));
    test.skip('pages', () => testScrollBorders(customizeDialog.$.pages));
  });

  suite('backgrounds', () => {
    setup(() => {
      const theme = createTheme();
      theme.dailyRefreshEnabled = true;
      theme.backgroundImageCollectionId = 'landscape';
      theme.backgroundImage =
          createBackgroundImage('https://example.com/image.png');
      customizeDialog.theme = theme;
    });

    test('daily refresh toggle in sync with theme', () => {
      assertFalse(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {
        id: 'landscape',
        label: '',
        previewImageUrl: {url: ''},
      };
      assertTrue(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {
        id: 'abstract',
        label: '',
        previewImageUrl: {url: ''},
      };
      assertFalse(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {
        id: 'landscape',
        label: '',
        previewImageUrl: {url: ''},
      };
      assertTrue(customizeDialog.$.refreshToggle.checked);
    });

    test('daily refresh toggle set to new value', () => {
      customizeDialog.$.backgrounds.selectedCollection = {
        id: 'abstract',
        label: '',
        previewImageUrl: {url: ''},
      };
      assertFalse(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.refreshToggle.click();
      assertTrue(customizeDialog.$.refreshToggle.checked);
      customizeDialog.$.backgrounds.selectedCollection = {
        id: 'landscape',
        label: '',
        previewImageUrl: {url: ''},
      };
      assertEquals(1, handler.getCallCount('setDailyRefreshCollectionId'));
    });

    test('clicking back', () => {
      customizeDialog.$.backgrounds.selectedCollection = {
        id: 'landscape',
        label: '',
        previewImageUrl: {url: ''},
      };
      customizeDialog.$.pages.scrollTop = 100;
      customizeDialog.shadowRoot!
          .querySelector<HTMLElement>('.icon-arrow-back')!.click();
      assertEquals(customizeDialog.$.pages.scrollTop, 0);
    });

    test('clicking cancel', () => {
      customizeDialog.$.backgrounds.selectedCollection = {
        id: 'landscape',
        label: '',
        previewImageUrl: {url: ''},
      };
      assertTrue(customizeDialog.$.refreshToggle.checked);
      customizeDialog.shadowRoot!.querySelector<HTMLElement>(
                                     '.cancel-button')!.click();
      assertEquals(1, handler.getCallCount('revertBackgroundChanges'));
    });

    suite('clicking done', () => {
      function done() {
        customizeDialog.shadowRoot!
            .querySelector<HTMLElement>('.action-button')!.click();
      }

      test('sets daily refresh', async () => {
        customizeDialog.$.backgrounds.selectedCollection = {
          id: 'abstract',
          label: '',
          previewImageUrl: {url: ''},
        };
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
