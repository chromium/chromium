// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/topics/topic_details.js';
import 'chrome://context-hub/topics/topics_view.js';

import {browserProxyFactory, PageHandlerRemote} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {Topic, TopicVisit} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {TopicCardElement} from 'chrome://context-hub/topics/topic_card.js';
import type {TopicDetailsElement} from 'chrome://context-hub/topics/topic_details.js';
import {getSuggestedPrompts, TOPIC_DETAILS_TABS} from 'chrome://context-hub/topics/topic_details.js';
import type {TopicSitesDialogElement} from 'chrome://context-hub/topics/topic_sites_dialog.js';
import {BADGE_BACKGROUND_COLORS, DEFAULT_ICON, getBackgroundColorForTopic, getBadgePath, getBadgeShapeForTopic, getDisplayDomain, getOpenableUrls, getTopicSites, MAX_TOPIC_SITES, toTopicItem} from 'chrome://context-hub/topics/topic_utils.js';
import type {BadgeShape} from 'chrome://context-hub/topics/topic_utils.js';
import type {TopicsViewElement} from 'chrome://context-hub/topics/topics_view.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// U+1F4C1 FILE FOLDER.
const EMOJI = '\u{1F4C1}';

function createTopic(overrides: Partial<Topic> = {}): Topic {
  return {
    id: 'topic-1',
    title: 'Topic title',
    creationTime: {internalValue: 0n},
    emoji: EMOJI,
    overview: 'Long overview.',
    shortOverview: 'Short overview.',
    visits: [
      {url: 'https://example.com/page-1', title: 'Page 1'},
      {url: 'chrome://settings', title: 'Settings'},
      {url: 'http://example.com/page-2', title: 'Page 2'},
    ],
    continuationQueries: [
      {title: 'Query title', prompt: 'Query prompt'},
    ],
    ...overrides,
  };
}

suite('TopicUtils', () => {
  test('toTopicItem maps fields', () => {
    const item = toTopicItem(createTopic());
    assertEquals('topic-1', item.id);
    assertEquals('Topic title', item.title);
    assertEquals('Short overview.', item.description);
    assertEquals('Long overview.', item.longDescription);
    assertEquals(EMOJI, item.icon);
    assertEquals(3, item.visits.length);
    assertEquals('Page 1', item.visits[0]!.title);
    assertEquals(1, item.continuationQueries.length);
  });

  test(
      'toTopicItem falls back between overviews and to the default icon',
      () => {
        const longOnly =
            toTopicItem(createTopic({shortOverview: null, emoji: null}));
        assertEquals('Long overview.', longOnly.description);
        assertEquals('Long overview.', longOnly.longDescription);
        assertEquals(DEFAULT_ICON, longOnly.icon);

        const shortOnly = toTopicItem(createTopic({overview: null}));
        assertEquals('Short overview.', shortOnly.description);
        assertEquals('Short overview.', shortOnly.longDescription);

        const neither =
            toTopicItem(createTopic({overview: null, shortOverview: null}));
        assertEquals('', neither.description);
        assertEquals('', neither.longDescription);
      });

  test('badge is derived from the topic id only', () => {
    assertEquals(getBadgeShapeForTopic('abc'), getBadgeShapeForTopic('abc'));
    assertEquals(
        getBackgroundColorForTopic('abc'), getBackgroundColorForTopic('abc'));
    assertTrue(
        BADGE_BACKGROUND_COLORS.includes(getBackgroundColorForTopic('abc')));

    // Across enough ids, every shape is used.
    const shapes = new Set<BadgeShape>();
    for (let i = 0; i < 100; i++) {
      shapes.add(getBadgeShapeForTopic(`topic-${i}`));
    }
    assertEquals(4, shapes.size);
    for (const shape of shapes) {
      assertTrue(getBadgePath(shape).startsWith('M'));
    }
  });

  test('getOpenableUrls only returns web URLs', () => {
    const item = toTopicItem(createTopic({
      visits: [
        {url: 'https://a.com/', title: ''},
        {url: 'file:///etc/passwd', title: ''},
        {url: 'javascript:alert(1)', title: ''},
        {url: 'http://b.com/', title: ''},
        {url: '', title: ''},
      ],
    }));
    assertDeepEquals(
        ['https://a.com/', 'http://b.com/'], getOpenableUrls(item));
  });

  test('getTopicSites keeps web URLs once each, capped', () => {
    const item = toTopicItem(createTopic({
      visits: [
        {url: 'https://a.com/', title: 'A'},
        {url: 'chrome://history', title: 'History'},
        {url: 'https://a.com/', title: 'A again'},
        {url: 'http://b.com/', title: 'B'},
      ],
    }));
    assertDeepEquals(['A', 'B'], getTopicSites(item).map(site => site.title));

    const visits: TopicVisit[] = [];
    for (let i = 0; i < MAX_TOPIC_SITES + 5; i++) {
      visits.push({url: `https://site${i}.com/`, title: `Site ${i}`});
    }
    const sites = getTopicSites(toTopicItem(createTopic({visits})));
    assertEquals(MAX_TOPIC_SITES, sites.length);
    assertEquals('Site 0', sites[0]!.title);
  });

  test('getDisplayDomain drops www and handles bad URLs', () => {
    assertEquals('example.com', getDisplayDomain('https://www.example.com/a'));
    assertEquals(
        'news.example.com', getDisplayDomain('http://news.example.com'));
    assertEquals('', getDisplayDomain('not a url'));
  });

  test('getSuggestedPrompts prefers titles, drops empties and caps', () => {
    const item = toTopicItem(createTopic({
      continuationQueries: [
        {title: ' Title 1 ', prompt: 'Prompt 1'},
        {title: '', prompt: 'Prompt 2'},
        {title: ' ', prompt: ' '},
        {title: 'Title 3', prompt: ''},
        {title: 'Title 4', prompt: ''},
      ],
    }));
    assertDeepEquals(
        ['Title 1', 'Prompt 2', 'Title 3'], getSuggestedPrompts(item));
  });
});

suite('TopicsView', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let view: TopicsViewElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({kTopics: true});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
  });

  async function createView() {
    view = document.createElement('topics-view');
    document.body.appendChild(view);
    await microtasksFinished();
  }

  function query(selector: string): HTMLElement|null {
    return view.shadowRoot.querySelector<HTMLElement>(selector);
  }

  test('renders nothing until topics load, then the cards', async () => {
    const resolver = new PromiseResolver<{topics: Topic[]}>();
    handler.setResultFor('getTopics', resolver.promise);
    await createView();

    assertFalse(!!query('#emptyState'));
    assertFalse(!!query('#cardsList'));

    resolver.resolve({topics: [createTopic(), createTopic({id: 'topic-2'})]});
    await microtasksFinished();

    assertFalse(!!query('#emptyState'));
    const cards = view.shadowRoot.querySelectorAll('topic-card');
    assertEquals(2, cards.length);
    assertEquals('topic-1', cards[0]!.topic!.id);
  });

  test('shows the empty state when there are no topics', async () => {
    handler.setResultFor('getTopics', Promise.resolve({topics: []}));
    await createView();
    assertTrue(!!query('#emptyState'));
    assertFalse(!!query('#errorState'));
  });

  test('shows an error when topics fail to load', async () => {
    handler.setResultFor('getTopics', Promise.reject(new Error('failed')));
    await createView();
    assertTrue(!!query('#errorState'));
    assertFalse(!!query('#emptyState'));
  });

  test('does not fetch topics when the feature is off', async () => {
    loadTimeData.overrideValues({kTopics: false});
    await createView();
    assertEquals(0, handler.getCallCount('getTopics'));
    assertTrue(!!query('#emptyState'));
  });

  test('jump back in opens the topic details page', async () => {
    handler.setResultFor(
        'getTopics', Promise.resolve({topics: [createTopic()]}));
    await createView();

    const card = query('topic-card') as TopicCardElement;
    card.shadowRoot.querySelector<HTMLElement>('cr-button')!.click();

    const {topicUrl} = await handler.whenCalled('openTopic');
    const [base, search] = (topicUrl as string).split('?');
    assertEquals('chrome://context-hub/topic_details', base);
    const params = new URLSearchParams(search);
    assertEquals('topic-1', params.get('id'));
    assertEquals('1', params.get('open_glic'));
  });
});

suite('TopicDetails', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let openWindowProxy: TestOpenWindowProxy;
  let details: TopicDetailsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({kTopics: true});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    handler.setResultFor('getTopic', Promise.resolve({topic: createTopic()}));
    handler.setResultFor(
        'openUrlsInTabGroup', Promise.resolve({success: true}));
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  teardown(() => {
    window.history.replaceState({}, '', '/');
  });

  async function createDetails(search: string) {
    window.history.replaceState({}, '', search);
    details = document.createElement('topic-details');
    document.body.appendChild(details);
    await microtasksFinished();
  }

  function query(selector: string): HTMLElement|null {
    return details.shadowRoot.querySelector<HTMLElement>(selector);
  }

  function queryPanel(selector: string): HTMLElement|null {
    return query('topic-summary-panel')!.shadowRoot!.querySelector<HTMLElement>(
        selector);
  }

  function getSitesDialog(): TopicSitesDialogElement {
    return query('#sitesDialog') as TopicSitesDialogElement;
  }

  function getCrDialog(): CrDialogElement {
    return getSitesDialog().shadowRoot.querySelector('cr-dialog')!;
  }

  test('fetches the topic named in the URL and renders it', async () => {
    await createDetails('?id=topic-1');

    assertEquals('topic-1', await handler.whenCalled('getTopic'));
    assertEquals('Topic title', query('#title')!.textContent.trim());
    assertTrue(!!query('topic-hero'));
    const panel = query('topic-summary-panel')!;
    assertEquals(
        'Long overview.',
        panel.shadowRoot!.querySelector(
                             '#longDescription')!.textContent.trim());
    assertEquals(`${EMOJI} Topic title`, document.title);
    assertFalse(!!query('#notFound'));
  });

  test('opens the Glic panel with suggestions only when asked', async () => {
    await createDetails('?id=topic-1');
    assertEquals(0, handler.getCallCount('openGlicPanel'));

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    await createDetails('?id=topic-1&open_glic=1');
    assertDeepEquals(
        ['Query title'], await handler.whenCalled('openGlicPanel'));
  });

  test('shows not found when the topic does not exist', async () => {
    handler.setResultFor('getTopic', Promise.resolve({topic: null}));
    await createDetails('?id=missing&open_glic=1');
    assertTrue(!!query('#notFound'));
    assertFalse(!!query('#header'));
    assertEquals(0, handler.getCallCount('openGlicPanel'));
  });

  test('shows not found when the URL has no id', async () => {
    await createDetails('?');
    assertTrue(!!query('#notFound'));
    assertEquals(0, handler.getCallCount('getTopic'));
  });

  test('opens only the web URLs of the topic in a tab group', async () => {
    await createDetails('?id=topic-1');
    const button = queryPanel('#openRelatedTabs')!;
    assertFalse(button.hidden);
    button.click();

    const [label, urls] = await handler.whenCalled('openUrlsInTabGroup');
    assertEquals('Topic title', label);
    assertDeepEquals(
        ['https://example.com/page-1', 'http://example.com/page-2'], urls);
  });

  test('hides the site buttons when nothing can be opened', async () => {
    handler.setResultFor('getTopic', Promise.resolve({
      topic: createTopic({visits: [{url: 'chrome://history', title: ''}]}),
    }));
    await createDetails('?id=topic-1');
    assertTrue(queryPanel('#openRelatedTabs')!.hidden);
    assertTrue(query('#sitesButton')!.hidden);
  });

  test('sites button counts the sites and shows their favicons', async () => {
    await createDetails('?id=topic-1');
    const button = query('#sitesButton')!;
    assertFalse(button.hidden);
    assertEquals('2 sites', button.textContent.trim());
    assertEquals(2, button.querySelectorAll('.favicon').length);
  });

  test('sites button caps the count and the favicons', async () => {
    const visits: TopicVisit[] = [];
    for (let i = 0; i < MAX_TOPIC_SITES + 5; i++) {
      visits.push({url: `https://site${i}.com/`, title: `Site ${i}`});
    }
    handler.setResultFor(
        'getTopic', Promise.resolve({topic: createTopic({visits})}));
    await createDetails('?id=topic-1');

    const button = query('#sitesButton')!;
    assertEquals(`${MAX_TOPIC_SITES} sites`, button.textContent.trim());
    assertEquals(3, button.querySelectorAll('.favicon').length);
  });

  test('sites button opens the sites dialog', async () => {
    await createDetails('?id=topic-1');
    assertFalse(getCrDialog().open);

    query('#sitesButton')!.click();
    await microtasksFinished();
    assertTrue(getCrDialog().open);

    const dialog = getSitesDialog();
    assertEquals(
        'Topic title',
        dialog.shadowRoot.querySelector('#title')!.textContent.trim());
    const rows = dialog.shadowRoot.querySelectorAll<HTMLElement>('.site');
    assertDeepEquals(
        ['Page 1', 'Page 2'],
        Array.from(rows).map(
            row => row.querySelector('.site-title')!.textContent.trim()));
    assertEquals(
        'example.com', rows[0]!.querySelector('.site-domain')!.textContent);
  });

  test('sites dialog shows an untitled site by its domain once', async () => {
    const visits = [{url: 'https://www.example.com/', title: ''}];
    handler.setResultFor(
        'getTopic', Promise.resolve({topic: createTopic({visits})}));
    await createDetails('?id=topic-1');

    const row = getSitesDialog().shadowRoot.querySelector('.site')!;
    assertEquals(
        'example.com', row.querySelector('.site-title')!.textContent.trim());
    assertFalse(!!row.querySelector('.site-domain'));
  });

  test('clicking a site opens it in a new tab', async () => {
    await createDetails('?id=topic-1');
    query('#sitesButton')!.click();
    await microtasksFinished();

    getSitesDialog().shadowRoot.querySelectorAll<HTMLElement>(
                                   '.site')[1]!.click();
    assertEquals(
        'http://example.com/page-2',
        await openWindowProxy.whenCalled('openUrl'));
    assertTrue(getCrDialog().open);
  });

  test('open all tabs opens the related tabs and closes', async () => {
    await createDetails('?id=topic-1');
    query('#sitesButton')!.click();
    await microtasksFinished();

    getSitesDialog().shadowRoot.querySelector<HTMLElement>(
                                   '#openAllTabs')!.click();
    const [label, urls] = await handler.whenCalled('openUrlsInTabGroup');
    assertEquals('Topic title', label);
    assertDeepEquals(
        ['https://example.com/page-1', 'http://example.com/page-2'], urls);
    assertFalse(getCrDialog().open);
  });

  test('ok closes the sites dialog', async () => {
    await createDetails('?id=topic-1');
    query('#sitesButton')!.click();
    await microtasksFinished();

    getSitesDialog().shadowRoot.querySelector<HTMLElement>('#ok')!.click();
    assertFalse(getCrDialog().open);
    assertEquals(0, handler.getCallCount('openUrlsInTabGroup'));
  });

  test('has one panel per tab', async () => {
    await createDetails('?id=topic-1');
    const tabs = query('cr-tabs')!;
    assertEquals(
        TOPIC_DETAILS_TABS.length,
        tabs.shadowRoot!.querySelectorAll('[role=tab]').length);
    assertEquals(
        TOPIC_DETAILS_TABS.length,
        query('#panels')!.querySelectorAll('[role=tabpanel]').length);
  });
});
