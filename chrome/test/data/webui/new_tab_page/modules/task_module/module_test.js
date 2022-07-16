// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, shoppingTasksDescriptor, TaskModuleHandlerProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://test/chai_assert.js';
import {installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.js';

suite('NewTabPageModulesTaskModuleTest', () => {
  /** @type {!TestBrowserProxy} */
  let handler;

  setup(() => {
    document.body.innerHTML = '';

    handler = installMock(
        taskModule.mojom.TaskModuleHandlerRemote,
        TaskModuleHandlerProxy.setHandler);
  });

  test('creates no module if no task', async () => {
    // Arrange.
    handler.setResultFor('getPrimaryTask', Promise.resolve({task: null}));

    // Act.
    const moduleElement = await shoppingTasksDescriptor.initialize(0);

    // Assert.
    assertEquals(1, handler.getCallCount('getPrimaryTask'));
    assertEquals(null, moduleElement);
  });

  test('creates module if task', async () => {
    // Arrange.
    const task = {
      title: 'Hello world',
      taskItems: [
        {
          name: 'foo',
          imageUrl: {url: 'https://foo.com/img.png'},
          price: '1 gazillion dollars',
          info: 'foo info',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
          price: '2 gazillion dollars',
          info: 'bar info',
          targetUrl: {url: 'https://bar.com'},
        },
      ],
      relatedSearches: [
        {
          text: 'baz',
          targetUrl: {url: 'https://baz.com'},
        },
        {
          text: 'blub',
          targetUrl: {url: 'https://blub.com'},
        },
      ],
    };
    handler.setResultFor('getPrimaryTask', Promise.resolve({task}));

    // Act.
    const moduleElement = assert(await shoppingTasksDescriptor.initialize(0));
    document.body.append(moduleElement);
    $$(moduleElement, '#taskItemsRepeat').render();
    $$(moduleElement, '#relatedSearchesRepeat').render();

    // Assert.
    const products =
        Array.from(moduleElement.shadowRoot.querySelectorAll('.task-item'));
    const pills =
        Array.from(moduleElement.shadowRoot.querySelectorAll('.pill'));
    assertEquals(1, handler.getCallCount('getPrimaryTask'));
    assertEquals(2, products.length);
    assertEquals(2, pills.length);
    assertEquals('https://foo.com/', products[0].href);
    assertEquals(
        'https://foo.com/img.png', products[0].querySelector('img').autoSrc);
    assertEquals(
        '1 gazillion dollars', products[0].querySelector('.price').innerText);
    assertEquals('foo', products[0].querySelector('.name').innerText);
    assertEquals('foo', products[0].querySelector('.name').title);
    assertEquals('foo info', products[0].querySelector('.secondary').innerText);
    assertEquals('https://bar.com/', products[1].href);
    assertEquals(
        'https://bar.com/img.png', products[1].querySelector('img').autoSrc);
    assertEquals(
        '2 gazillion dollars', products[1].querySelector('.price').innerText);
    assertEquals('bar', products[1].querySelector('.name').innerText);
    assertEquals('bar', products[1].querySelector('.name').title);
    assertEquals('bar info', products[1].querySelector('.secondary').innerText);
    assertEquals('https://baz.com/', pills[0].href);
    assertEquals('baz', pills[0].querySelector('.search-text').innerText);
    assertEquals('https://blub.com/', pills[1].href);
    assertEquals('blub', pills[1].querySelector('.search-text').innerText);
  });

  test('products and pills are hidden when cutoff', async () => {
    const repeat = (n, fn) => Array(n).fill(0).map(fn);
    handler.setResultFor('getPrimaryTask', Promise.resolve({
      task: {
        title: 'Hello world',
        taskItems: repeat(20, () => ({
                                name: 'foo',
                                imageUrl: {url: 'https://foo.com/img.png'},
                                price: '1 gazillion dollars',
                                info: 'foo info',
                                targetUrl: {url: 'https://foo.com'},
                              })),
        relatedSearches: repeat(20, () => ({
                                      text: 'baz',
                                      targetUrl: {url: 'https://baz.com'},
                                    })),
      }
    }));
    const moduleElement = assert(await shoppingTasksDescriptor.initialize(0));
    document.body.append(moduleElement);
    $$(moduleElement, '#taskItemsRepeat').render();
    $$(moduleElement, '#relatedSearchesRepeat').render();
    const getElements = () => Array.from(
        moduleElement.shadowRoot.querySelectorAll('.task-item, .pill'));
    assertEquals(40, getElements().length);
    const hiddenCount = () =>
        getElements().filter(el => el.style.visibility === 'hidden').length;
    const checkHidden = async (width, count) => {
      const waitForVisibilityUpdate =
          eventToPromise('visibility-update', moduleElement);
      moduleElement.style.width = width;
      await waitForVisibilityUpdate;
      assertEquals(count, hiddenCount());
    };
    await checkHidden('500px', 31);
    await checkHidden('300px', 35);
    await checkHidden('700px', 26);
    await checkHidden('500px', 31);
  });

  test('Backend is notified when module is dismissed or restored', async () => {
    // Arrange.
    const task = {
      title: 'Continue searching for Hello world',
      name: 'Hello world',
      taskItems: [
        {
          name: 'foo',
          imageUrl: {url: 'https://foo.com/img.png'},
          price: '1 gazillion dollars',
          info: 'foo info',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
          price: '2 gazillion dollars',
          info: 'bar info',
          targetUrl: {url: 'https://bar.com'},
        },
      ],
      relatedSearches: [
        {
          text: 'baz',
          targetUrl: {url: 'https://baz.com'},
        },
        {
          text: 'blub',
          targetUrl: {url: 'https://blub.com'},
        },
      ],
    };
    handler.setResultFor('getPrimaryTask', Promise.resolve({task}));

    // Arrange.
    const moduleElement = assert(await shoppingTasksDescriptor.initialize(0));
    document.body.append(moduleElement);
    await flushTasks();

    // Act.
    const waitForDismissEvent = eventToPromise('dismiss-module', moduleElement);
    const dismissButton =
        moduleElement.shadowRoot.querySelector('ntp-module-header')
            .shadowRoot.querySelector('#dismissButton');
    dismissButton.click();
    const dismissEvent = await waitForDismissEvent;
    const toastMessage = dismissEvent.detail.message;
    const restoreCallback = dismissEvent.detail.restoreCallback;

    // Assert.
    assertEquals('Hello world hidden', toastMessage);
    assertDeepEquals(
        [taskModule.mojom.TaskModuleType.kShopping, 'Hello world'],
        await handler.whenCalled('dismissTask'));

    // Act.
    restoreCallback();

    // Assert.
    assertDeepEquals(
        [taskModule.mojom.TaskModuleType.kShopping, 'Hello world'],
        await handler.whenCalled('restoreTask'));
  });

  test('info button click opens info dialog', async () => {
    // Arrange.
    const task = {
      title: '',
      taskItems: [],
      relatedSearches: [],
    };
    handler.setResultFor('getPrimaryTask', Promise.resolve({task}));
    const moduleElement = assert(await shoppingTasksDescriptor.initialize(0));
    document.body.append(moduleElement);

    // Act.
    $$(moduleElement, 'ntp-module-header')
        .dispatchEvent(new Event('info-button-click'));

    // Assert.
    assertTrue(!!$$(moduleElement, 'ntp-info-dialog'));
  });
});
