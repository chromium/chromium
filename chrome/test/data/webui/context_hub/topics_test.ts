// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://context-hub/topics/topic_collection_carousel.js';
import 'chrome://context-hub/topics/topic_details.js';
import 'chrome://context-hub/topics/topics_view.js';

import {browserProxyFactory, PageHandlerRemote, TopicDefectCategory, TopicRating} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {Topic, TopicCollection, TopicFeedback, TopicsFeedbackExportPreview, TopicVisit} from 'chrome://context-hub/context_hub.mojom-webui.js';
import type {TopicCardElement} from 'chrome://context-hub/topics/topic_card.js';
import type {TopicCollectionCarouselElement} from 'chrome://context-hub/topics/topic_collection_carousel.js';
import type {TopicDetailsElement} from 'chrome://context-hub/topics/topic_details.js';
import {getSuggestedPrompts, TOPIC_DETAILS_TABS, TOPIC_VISITS_TAB} from 'chrome://context-hub/topics/topic_details.js';
import type {TopicFeedbackControlsElement} from 'chrome://context-hub/topics/topic_feedback_controls.js';
import {COVERAGE_TOPICS_STORAGE_KEY, getExportFileName, pickCoverageTopicIds, setFileDownloaderForTesting} from 'chrome://context-hub/topics/topic_feedback_export_dialog.js';
import type {TopicFeedbackExportDialogElement} from 'chrome://context-hub/topics/topic_feedback_export_dialog.js';
import type {TopicSitesDialogElement} from 'chrome://context-hub/topics/topic_sites_dialog.js';
import {BADGE_BACKGROUND_COLORS, createEmptyTopicFeedback, createTopicSnapshot, DEFAULT_ICON, formatTopicFeedbackComment, getBackgroundColorForTopic, getBadgePath, getBadgeShapeForTopic, getDisplayDomain, getOpenableUrls, getTopicSites, isTopicFeedbackEmpty, isTopicFeedbackValid, MAX_TOPIC_SITES, normalizeTopicFeedbackComment, parseTopicFeedbackComment, TOPIC_DEFECT_CATEGORIES, toTopicItem} from 'chrome://context-hub/topics/topic_utils.js';
import type {BadgeShape, Collection} from 'chrome://context-hub/topics/topic_utils.js';
import type {TopicsViewElement} from 'chrome://context-hub/topics/topics_view.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrChipElement} from 'chrome://resources/cr_elements/cr_chip/cr_chip.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
import type {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

// U+1F4C1 FILE FOLDER.
const EMOJI = '\u{1F4C1}';

function createVisit(url: string, title: string, time = 0): TopicVisit {
  return {url, title, visitTime: {internalValue: BigInt(time)}};
}

function createTopic(overrides: Partial<Topic> = {}): Topic {
  return {
    id: 'topic-1',
    title: 'Topic title',
    creationTime: {internalValue: 0n},
    emoji: EMOJI,
    overview: 'Long overview.',
    shortOverview: 'Short overview.',
    visits: [
      createVisit('https://example.com/page-1', 'Page 1', 3),
      createVisit('chrome://settings', 'Settings', 2),
      createVisit('http://example.com/page-2', 'Page 2', 1),
    ],
    continuationQueries: [
      {title: 'Query title', prompt: 'Query prompt'},
    ],
    collections: [],
    ...overrides,
  };
}

function createCollection(overrides: Partial<TopicCollection> = {}):
    TopicCollection {
  return {
    title: 'Collection 1',
    items: [
      {
        title: 'Item 1',
        url: 'https://example.com/item-1',
        siteName: 'Site name',
      },
      {
        title: 'Item 2',
        url: 'https://www.example.org/item-2',
        siteName: null,
      },
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
        createVisit('https://a.com/', ''),
        createVisit('file:///etc/passwd', ''),
        createVisit('javascript:alert(1)', ''),
        createVisit('http://b.com/', ''),
        createVisit('', ''),
      ],
    }));
    assertDeepEquals(
        ['https://a.com/', 'http://b.com/'], getOpenableUrls(item));
  });

  test('getTopicSites keeps web URLs once each, most recent first', () => {
    const item = toTopicItem(createTopic({
      visits: [
        createVisit('https://a.com/', 'A', 1),
        createVisit('http://b.com/', 'B', 2),
        createVisit('chrome://history', 'History', 5),
        createVisit('https://c.com/', 'C', 3),
        createVisit('https://a.com/', 'A again', 4),
      ],
    }));
    assertDeepEquals(
        ['A again', 'C', 'B'], getTopicSites(item).map(site => site.title));
  });

  test('getTopicSites caps to the most recent sites', () => {
    const visits: TopicVisit[] = [];
    for (let i = 0; i < MAX_TOPIC_SITES + 5; i++) {
      visits.push(createVisit(`https://site${i}.com/`, `Site ${i}`, i));
    }
    const sites = getTopicSites(toTopicItem(createTopic({visits})));
    assertEquals(MAX_TOPIC_SITES, sites.length);
    assertEquals(`Site ${MAX_TOPIC_SITES + 4}`, sites[0]!.title);
    assertEquals('Site 5', sites[MAX_TOPIC_SITES - 1]!.title);
  });

  test('getDisplayDomain drops www and handles bad URLs', () => {
    assertEquals('example.com', getDisplayDomain('https://www.example.com/a'));
    assertEquals(
        'news.example.com', getDisplayDomain('http://news.example.com'));
    assertEquals('', getDisplayDomain('not a url'));
  });

  test('toTopicItem maps collections, falling back to the domain', () => {
    const item = toTopicItem(createTopic({collections: [createCollection()]}));
    assertDeepEquals(
        [{
          title: 'Collection 1',
          items: [
            {
              title: 'Item 1',
              url: 'https://example.com/item-1',
              siteName: 'Site name',
            },
            {
              title: 'Item 2',
              url: 'https://www.example.org/item-2',
              siteName: 'example.org',
            },
          ],
        }],
        item.collections);
  });

  test('toTopicItem drops unopenable items and empty collections', () => {
    const item = toTopicItem(createTopic({
      collections: [
        createCollection({
          items: [
            {title: 'Settings', url: 'chrome://settings', siteName: null},
            {title: ' ', url: 'https://a.com/', siteName: null},
            {title: 'B', url: 'https://b.com/', siteName: ' '},
          ],
        }),
        createCollection({title: ' '}),
        createCollection({
          items: [{title: 'History', url: 'chrome://history', siteName: null}],
        }),
      ],
    }));
    assertEquals(1, item.collections.length);
    assertDeepEquals(
        [{title: 'B', url: 'https://b.com/', siteName: 'b.com'}],
        item.collections[0]!.items);
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

  test('createTopicSnapshot captures the topic as shown', () => {
    const snapshot = createTopicSnapshot(toTopicItem(createTopic()));
    assertEquals('Topic title', snapshot.title);
    assertEquals(EMOJI, snapshot.emoji);
    assertEquals('Long overview.', snapshot.overview);
    assertDeepEquals(
        [3, 2, 1], snapshot.visitTimes.map(time => Number(time.internalValue)));

    // The default icon isn't an emoji.
    assertEquals(
        '', createTopicSnapshot(toTopicItem(createTopic({emoji: null}))).emoji);
  });

  test('defect categories cover every TopicDefectCategory once', () => {
    const categories = TOPIC_DEFECT_CATEGORIES.map(item => item.category);
    assertEquals(
        TopicDefectCategory.MAX_VALUE - TopicDefectCategory.MIN_VALUE + 1,
        new Set(categories).size);
    assertEquals(categories.length, new Set(categories).size);
    assertTrue(TOPIC_DEFECT_CATEGORIES.every(
        item => !!item.label && !!item.description));
  });

  test('isTopicFeedbackValid requires a defect and an Other comment', () => {
    const empty = createEmptyTopicFeedback(toTopicItem(createTopic()));
    assertTrue(isTopicFeedbackValid(empty));
    assertTrue(isTopicFeedbackEmpty(empty));
    assertTrue(isTopicFeedbackValid({...empty, rating: TopicRating.kLiked}));
    assertFalse(isTopicFeedbackEmpty({...empty, rating: TopicRating.kLiked}));

    const disliked = {...empty, rating: TopicRating.kDisliked};
    assertFalse(isTopicFeedbackValid(disliked));
    assertTrue(isTopicFeedbackValid(
        {...disliked, defects: [TopicDefectCategory.kTooBroad]}));

    const other = {...disliked, defects: [TopicDefectCategory.kOther]};
    assertFalse(isTopicFeedbackValid(other));
    assertFalse(isTopicFeedbackValid({...other, comment: ' '}));
    assertTrue(isTopicFeedbackValid({...other, comment: 'Why'}));
    // Suggestions alone don't describe the issue.
    assertFalse(
        isTopicFeedbackValid({...other, comment: 'Better title: New title'}));
  });

  test('suggestions are stored as lines at the end of the comment', () => {
    const suggestions = new Map([
      [TopicDefectCategory.kOverviewInaccurate, 'New\noverview'],
      [TopicDefectCategory.kTitleWrongOrVague, 'New title'],
    ]);
    const comment = formatTopicFeedbackComment('Some text', suggestions);
    assertEquals(
        'Some text\n\nBetter title: New title\nBetter overview: New overview',
        comment);

    const parsed = parseTopicFeedbackComment(comment);
    assertEquals('Some text', parsed.text);
    assertEquals(
        'New title',
        parsed.suggestions.get(TopicDefectCategory.kTitleWrongOrVague));
    assertEquals(
        'New overview',
        parsed.suggestions.get(TopicDefectCategory.kOverviewInaccurate));
    assertFalse(parsed.suggestions.has(TopicDefectCategory.kEmojiWrong));

    // Without text, the comment is just the suggestions.
    assertEquals(
        'Better emoji: X',
        formatTopicFeedbackComment(
            '', new Map([[TopicDefectCategory.kEmojiWrong, 'X']])));
    // Without suggestions, the comment is just the text.
    assertEquals('Text', formatTopicFeedbackComment('Text', new Map()));
    assertEquals('Text', parseTopicFeedbackComment('Text').text);
    assertEquals(0, parseTopicFeedbackComment('Text').suggestions.size);

    // Text and suggestions keep their whitespace while being edited...
    const editing = formatTopicFeedbackComment(
        'Text\n', new Map([[TopicDefectCategory.kTitleWrongOrVague, 'A ']]));
    assertEquals('Text\n', parseTopicFeedbackComment(editing).text);
    assertEquals(
        'A ',
        parseTopicFeedbackComment(editing).suggestions.get(
            TopicDefectCategory.kTitleWrongOrVague));
    // ...and are trimmed when stored, dropping blank suggestions.
    assertEquals(
        'Text\n\nBetter title: A', normalizeTopicFeedbackComment(editing));
    assertEquals(
        'Text', normalizeTopicFeedbackComment('Text\n\nBetter title:  '));
  });
});

suite('TopicsView', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let view: TopicsViewElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues(
        {kTopics: true, kTopicsFishfoodFeedback: false});
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

  test('hides fishfood feedback controls when it is off', async () => {
    handler.setResultFor(
        'getTopics', Promise.resolve({topics: [createTopic()]}));
    await createView();

    assertFalse(!!query('#sendFeedbackButton'));
    assertFalse(!!query('#fishfoodNote'));
    const card = query('topic-card') as TopicCardElement;
    assertFalse(!!card.shadowRoot.querySelector('topic-feedback-controls'));
    assertEquals(0, handler.getCallCount('getTopicFeedbacks'));
  });
});

suite('TopicsFeedback', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let view: TopicsViewElement;
  let card: TopicCardElement;
  let controls: TopicFeedbackControlsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({kTopics: true, kTopicsFishfoodFeedback: true});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    handler.setResultFor(
        'getTopics', Promise.resolve({topics: [createTopic()]}));
  });

  async function createView(feedbacks: TopicFeedback[] = []) {
    handler.setResultFor('getTopicFeedbacks', Promise.resolve({feedbacks}));
    view = document.createElement('topics-view');
    document.body.appendChild(view);
    await microtasksFinished();
    card = view.shadowRoot.querySelector('topic-card')!;
    controls = card.shadowRoot.querySelector('topic-feedback-controls')!;
  }

  function queryCard<T extends HTMLElement = HTMLElement>(selector: string): T|
      null {
    return controls.shadowRoot.querySelector<T>(selector);
  }

  function getChip(category: TopicDefectCategory): CrChipElement {
    return queryCard<CrChipElement>(`cr-chip[data-category="${category}"]`)!;
  }

  async function click(selector: string) {
    queryCard(selector)!.click();
    await microtasksFinished();
  }

  async function clickChip(category: TopicDefectCategory) {
    getChip(category).click();
    await microtasksFinished();
  }

  function getLastSavedFeedback(): TopicFeedback {
    const args = handler.getArgs('setTopicFeedback');
    return args[args.length - 1];
  }

  test('loads feedback and shows the send feedback button', async () => {
    await createView();
    assertEquals(1, handler.getCallCount('getTopicFeedbacks'));
    assertTrue(!!view.shadowRoot.querySelector('#sendFeedbackButton'));
    assertTrue(!!view.shadowRoot.querySelector('#fishfoodNote'));
    assertTrue(!!controls);
    assertTrue(!!queryCard('#thumbsUp'));
    assertTrue(!!queryCard('#thumbsDown'));
    assertEquals(
        'Good topic: Topic title',
        queryCard('#thumbsUp')!.getAttribute('aria-label'));
    assertEquals(
        'Bad topic: Topic title',
        queryCard('#thumbsDown')!.getAttribute('aria-label'));
    assertFalse(!!queryCard('#defectsPanel'));
  });

  test('reloads feedback when the tab is shown again', async () => {
    await createView();
    assertEquals(null, card.feedback);

    // The topic was rated on its details page in another tab.
    const rated: TopicFeedback = {
      ...createEmptyTopicFeedback(toTopicItem(createTopic())),
      rating: TopicRating.kLiked,
    };
    handler.setResultFor(
        'getTopicFeedbacks', Promise.resolve({feedbacks: [rated]}));
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();

    assertEquals(2, handler.getCallCount('getTopicFeedbacks'));
    assertEquals(TopicRating.kLiked, card.feedback!.rating);
  });

  test('thumbs up saves a liked rating with a snapshot', async () => {
    await createView();
    await click('#thumbsUp');

    assertEquals(1, handler.getCallCount('setTopicFeedback'));
    const feedback = getLastSavedFeedback();
    assertEquals('topic-1', feedback.id);
    assertEquals(TopicRating.kLiked, feedback.rating);
    assertDeepEquals([], feedback.defects);
    assertEquals('Topic title', feedback.snapshot.title);
    assertEquals(EMOJI, feedback.snapshot.emoji);
    assertEquals('Long overview.', feedback.snapshot.overview);
    assertEquals(3, feedback.snapshot.visitTimes.length);
    assertEquals('true', queryCard('#thumbsUp')!.getAttribute('aria-pressed'));
    assertFalse(!!queryCard('#defectsPanel'));

    // Clicking it again clears the rating.
    await click('#thumbsUp');
    assertEquals('topic-1', await handler.whenCalled('deleteTopicFeedback'));
    assertEquals('false', queryCard('#thumbsUp')!.getAttribute('aria-pressed'));
  });

  test('thumbs down needs a defect, and saves each chip toggle', async () => {
    await createView();
    await click('#thumbsDown');

    assertTrue(!!queryCard('#defectsPanel'));
    assertTrue(!!queryCard('#defectError'));
    assertEquals(
        TOPIC_DEFECT_CATEGORIES.length,
        controls.shadowRoot.querySelectorAll('cr-chip').length);
    assertEquals(0, handler.getCallCount('setTopicFeedback'));

    await clickChip(TopicDefectCategory.kTooBroad);
    await clickChip(TopicDefectCategory.kEmojiWrong);
    assertFalse(!!queryCard('#defectError'));
    assertTrue(getChip(TopicDefectCategory.kTooBroad).selected);
    assertTrue(getChip(TopicDefectCategory.kEmojiWrong).selected);
    assertEquals(2, handler.getCallCount('setTopicFeedback'));
    let feedback = getLastSavedFeedback();
    assertEquals(TopicRating.kDisliked, feedback.rating);
    assertDeepEquals(
        [TopicDefectCategory.kTooBroad, TopicDefectCategory.kEmojiWrong],
        feedback.defects);

    await clickChip(TopicDefectCategory.kTooBroad);
    assertFalse(getChip(TopicDefectCategory.kTooBroad).selected);
    feedback = getLastSavedFeedback();
    assertDeepEquals([TopicDefectCategory.kEmojiWrong], feedback.defects);

    // Deselecting the last defect isn't valid, so isn't saved.
    await clickChip(TopicDefectCategory.kEmojiWrong);
    assertEquals(3, handler.getCallCount('setTopicFeedback'));
    assertTrue(!!queryCard('#defectError'));
  });

  test('chips show a short label and the full description', async () => {
    await createView();
    await click('#thumbsDown');

    const chip = getChip(TopicDefectCategory.kCombinesSeparateTopics);
    assertEquals('Mixes topics', chip.textContent.trim());
    assertEquals('Combines separate topics', chip.title);
    assertEquals('Combines separate topics', chip.chipAriaLabel);
  });

  test('Other requires a comment', async () => {
    await createView();
    await click('#thumbsDown');
    await clickChip(TopicDefectCategory.kOther);

    const comment = queryCard<CrTextareaElement>('#comment')!;
    assertTrue(comment.required);
    assertTrue(comment.invalid);
    assertTrue(!!comment.firstFooter);
    assertEquals('flex', getComputedStyle(comment.$.footerContainer).display);
    assertEquals(0, handler.getCallCount('setTopicFeedback'));

    async function enterComment(value: string) {
      comment.$.input.value = value;
      comment.$.input.dispatchEvent(new Event('input'));
      comment.$.input.dispatchEvent(new Event('change'));
      await microtasksFinished();
    }

    // Whitespace isn't a comment.
    await enterComment('  ');
    assertTrue(comment.invalid);
    assertEquals(0, handler.getCallCount('setTopicFeedback'));

    await enterComment(' Not a real topic ');
    assertFalse(comment.invalid);
    assertEquals('none', getComputedStyle(comment.$.footerContainer).display);
    assertEquals(1, handler.getCallCount('setTopicFeedback'));
    const feedback = getLastSavedFeedback();
    assertDeepEquals([TopicDefectCategory.kOther], feedback.defects);
    assertEquals('Not a real topic', feedback.comment);
  });

  test('asks for a better title, emoji or overview', async () => {
    await createView();
    await click('#thumbsDown');
    assertEquals(0, controls.shadowRoot.querySelectorAll('.suggestion').length);

    await clickChip(TopicDefectCategory.kOverviewInaccurate);
    await clickChip(TopicDefectCategory.kTitleWrongOrVague);
    await clickChip(TopicDefectCategory.kTooBroad);
    const inputs =
        controls.shadowRoot.querySelectorAll<CrInputElement>('.suggestion');
    // In display order, whatever order the chips were picked in.
    assertDeepEquals(
        ['Better title', 'Better overview'],
        Array.from(inputs, input => input.label));

    async function enter(element: CrInputElement|CrTextareaElement,
                         value: string) {
      element.$.input.value = value;
      element.$.input.dispatchEvent(new Event('input'));
      element.$.input.dispatchEvent(new Event('change'));
      await microtasksFinished();
    }

    await enter(inputs[0]!, ' New title ');
    assertEquals('Better title: New title', getLastSavedFeedback().comment);

    const comment = queryCard<CrTextareaElement>('#comment')!;
    await enter(comment, 'Some text');
    assertEquals(
        'Some text\n\nBetter title: New title', getLastSavedFeedback().comment);
    // The textarea only shows the rater's own text.
    assertEquals('Some text', comment.value);

    await enter(inputs[1]!, 'New overview');
    assertEquals(
        'Some text\n\nBetter title: New title\nBetter overview: New overview',
        getLastSavedFeedback().comment);

    // Deselecting a defect drops its suggestion.
    await clickChip(TopicDefectCategory.kTitleWrongOrVague);
    assertEquals(1, controls.shadowRoot.querySelectorAll('.suggestion').length);
    assertEquals(
        'Some text\n\nBetter overview: New overview',
        getLastSavedFeedback().comment);
  });

  test('shows stored suggestions in their fields', async () => {
    const stored: TopicFeedback = {
      ...createEmptyTopicFeedback(toTopicItem(createTopic())),
      rating: TopicRating.kDisliked,
      defects: [TopicDefectCategory.kEmojiWrong],
      comment: 'Stored comment\n\nBetter emoji: X',
    };
    await createView([stored]);

    assertEquals(
        'Stored comment', queryCard<CrTextareaElement>('#comment')!.value);
    const input = queryCard<CrInputElement>('.suggestion')!;
    assertEquals('Better emoji', input.label);
    assertEquals('X', input.value);
  });

  test('shows stored feedback, and thumbs up clears its defects', async () => {
    const stored: TopicFeedback = {
      ...createEmptyTopicFeedback(toTopicItem(createTopic())),
      rating: TopicRating.kDisliked,
      defects: [TopicDefectCategory.kTooNarrowOrFragmented],
      comment: 'Stored comment',
      rejectedVisits: [{internalValue: 2n}],
    };
    await createView([stored]);

    assertEquals(
        'true', queryCard('#thumbsDown')!.getAttribute('aria-pressed'));
    assertTrue(getChip(TopicDefectCategory.kTooNarrowOrFragmented).selected);
    assertEquals(
        'Stored comment', queryCard<CrTextareaElement>('#comment')!.value);

    await click('#thumbsUp');
    assertFalse(!!queryCard('#defectsPanel'));
    const feedback = getLastSavedFeedback();
    assertEquals(TopicRating.kLiked, feedback.rating);
    assertDeepEquals([], feedback.defects);
    assertEquals('', feedback.comment);
    // Feedback the card doesn't edit is kept.
    assertEquals(1, feedback.rejectedVisits.length);
  });
});

suite('TopicFeedbackExportDialog', () => {
  const FORM_URL = 'https://form.example.com/';
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let openWindowProxy: TestOpenWindowProxy;
  let view: TopicsViewElement;
  let dialog: TopicFeedbackExportDialogElement;
  let downloads: Array<{contents: string, fileName: string}>;

  function createPreview(): TopicsFeedbackExportPreview {
    return {
      ldap: 'rater',
      stats:
          {topics: 4, visits: 5, unclusteredVisits: 1, unresolvableTopics: 0},
      domains: [
        {
          domain: 'www.example.com',
          visitCount: 3,
          defaultExcluded: false,
          urls: [
            {url: 'https://www.example.com/a', title: 'Page A', visitCount: 2},
            {url: 'https://www.example.com/b', title: 'Page B', visitCount: 1},
          ],
        },
        {
          domain: 'docs.google.com',
          visitCount: 2,
          defaultExcluded: true,
          urls:
              [{url: 'https://docs.google.com/d', title: 'Doc', visitCount: 2}],
        },
      ],
    };
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      kTopics: true,
      kTopicsFishfoodFeedback: true,
      kTopicsFeedbackFormUrl: FORM_URL,
    });
    sessionStorage.clear();
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    handler.setResultFor('getTopics', Promise.resolve({
      topics: [1, 2, 3, 4].map(
          i => createTopic({id: `topic-${i}`, title: `Topic ${i}`})),
    }));
    handler.setResultFor(
        'getTopicsFeedbackExportPreview',
        Promise.resolve({preview: createPreview()}));
    handler.setResultFor(
        'generateTopicsFeedbackBundle',
        Promise.resolve({jsonBundle: '{"schema_version":1}'}));
    handler.setResultFor('clearTopicFeedbacks', Promise.resolve());
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    downloads = [];
    setFileDownloaderForTesting(
        (contents, fileName) => downloads.push({contents, fileName}));
  });

  teardown(() => {
    setFileDownloaderForTesting(null);
    sessionStorage.clear();
  });

  async function openDialog(feedbacks: TopicFeedback[] = []) {
    handler.setResultFor('getTopicFeedbacks', Promise.resolve({feedbacks}));
    view = document.createElement('topics-view');
    document.body.appendChild(view);
    await microtasksFinished();
    view.shadowRoot.querySelector<HTMLElement>('#sendFeedbackButton')!.click();
    await microtasksFinished();
    dialog = view.shadowRoot.querySelector('topic-feedback-export-dialog')!;
    await microtasksFinished();
  }

  function query<T extends HTMLElement = HTMLElement>(selector: string): T|
      null {
    return dialog.shadowRoot.querySelector<T>(selector);
  }

  function queryAll<T extends HTMLElement = HTMLElement>(selector: string):
      T[] {
    return Array.from(dialog.shadowRoot.querySelectorAll<T>(selector));
  }

  async function click(selector: string) {
    query(selector)!.click();
    await microtasksFinished();
  }

  // The dialog's close event is dispatched asynchronously.
  async function closeDialog() {
    const closed = eventToPromise('export-dialog-close', dialog);
    query('#close')!.click();
    await closed;
    await microtasksFinished();
  }

  async function select(element: HTMLSelectElement, value: string) {
    element.value = value;
    element.dispatchEvent(new Event('change'));
    await microtasksFinished();
  }

  test('opens from the topics view and loads the preview', async () => {
    await openDialog();
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
    assertEquals(5, await handler.whenCalled('getTopicsFeedbackExportPreview'));
    assertEquals(
        '4 topics · 5 visits · 1 visit not in any topic',
        query('#stats')!.textContent.trim().replace(/\s+/g, ' '));
    assertEquals(2, queryAll('.domain').length);

    await closeDialog();
    assertFalse(
        !!view.shadowRoot.querySelector('topic-feedback-export-dialog'));
  });

  test('changing the window refetches the preview', async () => {
    await openDialog();
    await select(query<HTMLSelectElement>('#windowSelect')!, '14');
    assertDeepEquals(
        [5, 14], handler.getArgs('getTopicsFeedbackExportPreview'));
    assertEquals(14, dialog.getExportOptionsForTesting().windowDays);
  });

  test('excludes default domains and applies domain choices', async () => {
    await openDialog();
    let options = dialog.getExportOptionsForTesting();
    assertDeepEquals(['docs.google.com'], options.excludedDomains);
    // Titles are never stripped on their own.
    assertDeepEquals([], options.titleStrippedDomains);
    assertFalse(options.stripAllTitles);
    assertFalse(!!query('#stripAllTitles'));
    const domains = queryAll('.domain');
    assertFalse(domains[0]!.hasAttribute('excluded'));
    assertTrue(domains[1]!.hasAttribute('excluded'));

    const checkboxes = queryAll<CrCheckboxElement>('.domain-checkbox');
    assertTrue(checkboxes[0]!.checked);
    assertFalse(checkboxes[1]!.checked);
    assertEquals('Share www.example.com', checkboxes[0]!.ariaLabelOverride);
    assertEquals('', checkboxes[0]!.textContent.trim());
    assertTrue(queryAll('.domain-count')[1]!.textContent.includes('left out'));

    checkboxes[0]!.click();
    checkboxes[1]!.click();
    await microtasksFinished();
    options = dialog.getExportOptionsForTesting();
    assertDeepEquals(['www.example.com'], options.excludedDomains);
    assertTrue(domains[0]!.hasAttribute('excluded'));
    assertFalse(domains[1]!.hasAttribute('excluded'));
    assertDeepEquals([], options.titleStrippedDomains);
    assertFalse(options.stripAllTitles);
  });

  test('expanding a domain excludes single URLs', async () => {
    await openDialog();
    assertEquals(0, queryAll('.url-checkbox').length);
    await click('.domain-expand');
    const checkboxes = queryAll<CrCheckboxElement>('.url-checkbox');
    assertEquals(2, checkboxes.length);
    assertTrue(checkboxes.every(checkbox => checkbox.checked));

    checkboxes[1]!.click();
    await microtasksFinished();
    assertDeepEquals(
        ['https://www.example.com/b'],
        dialog.getExportOptionsForTesting().excludedUrls);
    assertTrue(
        query('.domain-count')!.textContent.includes('1 page left out'));

    // Excluding the whole domain redacts all of its URLs instead.
    await click('.domain-checkbox');
    const options = dialog.getExportOptionsForTesting();
    assertDeepEquals(
        ['www.example.com', 'docs.google.com'], options.excludedDomains);
    assertDeepEquals([], options.excludedUrls);
    assertTrue(
        queryAll<CrCheckboxElement>('.url-checkbox')
            .every(checkbox => checkbox.disabled && !checkbox.checked));
  });

  // Thumbs-up feedback on the first `count` of the 4 topics.
  function likeTopics(count: number): TopicFeedback[] {
    return [1, 2, 3, 4].slice(0, count).map(i => {
      const topic = createTopic({id: `topic-${i}`, title: `Topic ${i}`});
      return {
        ...createEmptyTopicFeedback(toTopicItem(topic)),
        rating: TopicRating.kLiked,
      };
    });
  }

  test('download needs the acknowledgment', async () => {
    await openDialog(likeTopics(4));
    assertFalse(!!query('#ldap'));
    const download = query<HTMLButtonElement>('#download')!;
    assertTrue(download.disabled);

    await click('#acknowledge');
    assertFalse(download.disabled);
  });

  test('download needs every topic rated', async () => {
    await openDialog(likeTopics(3));
    await click('#acknowledge');
    const download = query<HTMLButtonElement>('#download')!;
    assertTrue(download.disabled);
    assertFalse(query('#ratingStatus')!.hasAttribute('complete'));
    assertTrue(query('#ratingProgress')!.textContent.includes(
        'You rated 3 of 4 topics. Rate every topic'));

    // The last topic was rated on its details page in another tab.
    handler.setResultFor(
        'getTopicFeedbacks', Promise.resolve({feedbacks: likeTopics(4)}));
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    assertFalse(download.disabled);
    assertTrue(query('#ratingStatus')!.hasAttribute('complete'));
    assertEquals(
        'You rated every topic.',
        query('#ratingProgress')!.textContent.trim());
  });

  test('downloads the bundle and links to the form', async () => {
    await openDialog(likeTopics(4));
    const missingTopics = query<CrTextareaElement>('#missingTopics')!;
    missingTopics.value = ' My trip planning ';
    await click('#acknowledge');
    await click('#download');

    const options = await handler.whenCalled('generateTopicsFeedbackBundle');
    assertEquals('', options.rater);
    assertEquals('My trip planning', options.missingTopics);
    assertEquals(5, options.windowDays);
    assertEquals(1, downloads.length);
    assertEquals('{"schema_version":1}', downloads[0]!.contents);
    assertTrue(/^topics-feedback-\d{4}-\d{2}-\d{2}\.json$/.test(
        downloads[0]!.fileName));
    assertTrue(!!query('#exportDone'));

    await click('#openForm');
    assertEquals(FORM_URL, await openWindowProxy.whenCalled('openUrl'));
  });

  test('asks to review up to 3 topics, kept for the session', async () => {
    sessionStorage.setItem(COVERAGE_TOPICS_STORAGE_KEY, '["topic-2"]');
    const detailed: TopicFeedback = {
      ...createEmptyTopicFeedback(
          toTopicItem(createTopic({id: 'topic-2', title: 'Topic 2'}))),
      rating: TopicRating.kLiked,
      queryFeedbacks: [{index: 0, queryText: 'Query title', liked: true}],
    };
    await openDialog([detailed]);

    assertTrue(
        query('#ratingProgress')!.textContent.includes('You rated 1 of'));
    const items = queryAll('#coverageTopics li');
    assertEquals(3, items.length);
    assertTrue(items[0]!.hasAttribute('done'));
    assertFalse(items[1]!.hasAttribute('done'));
    const ids =
        JSON.parse(sessionStorage.getItem(COVERAGE_TOPICS_STORAGE_KEY)!);
    assertEquals(3, ids.length);
    assertEquals('topic-2', ids[0]);

    items[1]!.querySelector('a')!.click();
    const {topicUrl} = await handler.whenCalled('openTopic');
    assertEquals(`chrome://context-hub/topic_details?id=${ids[1]}`, topicUrl);

    // Reopening the dialog asks about the same topics.
    await closeDialog();
    view.shadowRoot.querySelector<HTMLElement>('#sendFeedbackButton')!.click();
    await microtasksFinished();
    dialog = view.shadowRoot.querySelector('topic-feedback-export-dialog')!;
    await microtasksFinished();
    assertDeepEquals(
        ids, JSON.parse(sessionStorage.getItem(COVERAGE_TOPICS_STORAGE_KEY)!));
  });

  test('clear my ratings asks first, then clears', async () => {
    const stored: TopicFeedback = {
      ...createEmptyTopicFeedback(
          toTopicItem(createTopic({id: 'topic-1', title: 'Topic 1'}))),
      rating: TopicRating.kLiked,
    };
    await openDialog([stored]);
    await click('#clearRatings');
    assertEquals(0, handler.getCallCount('clearTopicFeedbacks'));
    await click('#cancelClear');
    assertTrue(!!query('#clearRatings'));

    await click('#clearRatings');
    await click('#confirmClear');
    await handler.whenCalled('clearTopicFeedbacks');
    await microtasksFinished();
    const card = view.shadowRoot.querySelector<TopicCardElement>('topic-card')!;
    assertEquals(null, card.feedback);
    assertTrue(
        query('#ratingProgress')!.textContent.includes('You rated 0 of'));
  });

  test('pickCoverageTopicIds keeps stored ids and tops up', () => {
    const ids = ['a', 'b', 'c', 'd', 'e'];
    assertDeepEquals(
        ['a', 'b', 'c'], pickCoverageTopicIds(ids, [], 3, () => 0));
    assertDeepEquals(
        ['d', 'a', 'b'], pickCoverageTopicIds(ids, ['d', 'gone'], 3, () => 0));
    assertDeepEquals(['b', 'a'], pickCoverageTopicIds(['a', 'b'], ['b']));
    assertDeepEquals(
        ['d', 'a', 'b'], pickCoverageTopicIds(ids, ['d', 'd'], 3, () => 0));
  });

  test('getExportFileName uses the local date', () => {
    assertEquals(
        'topics-feedback-2026-03-07.json',
        getExportFileName(new Date(2026, 2, 7)));
  });
});

suite('TopicDetails', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let openWindowProxy: TestOpenWindowProxy;
  let details: TopicDetailsElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues(
        {kTopics: true, kTopicsFishfoodFeedback: false});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    handler.setResultFor('getTopic', Promise.resolve({topic: createTopic()}));
    handler.setResultFor(
        'openUrlsInTabGroup', Promise.resolve({success: true}));
    handler.setResultFor(
        'getTopicPageImageUrl', Promise.resolve({imageUrl: null}));
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
      topic: createTopic({visits: [createVisit('chrome://history', '')]}),
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
      visits.push(createVisit(`https://site${i}.com/`, `Site ${i}`));
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
    const visits = [createVisit('https://www.example.com/', '')];
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

  test('summary panel shows a carousel per collection', async () => {
    handler.setResultFor('getTopic', Promise.resolve({
      topic: createTopic({
        collections: [
          createCollection(),
          createCollection({title: 'Collection 2'}),
        ],
      }),
    }));
    await createDetails('?id=topic-1');

    const carousels =
        query('topic-summary-panel')!.shadowRoot!.querySelectorAll(
            'topic-collection-carousel');
    assertDeepEquals(
        ['Collection 1', 'Collection 2'],
        Array.from(carousels).map(carousel => carousel.collection!.title));
  });

  test('hides fishfood feedback when it is off', async () => {
    await createDetails('?id=topic-1');
    assertFalse(!!query('#fishfoodRating'));
    assertFalse(!!query('topic-visits-panel'));
    assertFalse(!!queryPanel('#queryRatings'));
    assertFalse(!!getSitesDialog().shadowRoot.querySelector('.reject'));
    assertFalse(
        query('cr-tabs')!.shadowRoot!.textContent.includes(TOPIC_VISITS_TAB));
    assertEquals(0, handler.getCallCount('getTopicFeedbacks'));
  });
});

suite('TopicDetailsFeedback', () => {
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let details: TopicDetailsElement;

  const topic = createTopic({
    continuationQueries: [
      {title: 'Query 1', prompt: ''},
      {title: 'Query 2', prompt: ''},
      {title: 'Query 3', prompt: ''},
      {title: 'Query 4', prompt: ''},
    ],
  });
  const otherTopic = createTopic({id: 'topic-2', title: 'Other topic'});

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({kTopics: true, kTopicsFishfoodFeedback: true});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    handler.setResultFor('getTopic', Promise.resolve({topic}));
    handler.setResultFor(
        'getTopics', Promise.resolve({topics: [topic, otherTopic]}));
    handler.setResultFor(
        'getTopicPageImageUrl', Promise.resolve({imageUrl: null}));
  });

  teardown(() => {
    window.history.replaceState({}, '', '/');
  });

  async function createDetails(feedbacks: TopicFeedback[] = []) {
    handler.setResultFor('getTopicFeedbacks', Promise.resolve({feedbacks}));
    window.history.replaceState({}, '', '?id=topic-1');
    details = document.createElement('topic-details');
    document.body.appendChild(details);
    await microtasksFinished();
  }

  function query<T extends HTMLElement = HTMLElement>(selector: string): T|
      null {
    return details.shadowRoot.querySelector<T>(selector);
  }

  function queryIn(element: HTMLElement, selector: string): HTMLElement[] {
    return Array.from(
        element.shadowRoot!.querySelectorAll<HTMLElement>(selector));
  }

  async function click(element: HTMLElement) {
    element.click();
    await microtasksFinished();
  }

  function getLastSavedFeedback(): TopicFeedback {
    const args = handler.getArgs('setTopicFeedback');
    return args[args.length - 1];
  }

  function createStoredFeedback(overrides: Partial<TopicFeedback> = {}):
      TopicFeedback {
    return {...createEmptyTopicFeedback(toTopicItem(topic)), ...overrides};
  }

  test('shows the visits tab with its banner', async () => {
    await createDetails();
    assertEquals(1, handler.getCallCount('getTopicFeedbacks'));
    const tabs = query('cr-tabs')!;
    assertEquals(
        TOPIC_DETAILS_TABS.length + 1,
        tabs.shadowRoot!.querySelectorAll('[role=tab]').length);
    assertTrue(tabs.shadowRoot!.textContent.includes(TOPIC_VISITS_TAB));
    assertEquals(
        TOPIC_DETAILS_TABS.length + 1,
        query('#panels')!.querySelectorAll('[role=tabpanel]').length);

    const panel = query('topic-visits-panel')!;
    assertEquals(TOPIC_VISITS_TAB, panel.getAttribute('aria-label'));
    const banner = queryIn(panel, '#banner')[0]!;
    assertTrue(banner.textContent.includes('Fishfood Experiment Only'));
    // Every visit is listed, including ones the sites list leaves out.
    assertDeepEquals(
        ['Page 1', 'Settings', 'Page 2'],
        queryIn(panel, '.visit-title').map(el => el.textContent.trim()));
  });

  test('header rating keeps the rest of the feedback', async () => {
    await createDetails([createStoredFeedback({
      rating: TopicRating.kDisliked,
      defects: [TopicDefectCategory.kDuplicateOfAnotherTopic],
      duplicateOf: {id: 'topic-2', title: 'Other topic', visitTimes: []},
      rejectedVisits: [{internalValue: 2n}],
    })]);
    const controls = query('topic-feedback-controls#fishfoodRating')!;
    assertTrue(!!controls);

    await click(queryIn(controls, '#thumbsUp')[0]!);
    const feedback = getLastSavedFeedback();
    assertEquals(TopicRating.kLiked, feedback.rating);
    assertDeepEquals([], feedback.defects);
    // The duplicate only applies to the Duplicate defect.
    assertEquals(null, feedback.duplicateOf);
    assertEquals(1, feedback.rejectedVisits.length);
    assertFalse(!!query('#duplicateOf'));
  });

  test('header thumbs down opens the reasons popover', async () => {
    await createDetails();
    const controls = query('topic-feedback-controls#fishfoodRating')!;
    assertFalse(!!queryIn(controls, '#reasonsButton')[0]);

    await click(queryIn(controls, '#thumbsDown')[0]!);
    const panel = queryIn(controls, '#defectsPanel')[0]!;
    assertTrue(panel.matches(':popover-open'));
    const button = queryIn(controls, '#reasonsButton')[0]!;
    assertEquals('Add reasons', button.textContent.trim());
    assertTrue(button.hasAttribute('invalid'));
    assertEquals('true', button.getAttribute('aria-expanded'));

    await click(queryIn(controls, 'cr-chip')[0]!);
    assertEquals('1 reason', button.textContent.trim());
    assertFalse(button.hasAttribute('invalid'));

    panel.hidePopover();
    await microtasksFinished();
    assertEquals('false', button.getAttribute('aria-expanded'));
  });

  test('picking a duplicate saves a snapshot of it', async () => {
    await createDetails([createStoredFeedback({
      rating: TopicRating.kDisliked,
      defects: [TopicDefectCategory.kDuplicateOfAnotherTopic],
    })]);
    const select = query<HTMLSelectElement>('#duplicateOf')!;
    assertDeepEquals(
        ['', 'topic-2'], Array.from(select.options).map(o => o.value));
    assertEquals('', select.value);

    select.value = 'topic-2';
    select.dispatchEvent(new Event('change'));
    await microtasksFinished();
    let feedback = getLastSavedFeedback();
    assertEquals('topic-2', feedback.duplicateOf!.id);
    assertEquals('Other topic', feedback.duplicateOf!.title);
    assertEquals(3, feedback.duplicateOf!.visitTimes.length);
    assertEquals('topic-2', select.value);

    select.value = '';
    select.dispatchEvent(new Event('change'));
    await microtasksFinished();
    feedback = getLastSavedFeedback();
    assertEquals(null, feedback.duplicateOf);
  });

  test('rates queries, storing only rated ones', async () => {
    await createDetails();
    const panel = query('topic-summary-panel')!;
    const queries = queryIn(panel, '.query');
    // Only the queries sent to Glic are rated.
    assertEquals(3, queries.length);
    assertEquals(1, queryIn(panel, '#queryRatingsHint').length);

    await click(queries[1]!.querySelector<HTMLElement>('.query-dislike')!);
    assertDeepEquals(
        [{index: 1, queryText: 'Query 2', liked: false}],
        getLastSavedFeedback().queryFeedbacks);

    await click(queries[0]!.querySelector<HTMLElement>('.query-like')!);
    assertDeepEquals(
        [
          {index: 0, queryText: 'Query 1', liked: true},
          {index: 1, queryText: 'Query 2', liked: false},
        ],
        getLastSavedFeedback().queryFeedbacks);
    assertEquals(
        'true',
        queries[0]!.querySelector('.query-like')!.getAttribute('aria-pressed'));

    // Clicking a rating again clears it, and empty feedback is deleted.
    await click(queries[0]!.querySelector<HTMLElement>('.query-like')!);
    await click(queries[1]!.querySelector<HTMLElement>('.query-dislike')!);
    assertEquals('topic-1', await handler.whenCalled('deleteTopicFeedback'));
  });

  test('clicking a query does not open Glic', async () => {
    await createDetails();
    const panel = query('topic-summary-panel')!;
    await click(queryIn(panel, '.query-text')[0]!);
    assertEquals(0, handler.getCallCount('openGlicPanel'));
    assertEquals(0, handler.getCallCount('setTopicFeedback'));
  });

  test('marks visits that do not belong', async () => {
    await createDetails();
    const panel = query('topic-visits-panel')!;
    const rejectButtons = queryIn(panel, '.reject');
    assertEquals(3, rejectButtons.length);

    await click(rejectButtons[1]!);
    assertDeepEquals(
        [2],
        getLastSavedFeedback().rejectedVisits.map(
            time => Number(time.internalValue)));
    assertTrue(queryIn(panel, '.visit')[1]!.hasAttribute('rejected'));
    assertEquals('true', rejectButtons[1]!.getAttribute('aria-pressed'));
    assertEquals(1, queryIn(panel, '.rejected-label').length);

    await click(rejectButtons[1]!);
    assertEquals('topic-1', await handler.whenCalled('deleteTopicFeedback'));
    assertFalse(queryIn(panel, '.visit')[1]!.hasAttribute('rejected'));
    assertEquals(0, queryIn(panel, '.rejected-label').length);
  });

  test('sites dialog marks sites that do not belong', async () => {
    await createDetails();
    query('#sitesButton')!.click();
    await microtasksFinished();
    const dialog = query('#sitesDialog')!;
    const rejectButtons = queryIn(dialog, '.reject');
    assertEquals(2, rejectButtons.length);

    await click(rejectButtons[0]!);
    assertDeepEquals(
        [3],
        getLastSavedFeedback().rejectedVisits.map(
            time => Number(time.internalValue)));
    assertTrue(queryIn(dialog, '#siteList li')[0]!.hasAttribute('rejected'));

    // The visits tab shows the same rejection.
    const panel = query('topic-visits-panel')!;
    assertTrue(queryIn(panel, '.visit')[0]!.hasAttribute('rejected'));
  });

  test('sites dialog keeps the toggle in view for long titles', async () => {
    handler.setResultFor('getTopic', Promise.resolve({
      topic: {
        ...topic,
        visits: [createVisit('https://example.com/', 'Long title '.repeat(30))],
      },
    }));
    await createDetails();
    query('#sitesButton')!.click();
    await microtasksFinished();
    const row = queryIn(query('#sitesDialog')!, '#siteList li')[0]!;
    const reject = row.querySelector('.reject')!;
    assertTrue(reject.getBoundingClientRect().width > 0);
    assertTrue(
        reject.getBoundingClientRect().right <=
        row.getBoundingClientRect().right);
  });

  test(
      'rates collections and flags their pages, in the page only', async () => {
        handler.setResultFor('getTopic', Promise.resolve({
          topic: {...topic, collections: [createCollection()]},
        }));
        await createDetails();
        const summary = query('topic-summary-panel')!;
        const carousel = queryIn(summary, 'topic-collection-carousel')[0] as
            TopicCollectionCarouselElement;
        const like = queryIn(carousel, '.collection-like')[0]!;

        await click(like);
        assertEquals('true', like.getAttribute('aria-pressed'));
        assertEquals(true, carousel.feedback!.liked);

        const cards = queryIn(carousel, '#cards li');
        assertEquals(2, queryIn(carousel, '.reject').length);
        await click(cards[1]!.querySelector<HTMLElement>('.reject')!);
        assertTrue(cards[1]!.hasAttribute('rejected'));
        assertTrue(!!cards[1]!.querySelector('.rejected-label'));
        assertDeepEquals(
            ['https://www.example.org/item-2'],
            carousel.feedback!.rejectedItemUrls);

        // Clicking again undoes both.
        await click(like);
        await click(cards[1]!.querySelector<HTMLElement>('.reject')!);
        assertEquals('false', like.getAttribute('aria-pressed'));
        assertFalse(cards[1]!.hasAttribute('rejected'));
        // Not stored until the mojom has fields for it.
        assertEquals(0, handler.getCallCount('setTopicFeedback'));
      });
});

suite('TopicCollectionCarousel', () => {
  let carousel: TopicCollectionCarouselElement;
  let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;
  let openWindowProxy: TestOpenWindowProxy;

  // A bundled image, so that it loads in the test without a network request
  // and doesn't trigger the favicon fallback on its own.
  const IMAGE_URL = 'chrome://resources/images/add.svg';

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({kTopicsFishfoodFeedback: false});
    handler = TestMock.fromClass(PageHandlerRemote);
    const {instance} = browserProxyFactory.createForTest(handler);
    browserProxyFactory.setInstance(instance);
    handler.setResultFor(
        'getTopicPageImageUrl', Promise.resolve({imageUrl: null}));
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  async function createCarousel(collection: Collection) {
    carousel = document.createElement('topic-collection-carousel');
    carousel.style.width = '600px';
    carousel.collection = collection;
    document.body.appendChild(carousel);
    await microtasksFinished();
  }

  function createItems(count: number): Collection['items'] {
    const items: Collection['items'] = [];
    for (let i = 0; i < count; i++) {
      items.push({
        title: `Page ${i}`,
        url: `https://site${i}.com/`,
        siteName: `Site ${i}`,
      });
    }
    return items;
  }

  function query(selector: string): HTMLElement|null {
    return carousel.shadowRoot.querySelector<HTMLElement>(selector);
  }

  function getCardImages(): HTMLElement[] {
    return Array.from(
        carousel.shadowRoot.querySelectorAll<HTMLElement>('.card-image'));
  }

  test('renders the title and a card per item', async () => {
    await createCarousel({title: 'Collection title', items: createItems(2)});

    assertEquals('Collection title', query('#title')!.textContent.trim());
    const cards = carousel.shadowRoot.querySelectorAll('.card');
    assertEquals(2, cards.length);
    assertEquals(
        'Page 1', cards[1]!.querySelector('.card-title')!.textContent.trim());
    assertEquals(
        'Site 1', cards[1]!.querySelector('.card-site')!.textContent.trim());
  });

  test('shows the page image, or the favicon without one', async () => {
    handler.setResultMapperFor(
        'getTopicPageImageUrl',
        (url: string) => Promise.resolve(
            {imageUrl: url === 'https://site0.com/' ? IMAGE_URL : null}));
    await createCarousel({title: 'Collection title', items: createItems(2)});
    await microtasksFinished();

    assertDeepEquals(
        ['https://site0.com/', 'https://site1.com/'],
        handler.getArgs('getTopicPageImageUrl'));
    const [withImage, withoutImage] = getCardImages();
    assertEquals(
        IMAGE_URL,
        withImage!.querySelector('.card-page-image')!.getAttribute('auto-src'));
    assertFalse(!!withImage!.querySelector('.card-favicon'));
    assertFalse(!!withoutImage!.querySelector('.card-page-image'));
    assertTrue(!!withoutImage!.querySelector('.card-favicon'));
  });

  test('falls back to the favicon when the image fails', async () => {
    handler.setResultFor(
        'getTopicPageImageUrl', Promise.resolve({imageUrl: IMAGE_URL}));
    await createCarousel({title: 'Collection title', items: createItems(1)});
    await microtasksFinished();

    const cardImage = getCardImages()[0]!;
    cardImage.querySelector('.card-page-image')!.dispatchEvent(
        new Event('error'));
    await microtasksFinished();
    assertFalse(!!cardImage.querySelector('.card-page-image'));
    assertTrue(!!cardImage.querySelector('.card-favicon'));
  });

  test('ignores images for a replaced collection', async () => {
    const resolver = new PromiseResolver<{imageUrl: string | null}>();
    handler.setResultFor('getTopicPageImageUrl', resolver.promise);
    await createCarousel({title: 'Collection 1', items: createItems(1)});

    handler.setResultFor(
        'getTopicPageImageUrl', Promise.resolve({imageUrl: null}));
    carousel.collection = {title: 'Collection 2', items: createItems(1)};
    await microtasksFinished();
    resolver.resolve({imageUrl: IMAGE_URL});
    await microtasksFinished();
    assertEquals(2, handler.getCallCount('getTopicPageImageUrl'));
    assertFalse(!!query('.card-page-image'));
  });

  test('clicking a card opens its page', async () => {
    await createCarousel({title: 'Collection title', items: createItems(2)});

    carousel.shadowRoot.querySelectorAll<HTMLElement>('.card')[1]!.click();
    assertEquals(
        'https://site1.com/', await openWindowProxy.whenCalled('openUrl'));
  });

  test('three cards fit, even with a long title', async () => {
    const items = createItems(3);
    items[2]!.title = 'A long title '.repeat(10);
    await createCarousel({title: 'Collection title', items});

    assertTrue(query('#scrollButtons')!.hidden);
    const cards = query('#cards')!;
    assertTrue(cards.scrollWidth <= cards.clientWidth + 1);
    const widths = Array.from(carousel.shadowRoot.querySelectorAll('li'))
                       .map(li => Math.round(li.getBoundingClientRect().width));
    assertEquals(1, new Set(widths).size);
  });

  test('shows the scroll buttons when the cards overflow', async () => {
    await createCarousel({title: 'Collection title', items: createItems(6)});
    assertFalse(query('#scrollButtons')!.hidden);
  });

  // Records where the cards were asked to scroll to, or by.
  function stubScrolling(cards: HTMLElement) {
    const calls: Array<{method: string, left: number}> = [];
    cards.scrollTo = ((options: ScrollToOptions) => {
                       calls.push({method: 'scrollTo', left: options.left!});
                     }) as typeof cards.scrollTo;
    cards.scrollBy = ((options: ScrollToOptions) => {
                       calls.push({method: 'scrollBy', left: options.left!});
                     }) as typeof cards.scrollBy;
    return calls;
  }

  test('scroll buttons wrap around at either end', async () => {
    await createCarousel({title: 'Collection title', items: createItems(6)});
    const cards = query('#cards')!;
    const calls = stubScrolling(cards);
    const lastPage = cards.scrollWidth - cards.clientWidth;

    // At the start, back wraps to the end and forward scrolls a page.
    query('#back')!.click();
    query('#forward')!.click();
    assertDeepEquals(
        [
          {method: 'scrollTo', left: lastPage},
          {method: 'scrollBy', left: cards.clientWidth},
        ],
        calls);

    // At the end, forward wraps to the start.
    calls.length = 0;
    cards.scrollLeft = lastPage;
    cards.dispatchEvent(new Event('scroll'));
    await microtasksFinished();
    query('#forward')!.click();
    assertDeepEquals([{method: 'scrollTo', left: 0}], calls);
  });
});
