// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/app.js';

import {CrRouter} from 'chrome://resources/js/cr_router.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {SkillsAppElement} from 'chrome://skills/app.js';
import {SkillsPageBrowserProxy} from 'chrome://skills/skills_page_browser_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSkillsBrowserProxy} from './test_skills_browser_proxy.js';

suite('SkillsAppPage', function() {
  let app: SkillsAppElement;
  let browserProxy: TestSkillsBrowserProxy;

  setup(function() {
    browserProxy = new TestSkillsBrowserProxy();
    SkillsPageBrowserProxy.setInstance(browserProxy);
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
    assertEquals(loadTimeData.getString('skillsTitle'), app.$.toolbar.pageName);

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
    assertEquals('/yourSkills', CrRouter.getInstance().getPath());

    tabs[1]!.click();
    await microtasksFinished();
    assertEquals('/browse', CrRouter.getInstance().getPath());
  });

  test('UndefinedRouteNavigatesToUserSkills', async function() {
    navigateTo('/test');
    await microtasksFinished();
    const selectedTab =
        app.$.menu.shadowRoot.querySelector('.cr-nav-menu-item[selected]');
    assertEquals('chrome://skills/yourSkills', window.location.href);
    assertEquals(
        loadTimeData.getString('userSkillsTitle'),
        selectedTab!.querySelector('.name')!.textContent.trim());
  });

  test('DiscoverSkillsPageLoadsCorrectly', async function() {
    navigateTo('/browse');
    await eventToPromise('iron-select', app.$.menu);
    assertEquals('chrome://skills/browse', window.location.href);
    await microtasksFinished();
    const selectedTab =
        app.$.menu.shadowRoot.querySelector('.cr-nav-menu-item[selected]');
    assertEquals(
        loadTimeData.getString('browseSkillsTitle'),
        selectedTab!.querySelector('.name')!.textContent.trim());
  });

  test('UserSkillsPageLoadsCorrectly', async function() {
    navigateTo('/yourSkills');
    await microtasksFinished();
    assertEquals('chrome://skills/yourSkills', window.location.href);
    await microtasksFinished();
    const selectedTab =
        app.$.menu.shadowRoot.querySelector('.cr-nav-menu-item[selected]');
    assertEquals(
        loadTimeData.getString('userSkillsTitle'),
        selectedTab!.querySelector('.name')!.textContent.trim());
  });

  test('BrowseSkillsButtonNavigatesToDiscoverSkills', async function() {
    navigateTo('/yourSkills');
    await microtasksFinished();
    const button = app.$.userSkillsPage.$['browseSkillsButton'];
    assertTrue(!!button);
    (button as HTMLElement).click();
    await microtasksFinished();
    assertEquals('/browse', CrRouter.getInstance().getPath());
    const selectedTab =
        app.$.menu.shadowRoot.querySelector('.cr-nav-menu-item[selected]');
    assertEquals(
        loadTimeData.getString('browseSkillsTitle'),
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

  test('BackNavigationWorksAfterMultipleTabClicks', async function() {
    navigateTo('/yourSkills');
    await microtasksFinished();

    const tabs = app.$.menu.shadowRoot.querySelectorAll<HTMLElement>(
        '.cr-nav-menu-item');
    const userSkillsTab = tabs[0]!;
    const discoverSkillsTab = tabs[1]!;

    discoverSkillsTab.click();
    await microtasksFinished();
    assertEquals('/browse', CrRouter.getInstance().getPath());
    userSkillsTab.click();
    await microtasksFinished();
    assertEquals('/yourSkills', CrRouter.getInstance().getPath());
    discoverSkillsTab.click();
    await microtasksFinished();
    assertEquals('/browse', CrRouter.getInstance().getPath());

    const backPromise = eventToPromise('popstate', window);
    window.history.back();
    await backPromise;
    await microtasksFinished();
    assertEquals('/yourSkills', CrRouter.getInstance().getPath());
    const backPromise2 = eventToPromise('popstate', window);
    window.history.back();
    await backPromise2;
    await microtasksFinished();
    assertEquals('/browse', CrRouter.getInstance().getPath());
  });

  test('Request1PSkillsOnDiscoverSkillsNavigation', async function() {
    navigateTo('/browse');
    await browserProxy.handler.whenCalled('request1PSkills');
  });
});
