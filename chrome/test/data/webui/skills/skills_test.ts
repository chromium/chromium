// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/app.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import type {SkillsAppElement} from 'chrome://skills/app.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('SkillsAppPage', function() {
  let app: SkillsAppElement;

  setup(function() {
    window.history.replaceState({}, '', '/');
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    CrRouter.resetForTesting();
    app = document.createElement('skills-app');
    document.body.appendChild(app);
    return microtasksFinished();
  });

  function navigateTo(route: string) {
    window.history.replaceState({}, '', route);
    window.dispatchEvent(new CustomEvent('popstate'));
  }

  test('InitialPageLoadsCorrectly', function() {
    assertEquals('Skills', app.$.toolbar.pageName);

    const tabs = app.$.menu.shadowRoot.querySelectorAll<HTMLElement>(
        '.cr-nav-menu-item');
    assertTrue(!!tabs);
    assertEquals(2, tabs.length);
  });

  test('SkillMenuTabsNavigateCorrectly', async function() {
    const tabs = app.$.menu.shadowRoot.querySelectorAll<HTMLElement>(
        '.cr-nav-menu-item');
    assertTrue(!!tabs);

    tabs[0]!.click();
    await microtasksFinished();
    assertEquals('/user-skills', CrRouter.getInstance().getPath());

    tabs[1]!.click();
    await microtasksFinished();
    assertEquals('/discover-skills', CrRouter.getInstance().getPath());
  });

  test('UndefinedRouteNavigatesToUserSkills', async function() {
    navigateTo('/test');
    await microtasksFinished();
    const selectedTab =
        app.$.menu.shadowRoot.querySelector('.cr-nav-menu-item[selected]');
    assertEquals('chrome://skills/user-skills', window.location.href);
    assertEquals(
        'Your skills', selectedTab!.querySelector('.name')!.textContent.trim());
  });

  test('DiscoverSkillsPageLoadsCorrectly', async function() {
    navigateTo('/discover-skills');
    await eventToPromise('iron-select', app.$.menu);
    assertEquals('chrome://skills/discover-skills', window.location.href);
    await microtasksFinished();
    const selectedTab =
        app.$.menu.shadowRoot.querySelector('.cr-nav-menu-item[selected]');
    assertEquals(
        'Discover skills',
        selectedTab!.querySelector('.name')!.textContent.trim());
  });

  test('UserSkillsPageLoadsCorrectly', async function() {
    navigateTo('/user-skills');
    await microtasksFinished();
    assertEquals('chrome://skills/user-skills', window.location.href);
    await microtasksFinished();
    const selectedTab =
        app.$.menu.shadowRoot.querySelector('.cr-nav-menu-item[selected]');
    assertEquals(
        'Your skills', selectedTab!.querySelector('.name')!.textContent.trim());
  });

  test('BrowseSkillsButtonNavigatesToDiscoverSkills', async function() {
    navigateTo('/user-skills');
    await microtasksFinished();
    const button = app.$.userSkillsPage.shadowRoot.querySelector<HTMLElement>(
        '#browse-skills-button');
    assertTrue(!!button);
    button.click();
    await microtasksFinished();
    assertEquals('/discover-skills', CrRouter.getInstance().getPath());
    const selectedTab =
        app.$.menu.shadowRoot.querySelector('.cr-nav-menu-item[selected]');
    assertEquals(
        'Discover skills',
        selectedTab!.querySelector('.name')!.textContent.trim());
  });

  test('NarrowModeHidesSidebarAndShowsDrawer', async function() {
    app.$.toolbar.narrow = true;
    await microtasksFinished();
    assertTrue(app.$.toolbar.showMenu);
    assertFalse(app.$.drawer.open);
    assertTrue(app.$.menu.parentElement!.hidden);

    app.$.toolbar.dispatchEvent(new CustomEvent('cr-toolbar-menu-click'));
    await microtasksFinished();
    assertTrue(app.$.drawer.open);
  });
});
