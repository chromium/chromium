// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {recipeTasksDescriptor, RecipeTasksHandlerProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';

suite('NewTabPageModulesRecipeTasksModuleTest', () => {
  /**
   * @implements {RecipeTasksHandlerProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  setup(() => {
    PolymerTest.clearBody();

    testProxy = TestBrowserProxy.fromClass(RecipeTasksHandlerProxy);
    testProxy.handler =
        TestBrowserProxy.fromClass(recipeTasks.mojom.RecipeTasksHandlerRemote);
    RecipeTasksHandlerProxy.instance_ = testProxy;
  });

  test('creates no module if no task', async () => {
    // Arrange.
    testProxy.handler.setResultFor(
        'getPrimaryRecipeTask', Promise.resolve({recipeTask: null}));

    // Act.
    await recipeTasksDescriptor.initialize();

    // Assert.
    assertEquals(1, testProxy.handler.getCallCount('getPrimaryRecipeTask'));
    assertEquals(null, recipeTasksDescriptor.element);
  });

  test('creates module if task', async () => {
    // Arrange.
    const recipeTask = {
      title: 'Hello world',
      recipes: [
        {
          name: 'foo',
          imageUrl: {url: 'https://foo.com/img.png'},
          info: 'foo info',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
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
    testProxy.handler.setResultFor(
        'getPrimaryRecipeTask', Promise.resolve({recipeTask}));

    // Act.
    await recipeTasksDescriptor.initialize();
    const module = recipeTasksDescriptor.element;
    document.body.append(module);
    module.$.recipesRepeat.render();
    module.$.relatedSearchesRepeat.render();

    // Assert.
    const recipes = Array.from(module.shadowRoot.querySelectorAll('.recipe'));
    const pills = Array.from(module.shadowRoot.querySelectorAll('.pill'));
    assertEquals(1, testProxy.handler.getCallCount('getPrimaryRecipeTask'));
    assertEquals(2, recipes.length);
    assertEquals(2, pills.length);
    assertEquals('https://foo.com/', recipes[0].href);
    assertEquals(
        'https://foo.com/img.png', recipes[0].querySelector('img').autoSrc);
    assertEquals('foo', recipes[0].querySelector('.name').innerText);
    assertEquals('foo', recipes[0].querySelector('.name').title);
    assertEquals('foo info', recipes[0].querySelector('.info').innerText);
    assertEquals('https://bar.com/', recipes[1].href);
    assertEquals(
        'https://bar.com/img.png', recipes[1].querySelector('img').autoSrc);
    assertEquals('bar', recipes[1].querySelector('.name').innerText);
    assertEquals('bar', recipes[1].querySelector('.name').title);
    assertEquals('bar info', recipes[1].querySelector('.info').innerText);
    assertEquals('https://baz.com/', pills[0].href);
    assertEquals('baz', pills[0].querySelector('.search-text').innerText);
    assertEquals('https://blub.com/', pills[1].href);
    assertEquals('blub', pills[1].querySelector('.search-text').innerText);
  });

  test('recipes and pills are hidden when cutoff', async () => {
    const repeat = (n, fn) => Array(n).fill(0).map(fn);
    testProxy.handler.setResultFor('getPrimaryRecipeTask', Promise.resolve({
      recipeTask: {
        title: 'Hello world',
        recipes: repeat(20, () => ({
                              name: 'foo',
                              imageUrl: {url: 'https://foo.com/img.png'},
                              info: 'foo info',
                              targetUrl: {url: 'https://foo.com'},
                            })),
        relatedSearches: repeat(20, () => ({
                                      text: 'baz',
                                      targetUrl: {url: 'https://baz.com'},
                                    })),
      }
    }));
    await recipeTasksDescriptor.initialize();
    const moduleElement = recipeTasksDescriptor.element;
    document.body.append(moduleElement);
    moduleElement.$.recipesRepeat.render();
    moduleElement.$.relatedSearchesRepeat.render();
    const getElements = () =>
        Array.from(moduleElement.shadowRoot.querySelectorAll('.recipe, .pill'));
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
    const recipeTask = {
      title: 'Continue searching for Hello world',
      name: 'Hello world',
      recipes: [
        {
          name: 'foo',
          imageUrl: {url: 'https://foo.com/img.png'},
          info: 'foo info',
          targetUrl: {url: 'https://foo.com'},
        },
        {
          name: 'bar',
          imageUrl: {url: 'https://bar.com/img.png'},
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
    testProxy.handler.setResultFor(
        'getPrimaryRecipeTask', Promise.resolve({recipeTask}));


    // Act.
    await recipeTasksDescriptor.initialize();

    // Assert.
    assertEquals('function', typeof recipeTasksDescriptor.actions.dismiss);
    assertEquals('function', typeof recipeTasksDescriptor.actions.restore);

    // Act.
    const toastMessage = recipeTasksDescriptor.actions.dismiss();

    // Assert.
    assertEquals('Removed Hello world', toastMessage);
    assertEquals(
        'Hello world', await testProxy.handler.whenCalled('dismissRecipeTask'));

    // Act.
    recipeTasksDescriptor.actions.restore();

    // Assert.
    assertEquals(
        'Hello world', await testProxy.handler.whenCalled('restoreRecipeTask'));
  });
});
