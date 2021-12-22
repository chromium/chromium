// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, recipeTasksV2Descriptor, TaskModuleHandlerProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {assertEquals, assertTrue} from 'chrome://test/chai_assert.js';
import {installMock} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.js';

suite('NewTabPageModulesRecipesV2ModuleTest', () => {
  /** @type {!TestBrowserProxy} */
  let handler;

  setup(() => {
    document.body.innerHTML = '';

    handler = installMock(
        taskModule.mojom.TaskModuleHandlerRemote,
        TaskModuleHandlerProxy.setHandler);
  });

  test('module appears on render with recipes', async () => {
    // Arrange.
    const task = {
      title: 'First Recipes',
      taskItems: [
        {
          name: 'apricot',
          imageUrl: {url: 'https://apricot.com/img.png'},
          info: 'Viewed 6 months ago',
          siteName: 'Apricot Site',
        },
        {
          name: 'banana',
          imageUrl: {url: 'https://banana.com/img.png'},
          info: 'Viewed 2 days ago',
          siteName: 'Banana Site',
        },
        {
          name: 'cranberry',
          imageUrl: {url: 'https://cranberry.com/img.png'},
          info: 'Viewed 3 weeks ago',
          siteName: 'Cranberry Site',
        },
      ],
    };
    handler.setResultFor('getPrimaryTask', Promise.resolve({task}));

    // Act.
    const moduleElement = assert(await recipeTasksV2Descriptor.initialize(0));
    document.body.append(moduleElement);
    $$(moduleElement, '#recipesRepeat').render();

    // Assert.
    const recipes =
        Array.from(moduleElement.shadowRoot.querySelectorAll('.recipe-item'));
    assertEquals(1, handler.getCallCount('getPrimaryTask'));
    assertEquals(3, recipes.length);
    assertEquals(
        'https://apricot.com/img.png', recipes[0].querySelector('img').autoSrc);
    assertEquals('apricot', recipes[0].querySelector('.name').innerText);
    assertEquals('apricot', recipes[0].querySelector('.name').title);
    assertEquals(
        'Viewed 6 months ago', recipes[0].querySelector('.info').innerText);
    assertEquals(
        'Apricot Site', recipes[0].querySelector('.site-name').innerText);
    assertEquals(
        'https://banana.com/img.png', recipes[1].querySelector('img').autoSrc);
    assertEquals('banana', recipes[1].querySelector('.name').innerText);
    assertEquals('banana', recipes[1].querySelector('.name').title);
    assertEquals(
        'Viewed 2 days ago', recipes[1].querySelector('.info').innerText);
    assertEquals(
        'Banana Site', recipes[1].querySelector('.site-name').innerText);
    assertEquals(
        'https://cranberry.com/img.png',
        recipes[2].querySelector('img').autoSrc);
    assertEquals('cranberry', recipes[2].querySelector('.name').innerText);
    assertEquals('cranberry', recipes[2].querySelector('.name').title);
    assertEquals(
        'Viewed 3 weeks ago', recipes[2].querySelector('.info').innerText);
    assertEquals(
        'Cranberry Site', recipes[2].querySelector('.site-name').innerText);
  });

  test('empty module renders if no tasks available', async () => {
    // Arrange.
    handler.setResultFor('getPrimaryTask', Promise.resolve({task: null}));

    // Act.
    const moduleElement = await recipeTasksV2Descriptor.initialize(0);

    // Assert.
    assertTrue(!!moduleElement);
  });
});
