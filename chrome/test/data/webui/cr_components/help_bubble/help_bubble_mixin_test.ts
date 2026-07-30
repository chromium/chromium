// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import './help_bubble_mixin_lit_test_element.js';
import './help_bubble_mixin_test_element.js';

import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import type {TrackedElementProxy} from '//resources/js/tracked_element/tracked_element_proxy.js';
import {TrackedElementProxyImpl} from '//resources/js/tracked_element/tracked_element_proxy.js';
import type {HelpBubbleClientRemote, HelpBubbleHandlerInterface, HelpBubbleParams} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {browserProxyFactory, HelpBubbleArrowPosition, HelpBubbleClosedReason} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {ANCHOR_HIGHLIGHT_CLASS} from 'chrome://resources/cr_components/help_bubble/help_bubble_controller.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {TrackedElementManagerCallbackRouter} from 'chrome://resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import type {TrackedElementHandlerInterface, TrackedElementHandlerPendingReceiver, TrackedElementIdentifier, TrackedElementManagerRemote} from 'chrome://resources/mojo/ui/webui/resources/js/tracked_element/tracked_element.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import type {HelpBubbleMixinLitTestElement} from './help_bubble_mixin_lit_test_element.js';
import type {HelpBubbleMixinTestElement} from './help_bubble_mixin_test_element.js';

const TITLE_NATIVE_ID: string = 'kHelpBubbleMixinTestTitleElementId';
const PARAGRAPH_NATIVE_ID: string = 'kHelpBubbleMixinTestParagraphElementId';
const LIST_NATIVE_ID: string = 'kHelpBubbleMixinTestListElementId';
const SPAN_NATIVE_ID: string = 'kHelpBubbleMixinTestSpanElementId';
const LIST_ITEM_NATIVE_ID: string = 'kHelpBubbleMixinTestListItemElementId';
const NESTED_CHILD_NATIVE_ID: string = 'kHelpBubbleMixinTestChildElementId';
const EVENT1_NAME: string = 'kFirstExampleCustomEvent';
const EVENT2_NAME: string = 'kSecondExampleCustomEvent';
const CLOSE_BUTTON_ALT_TEXT: string = 'Close help bubble.';
const BODY_ICON_ALT_TEXT: string = 'Icon help bubble.';
const CUSTOM_CONTAINER_NATIVE_ID: string =
    'kHelpBubbleMixinTestCustomContainerElementId';
const UNKNOWN_SECONDARY_ID: string = '0';

class TestTrackedElementHandler extends TestBrowserProxy implements
    TrackedElementHandlerInterface {
  // Records the current visibility of all known elements.
  // Simply looking at the call logs can produce extraneous results, as
  // visible=true may be generated multiple times if an element e.g. changes
  // position on the page.
  visibility: Map<string, boolean> = new Map();
  constructor() {
    super([
      'setManager',
      'trackedElementVisibilityChanged',
      'trackedElementActivated',
      'trackedElementCustomEvent',
      'trackedElementCanHighlightChanged',
    ]);
  }

  setManager(_: TrackedElementManagerRemote) {
    this.methodCalled('setManager');
  }

  trackedElementVisibilityChanged(
      id: TrackedElementIdentifier, visible: boolean) {
    this.visibility.set(id.nativeIdentifier, visible);
    this.methodCalled(
        'trackedElementVisibilityChanged', id.nativeIdentifier, visible);
  }

  trackedElementActivated(id: TrackedElementIdentifier) {
    this.methodCalled('trackedElementActivated', id.nativeIdentifier);
  }

  trackedElementCustomEvent(id: TrackedElementIdentifier, eventName: string) {
    this.methodCalled(
        'trackedElementCustomEvent', id.nativeIdentifier, eventName);
  }

  trackedElementCanHighlightChanged(
      id: TrackedElementIdentifier, canHighlight: boolean) {
    this.methodCalled(
        'trackedElementCanHighlightChanged', id.nativeIdentifier, canHighlight);
  }
}

class TestHelpBubbleHandler extends TestBrowserProxy implements
    HelpBubbleHandlerInterface {
  constructor() {
    super([
      'helpBubbleButtonPressed',
      'helpBubbleClosed',
    ]);
  }

  bindTrackedElementHandler(handler: TrackedElementHandlerPendingReceiver) {
    this.methodCalled('bindTrackedElementHandler', handler);
  }

  helpBubbleButtonPressed(id: TrackedElementIdentifier, button: number) {
    this.methodCalled('helpBubbleButtonPressed', id, button);
  }

  helpBubbleClosed(
      id: TrackedElementIdentifier, reason: HelpBubbleClosedReason) {
    this.methodCalled('helpBubbleClosed', id, reason);
  }
}

class TestTrackedElementProxy implements TrackedElementProxy {
  private handler_: TrackedElementHandlerInterface;
  callbackRouter: TrackedElementManagerCallbackRouter =
      new TrackedElementManagerCallbackRouter();

  constructor(handler: TrackedElementHandlerInterface) {
    this.handler_ = handler;
  }
  getHandler(): TrackedElementHandlerInterface {
    return this.handler_;
  }
}

interface WaitForSuccessParams {
  retryIntervalMs: number;
  totalMs: number;
  assertionFn: () => void;
}

enum Version {
  POLYMER = 'POLYMER',
  LIT = 'LIT'
}

[Version.POLYMER, Version.LIT].forEach(version => {
  suite(`CrComponentsHelpBubbleMixinTest/${version}`, () => {
    let callbackRouterRemote: HelpBubbleClientRemote;
    let mockHandler: TestHelpBubbleHandler;
    let testTrackedElementHandler: TestTrackedElementHandler;
    let container: HelpBubbleMixinTestElement|HelpBubbleMixinLitTestElement;

    function getId(nativeIdentifier: string): TrackedElementIdentifier {
      const elements: HTMLElement[] =
          TrackedElementManager.getInstance().getAllElementsWithId(
              nativeIdentifier);
      let secondaryIdentifier = UNKNOWN_SECONDARY_ID;
      if (elements.length) {
        const element: HTMLElement = elements[0]!;
        const id = TrackedElementManager.getElementId(element);
        if (!id) {
          console.warn(
              'Invalid or missing secondary ID for element "', nativeIdentifier,
              '"');
        } else {
          secondaryIdentifier = id.secondaryIdentifier;
        }
      } else {
        console.warn('No matching HTML element for "', nativeIdentifier, '"');
      }
      return {nativeIdentifier, secondaryIdentifier};
    }

    // Makes a default set of help bubble params with the given overrides (if
    // specified).
    function makeParams(overrides: Partial<HelpBubbleParams> = {}) {
      return Object.assign(
          {
            id: {
              nativeIdentifier: PARAGRAPH_NATIVE_ID,
              secondaryIdentifier: UNKNOWN_SECONDARY_ID,
            },
            closeButtonAltText: CLOSE_BUTTON_ALT_TEXT,
            position: HelpBubbleArrowPosition.BOTTOM_CENTER,
            bodyText: 'This is a help bubble.',
            bodyIconName: 'lightbulb_outline',
            bodyIconAltText: BODY_ICON_ALT_TEXT,
            buttons: [],
            focusOnShowHint: null,
            titleText: null,
            progress: null,
            timeout: null,
          },
          overrides);
    }

    /**
     * Waits for the current frame to render, which queues intersection events,
     * and then waits for the intersection events to propagate to listeners,
     * which triggers visibility messages.
     *
     * This takes a total of two frames. A single frame will cause the layout to
     * be updated, but will not actually propagate the events.
     */
    function waitForVisibilityEvents() {
      return new Promise<void>(resolve => {
        requestAnimationFrame(async () => {
          await sleep(1);
          resolve();
        });
      });
    }

    /**
     * Create a promise that resolves after a given amount of time
     */
    async function sleep(milliseconds: number) {
      return new Promise((res) => {
        setTimeout(res, milliseconds);
      });
    }

    /**
     * Returns the current timestamp in milliseconds since UNIX epoch
     */
    function now() {
      return +new Date();
    }

    /**
     * Try/catch a function for some time, retrying after failures
     *
     * If the callback function succeeds, return early with the total time
     * If the callback always fails, throw the error after the last run
     */
    async function waitForSuccess(params: WaitForSuccessParams):
        Promise<number|null> {
      const startMs = now();
      let lastAttemptMs = startMs;
      let lastError: Error|null = null;
      let attempts = 0;
      while (now() - startMs < params.totalMs) {
        await sleep(params.retryIntervalMs);
        lastAttemptMs = now();
        try {
          params.assertionFn();
          return lastAttemptMs - startMs;
        } catch (e) {
          lastError = e as Error;
        }
        attempts++;
      }
      if (lastError !== null) {
        lastError.message = `[Attempts: ${attempts}, Total time: ${
            lastAttemptMs - startMs}ms]: ${lastError.message}`;
        throw lastError;
      }
      return Infinity;
    }

    setup(async () => {
      mockHandler = new TestHelpBubbleHandler();
      const {instance, remote} = browserProxyFactory.createForTest(mockHandler);
      callbackRouterRemote = remote;
      browserProxyFactory.setInstance(instance);

      testTrackedElementHandler = new TestTrackedElementHandler();
      TrackedElementProxyImpl.setInstance(
          new TestTrackedElementProxy(testTrackedElementHandler));

      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      switch (version) {
        case Version.POLYMER:
          container = document.createElement('help-bubble-mixin-test');
          break;
        case Version.LIT:
          container = document.createElement('help-bubble-mixin-lit-test');
          break;
        default:
          assertNotReachedCase(version);
      }
      document.body.appendChild(container);
      await microtasksFinished();

      const spanEl = container.shadowRoot!.querySelector('span');
      assertTrue(spanEl !== null, 'connectedCallback: span element exists');

      assertTrue(container.registerHelpBubble(TITLE_NATIVE_ID, '#title'));
      assertTrue(container.registerHelpBubble(PARAGRAPH_NATIVE_ID, '#p1'));
      assertTrue(container.registerHelpBubble(LIST_NATIVE_ID, '#bulletList'));
      assertTrue(container.registerHelpBubble(SPAN_NATIVE_ID, spanEl));

      // using different types of selectors to test query mechanism
      assertTrue(container.registerHelpBubble(
          NESTED_CHILD_NATIVE_ID, ['#container-element', '.child-element']));

      const customContainer =
          container.shadowRoot!.querySelector<HTMLElement>('#custom-container');
      assertTrue(
          customContainer !== null,
          'connectedCallback: custom container exists');
      assertTrue(container.registerHelpBubble(
          CUSTOM_CONTAINER_NATIVE_ID, '#custom-anchor',
          {containerElement: customContainer}));

      return waitForVisibilityEvents();
    });

    test('reports bubble closed', () => {
      assertFalse(container.isHelpBubbleShowing());
    });

    test('shows bubble when called directly', () => {
      assertFalse(container.isHelpBubbleShowing());
      assertFalse(container.isHelpBubbleShowingForTesting('p1'));
      container.showHelpBubble(makeParams({id: getId(PARAGRAPH_NATIVE_ID)}));
      assertTrue(container.isHelpBubbleShowing());
      assertTrue(container.isHelpBubbleShowingForTesting('p1'));
    });

    test('shows bubble anchored to arbitrary HTMLElment', () => {
      assertFalse(container.isHelpBubbleShowing());
      container.showHelpBubble(makeParams({id: getId(SPAN_NATIVE_ID)}));
      assertTrue(container.isHelpBubbleShowing());
      assertTrue(container.isHelpBubbleShowingForTesting(SPAN_NATIVE_ID));
    });

    test('shows bubble attached to custom containerElement', () => {
      assertFalse(container.isHelpBubbleShowing());
      container.showHelpBubble(
          makeParams({id: getId(CUSTOM_CONTAINER_NATIVE_ID)}));
      assertTrue(container.isHelpBubbleShowing());
      assertTrue(
          container.isHelpBubbleShowingForTesting(CUSTOM_CONTAINER_NATIVE_ID));
      const bubble = container.getHelpBubbleForTesting('custom-anchor');
      assertTrue(!!bubble, 'bubble exists');
      assertEquals(
          container.shadowRoot!.querySelector('#custom-container'),
          bubble.parentElement, 'bubble is child of custom container');
    });

    test('can pierce shadow dom to anchor to deep query', () => {
      const containerElement =
          container.shadowRoot!.querySelector('#container-element');
      let childElement: HTMLElement|null =
          container.shadowRoot!.querySelector('.child-element');

      assertTrue(containerElement !== null, 'container element is found');
      assertTrue(
          childElement === null, 'child element is isolated from container');

      childElement =
          containerElement.shadowRoot!.querySelector('.child-element');
      assertTrue(
          childElement !== null, 'child element is rendered in shadow dom');

      assertFalse(container.isHelpBubbleShowing());
      container.showHelpBubble(makeParams({id: getId(NESTED_CHILD_NATIVE_ID)}));
      assertTrue(container.isHelpBubbleShowing());
      const bubble = container.getHelpBubbleForTesting(childElement);
      assert(bubble);
      assert(bubble.getAnchorElement());
      assertEquals(childElement, bubble.getAnchorElement());
    });

    test('reports not open for other elements', () => {
      // Valid but not open.
      assertFalse(container.isHelpBubbleShowingForTesting('title'));
      // Not valid (and not open).
      assertFalse(container.isHelpBubbleShowingForTesting('foo'));
    });

    test('hides bubble when called directly', () => {
      container.showHelpBubble(makeParams({id: getId(PARAGRAPH_NATIVE_ID)}));
      assertTrue(container.hideHelpBubble(PARAGRAPH_NATIVE_ID));
      assertFalse(container.isHelpBubbleShowing());
    });

    test('called directly doesn\'t hide wrong bubble', () => {
      container.showHelpBubble(makeParams({id: getId(PARAGRAPH_NATIVE_ID)}));
      assertFalse(container.hideHelpBubble(TITLE_NATIVE_ID));
      assertTrue(container.isHelpBubbleShowing());
    });

    test('show and hide multiple bubbles directly', () => {
      container.showHelpBubble(makeParams({id: getId(PARAGRAPH_NATIVE_ID)}));
      assertTrue(container.isHelpBubbleShowingForTesting('p1'));
      assertFalse(container.isHelpBubbleShowingForTesting('title'));
      assertTrue(container.isHelpBubbleShowing());

      container.showHelpBubble(makeParams({id: getId(TITLE_NATIVE_ID)}));
      assertTrue(container.isHelpBubbleShowingForTesting('p1'));
      assertTrue(container.isHelpBubbleShowingForTesting('title'));
      assertTrue(container.isHelpBubbleShowing());

      container.hideHelpBubble(PARAGRAPH_NATIVE_ID);
      assertFalse(container.isHelpBubbleShowingForTesting('p1'));
      assertTrue(container.isHelpBubbleShowingForTesting('title'));
      assertTrue(container.isHelpBubbleShowing());

      container.hideHelpBubble(TITLE_NATIVE_ID);
      assertFalse(container.isHelpBubbleShowingForTesting('p1'));
      assertFalse(container.isHelpBubbleShowingForTesting('title'));
      assertFalse(container.isHelpBubbleShowing());
    });

    test('shows help bubble when called via proxy', async () => {
      callbackRouterRemote.showHelpBubble(makeParams());
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing(), 'a bubble is showing');
      const bubble = container.getHelpBubbleForTesting('p1');
      assertTrue(!!bubble, 'bubble exists');
      assertEquals(
          container.$.p1, bubble.getAnchorElement(),
          'bubble has correct anchor');
      assertTrue(isVisible(bubble), 'bubble is visible');
    });

    test('uses close button alt text', async () => {
      callbackRouterRemote.showHelpBubble(makeParams());
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      const bubble = container.getHelpBubbleForTesting('p1')!;
      assert(bubble);
      const closeButton =
          bubble.shadowRoot.querySelector<HTMLElement>('#close');
      assertTrue(!!closeButton);
      assertEquals(
          CLOSE_BUTTON_ALT_TEXT, closeButton.getAttribute('aria-label'));
    });

    test('uses body icon', async () => {
      const params = makeParams();
      callbackRouterRemote.showHelpBubble(params);
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      const bubble = container.getHelpBubbleForTesting('p1')!;
      assert(bubble);
      assertEquals(bubble.bodyIconName, params.bodyIconName);
      const bodyIcon =
          bubble.shadowRoot.querySelector<HTMLElement>('#bodyIcon');
      assertTrue(!!bodyIcon);
      const ironIcon = bodyIcon.querySelector('cr-icon');
      assertTrue(!!ironIcon);
      assertEquals(`iph:${params.bodyIconName}`, ironIcon.icon);
    });

    test('does not use body icon when not defined', async () => {
      const noIconParams = makeParams({bodyIconName: null});
      callbackRouterRemote.showHelpBubble(noIconParams);
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      const bubble = container.getHelpBubbleForTesting('p1')!;
      assert(bubble);
      assertEquals(bubble.bodyIconName, null);
      const bodyIcon =
          bubble.shadowRoot.querySelector<HTMLElement>('#bodyIcon');
      assertTrue(!!bodyIcon);
      assertTrue(bodyIcon.hidden);
    });

    test('hides help bubble when called via proxy', async () => {
      const params = makeParams();
      callbackRouterRemote.showHelpBubble(params);
      await microtasksFinished();
      callbackRouterRemote.hideHelpBubble(params.id);
      await microtasksFinished();
      assertFalse(container.isHelpBubbleShowing());
    });

    test('adds class to element on external help bubble shown', async () => {
      callbackRouterRemote.externalHelpBubbleUpdated(
          getId(TITLE_NATIVE_ID), true);
      await microtasksFinished();
      assertTrue(container.$.title.classList.contains(ANCHOR_HIGHLIGHT_CLASS));
      callbackRouterRemote.externalHelpBubbleUpdated(
          getId(TITLE_NATIVE_ID), false);
      await microtasksFinished();
      assertFalse(container.$.title.classList.contains(ANCHOR_HIGHLIGHT_CLASS));
    });

    test('doesn\'t hide help bubble when called with wrong id', async () => {
      callbackRouterRemote.showHelpBubble(makeParams());
      await microtasksFinished();
      callbackRouterRemote.hideHelpBubble(getId(LIST_NATIVE_ID));
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
    });

    test('ignores unregistered ID in ShowHelpBubble call', async () => {
      const params = makeParams({
        id: getId('This is an unregistered identifier'),
      });

      callbackRouterRemote.showHelpBubble(params);
      await microtasksFinished();
      assertFalse(container.isHelpBubbleShowing());
    });

    test('ignores unregistered ID in HideHelpBubble call', async () => {
      callbackRouterRemote.showHelpBubble(makeParams());
      await microtasksFinished();
      callbackRouterRemote.hideHelpBubble(
          getId('This is an unregistered identifier'));
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
    });

    test('ignores unregistered ID in focus call', async () => {
      callbackRouterRemote.showHelpBubble(makeParams());
      await microtasksFinished();
      callbackRouterRemote.toggleFocusForAccessibility(
          getId('This is an unregistered identifier'));
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
    });

    test('sends events on initially visible', async () => {
      await microtasksFinished();
      assertDeepEquals(
          new Map<string, boolean>([
            [TITLE_NATIVE_ID, true],
            [PARAGRAPH_NATIVE_ID, true],
            [LIST_NATIVE_ID, true],
            [SPAN_NATIVE_ID, true],
            [NESTED_CHILD_NATIVE_ID, true],
            [CUSTOM_CONTAINER_NATIVE_ID, true],
          ]),
          testTrackedElementHandler.visibility);
    });

    test('sends event on lost visibility', async () => {
      await microtasksFinished();
      container.style.display = 'none';
      await waitForVisibilityEvents();
      assertDeepEquals(
          new Map<string, boolean>([
            [TITLE_NATIVE_ID, false],
            [PARAGRAPH_NATIVE_ID, false],
            [LIST_NATIVE_ID, false],
            [SPAN_NATIVE_ID, false],
            [NESTED_CHILD_NATIVE_ID, false],
            [CUSTOM_CONTAINER_NATIVE_ID, false],
          ]),
          testTrackedElementHandler.visibility);
    });

    test('sends event on element activated', async () => {
      container.showHelpBubble(makeParams({id: getId(TITLE_NATIVE_ID)}));
      container.showHelpBubble(makeParams({id: getId(LIST_NATIVE_ID)}));
      await microtasksFinished();
      container.notifyHelpBubbleAnchorActivated(LIST_NATIVE_ID);
      container.notifyHelpBubbleAnchorActivated(TITLE_NATIVE_ID);
      assertEquals(
          2, testTrackedElementHandler.getCallCount('trackedElementActivated'));
      assertDeepEquals(
          [LIST_NATIVE_ID, TITLE_NATIVE_ID],
          testTrackedElementHandler.getArgs('trackedElementActivated'));
    });

    test('sends custom events', async () => {
      container.showHelpBubble(makeParams({id: getId(PARAGRAPH_NATIVE_ID)}));
      container.showHelpBubble(makeParams({id: getId(TITLE_NATIVE_ID)}));
      await microtasksFinished();
      container.notifyHelpBubbleAnchorCustomEvent(
          PARAGRAPH_NATIVE_ID, EVENT1_NAME);
      container.notifyHelpBubbleAnchorCustomEvent(TITLE_NATIVE_ID, EVENT2_NAME);
      assertEquals(
          2,
          testTrackedElementHandler.getCallCount('trackedElementCustomEvent'));
      assertDeepEquals(
          [
            [PARAGRAPH_NATIVE_ID, EVENT1_NAME],
            [TITLE_NATIVE_ID, EVENT2_NAME],
          ],
          testTrackedElementHandler.getArgs('trackedElementCustomEvent'));
    });

    test('sends event on closed due to anchor losing visibility', async () => {
      const id = getId(PARAGRAPH_NATIVE_ID);
      container.showHelpBubble(makeParams({id}));

      // Hiding the container will cause the bubble to be closed.
      container.$.p1.style.display = 'none';
      await waitForVisibilityEvents();

      assertEquals(1, mockHandler.getCallCount('helpBubbleClosed'));
      const expected = [[id, HelpBubbleClosedReason.kPageChanged]];
      const actual = mockHandler.getArgs('helpBubbleClosed');
      assertDeepEquals(expected, actual);
      assertFalse(container.isHelpBubbleShowing());
    });

    test('does not send event when non-anchor loses visibility', async () => {
      container.showHelpBubble(makeParams({id: getId(PARAGRAPH_NATIVE_ID)}));

      // This is not the current bubble anchor, so should not send an event.
      container.$.title.style.display = 'none';
      await waitForVisibilityEvents();
      assertEquals(0, mockHandler.getCallCount('helpBubbleClosed'));
      assertTrue(container.isHelpBubbleShowing());
    });

    test('does not timeout by default', async () => {
      container.showHelpBubble(makeParams({id: getId(PARAGRAPH_NATIVE_ID)}));

      // This is not the current bubble anchor, so should not send an event.
      container.$.title.style.display = 'none';
      await waitForVisibilityEvents();
      assertEquals(0, mockHandler.getCallCount('helpBubbleClosed'));
      assertTrue(container.isHelpBubbleShowing());
      await sleep(100);  // 100ms
      assertEquals(0, mockHandler.getCallCount('helpBubbleClosed'));
      assertTrue(container.isHelpBubbleShowing());
    });

    test('reshow bubble', async () => {
      const params = makeParams();
      callbackRouterRemote.showHelpBubble(params);
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      callbackRouterRemote.hideHelpBubble(params.id);
      await microtasksFinished();
      assertFalse(container.isHelpBubbleShowing());
      callbackRouterRemote.showHelpBubble(params);
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      const bubble = container.getHelpBubbleForTesting('p1');
      assertTrue(!!bubble);
      assertEquals(container.$.p1, bubble.getAnchorElement());
      assertTrue(isVisible(bubble));
    });

    test('shows multiple bubbles', async () => {
      callbackRouterRemote.showHelpBubble(makeParams());
      await microtasksFinished();
      callbackRouterRemote.showHelpBubble(makeParams({
        id: getId(TITLE_NATIVE_ID),
        titleText: 'This is a title',
        position: HelpBubbleArrowPosition.TOP_CENTER,
      }));
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      const bubble1 = container.getHelpBubbleForTesting('title');
      const bubble2 = container.getHelpBubbleForTesting('p1');
      assertTrue(!!bubble1);
      assertTrue(!!bubble2);
      assertEquals(container.$.title, bubble1.getAnchorElement());
      assertEquals(container.$.p1, bubble2.getAnchorElement());
      assertTrue(isVisible(bubble1));
      assertTrue(isVisible(bubble2));
    });

    test('shows bubbles with and without title', async () => {
      callbackRouterRemote.showHelpBubble(makeParams());
      await microtasksFinished();
      const paramsWithTitle = makeParams({
        id: getId(TITLE_NATIVE_ID),
        titleText: 'This is a title',
        position: HelpBubbleArrowPosition.TOP_CENTER,
      });
      callbackRouterRemote.showHelpBubble(paramsWithTitle);
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      const titleBubble = container.getHelpBubbleForTesting('title')!;
      const paragraphBubble = container.getHelpBubbleForTesting('p1')!;
      // Testing that setting `titleText` will cause the title to display
      // correctly is present in help_bubble_test.ts, so it is sufficient to
      // verify that the property is set correctly.
      assertEquals('', paragraphBubble.titleText);
      assertEquals(paramsWithTitle.titleText, titleBubble.titleText);
    });

    test('shows bubbles with and without progress', async () => {
      const paramsWithProgress = makeParams({
        id: getId(LIST_NATIVE_ID),
        position: HelpBubbleArrowPosition.TOP_CENTER,
        progress: {current: 1, total: 3},
      });

      callbackRouterRemote.showHelpBubble(makeParams());
      await microtasksFinished();
      callbackRouterRemote.showHelpBubble(paramsWithProgress);
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      const paragraphBubble = container.getHelpBubbleForTesting('p1')!;
      const progressBubble = container.getHelpBubbleForTesting('bulletList')!;
      // Testing that setting `progress` will cause the progress to display
      // correctly is present in help_bubble_test.ts, so it is sufficient to
      // verify that the property is set correctly.
      assertFalse(!!paragraphBubble.progress);
      assertDeepEquals(paramsWithProgress.progress, progressBubble.progress);
    });

    test('hides multiple bubbles', async () => {
      const params = makeParams();
      callbackRouterRemote.showHelpBubble(params);
      await microtasksFinished();
      const paramsWithTitle = makeParams({
        id: getId(TITLE_NATIVE_ID),
        titleText: 'This is a title',
        position: HelpBubbleArrowPosition.TOP_CENTER,
      });
      callbackRouterRemote.showHelpBubble(paramsWithTitle);
      await microtasksFinished();

      callbackRouterRemote.hideHelpBubble(params.id);
      await microtasksFinished();
      assertTrue(container.isHelpBubbleShowing());
      assertEquals(
          container.$.title,
          container.getHelpBubbleForTesting('title')?.getAnchorElement());
      assertEquals(null, container.getHelpBubbleForTesting('p1'));

      callbackRouterRemote.hideHelpBubble(paramsWithTitle.id);
      await microtasksFinished();
      assertFalse(container.isHelpBubbleShowing());
      assertEquals(null, container.getHelpBubbleForTesting('title'));
      assertEquals(null, container.getHelpBubbleForTesting('p1'));
    });

    test('sends event on closed via button', async () => {
      const id = getId(PARAGRAPH_NATIVE_ID);
      container.showHelpBubble(makeParams({id}));

      // Click the close button.
      container.shadowRoot!.querySelector('help-bubble')!.$.close.click();
      await waitForVisibilityEvents();
      assertEquals(1, mockHandler.getCallCount('helpBubbleClosed'));
      assertDeepEquals(
          [[id, HelpBubbleClosedReason.kDismissedByUser]],
          mockHandler.getArgs('helpBubbleClosed'));
      assertFalse(container.isHelpBubbleShowing());
    });

    test('sends action button clicked event', async () => {
      const id = getId(PARAGRAPH_NATIVE_ID);
      const buttonParams = makeParams({
        id,
        position: HelpBubbleArrowPosition.TOP_CENTER,
        buttons: [
          {
            text: 'button1',
            isDefault: false,
          },
          {
            text: 'button2',
            isDefault: true,
          },
        ],
      });

      container.showHelpBubble(buttonParams);
      await microtasksFinished();

      // Click one of the action buttons.
      const button =
          container.shadowRoot!.querySelector(
                                   'help-bubble')!.getButtonForTesting(1);
      assertTrue(!!button);
      button.click();
      await waitForVisibilityEvents();
      assertEquals(1, mockHandler.getCallCount('helpBubbleButtonPressed'));
      assertDeepEquals(
          [[id, 1]], mockHandler.getArgs('helpBubbleButtonPressed'));
      assertFalse(container.isHelpBubbleShowing());
    });

    // It is hard to guarantee the correct timing on various test systems,
    // so the 'before timeout' and 'after timeout' tests are split
    // into 2 separate fixtures

    // Before timeout
    // Use a long timeout to test base state that a timeout will
    // not be accidentally triggered when a timeout is set
    test('does not immediately timeout', async () => {
      const longTimeoutParams = makeParams({
        timeout: {
          microseconds: BigInt(10 * 1000 * 1000),  // 10s
        },
      });

      container.showHelpBubble(longTimeoutParams);
      await microtasksFinished();
      assertEquals(
          0, mockHandler.getCallCount('helpBubbleClosed'),
          'helpBubbleClosed has not be called');
      assertTrue(container.isHelpBubbleShowing());
    });

    // After timeout
    // Use a short timeout and a retry loop to
    test('sends timeout event', async () => {
      const id = getId(PARAGRAPH_NATIVE_ID);
      const timeoutMs = 100;
      const shortTimeoutParams = makeParams({
        id,
        timeout: {
          microseconds: BigInt(timeoutMs * 1000),  // 100ms
        },
      });

      container.showHelpBubble(shortTimeoutParams);
      await microtasksFinished();
      await waitForSuccess({
        retryIntervalMs: 50,
        totalMs: 1500,
        assertionFn: () => assertEquals(
            1, mockHandler.getCallCount('helpBubbleClosed'),
            'helpBubbleClosed has been called'),
      }) as number;
      assertDeepEquals(
          [[id, HelpBubbleClosedReason.kTimedOut]],
          mockHandler.getArgs('helpBubbleClosed'),
          'helpBubbleClosed is called with correct arguments');
      assertFalse(container.isHelpBubbleShowing(), 'no bubbles are showing');
    });

    test('can unregister', () => {
      let listItemBubble =
          container.registerHelpBubble(LIST_ITEM_NATIVE_ID, '#bulletList');
      assertTrue(listItemBubble !== null, 'help bubble is registered');
      assertTrue(
          container.canShowHelpBubble(LIST_ITEM_NATIVE_ID),
          'help bubble can be shown');

      // re-register when help bubble is not showing
      listItemBubble =
          container.registerHelpBubble(LIST_ITEM_NATIVE_ID, '#list-item');
      assertTrue(
          listItemBubble !== null,
          'help bubble can be re-registered with same nativeId');
      assertTrue(
          container.canShowHelpBubble(LIST_ITEM_NATIVE_ID),
          'help bubble can be shown after re-registering');

      // un-register directly when help bubble is not showing
      container.unregisterHelpBubble(LIST_ITEM_NATIVE_ID);
      assertFalse(
          container.canShowHelpBubble(LIST_ITEM_NATIVE_ID),
          'help bubble cannot be shown');
      // unregisterHelpBubble clears out the nativeIds
      assertThrows(
          () => container.showHelpBubble(
              makeParams({id: getId(LIST_ITEM_NATIVE_ID)})),
          'Can\'t show help bubble',
      );
    });

    test('can unregister when bubble is showing', () => {
      const listItemBubble =
          container.registerHelpBubble(LIST_ITEM_NATIVE_ID, '#list-item');
      assertTrue(listItemBubble !== null, 'help bubble is registered');
      assertTrue(
          container.canShowHelpBubble(LIST_ITEM_NATIVE_ID),
          'help bubble can be shown');
      assertFalse(container.isHelpBubbleShowing());
      assertFalse(container.isHelpBubbleShowingForTesting('list-item'));

      container.showHelpBubble(makeParams({id: getId(LIST_ITEM_NATIVE_ID)}));
      assertTrue(container.isHelpBubbleShowing());
      assertTrue(container.isHelpBubbleShowingForTesting('list-item'));

      // re-register when help bubble is shown
      assertFalse(
          container.registerHelpBubble(LIST_ITEM_NATIVE_ID, '#list-item'),
          'registerHelpBubble fails when help bubble is shown');
      assertTrue(
          container.isHelpBubbleShowing(),
          're-registering does not hide help bubble');
      assertTrue(container.isHelpBubbleShowingForTesting('list-item'));

      // unregister directly when help bubble is shown
      container.unregisterHelpBubble(LIST_ITEM_NATIVE_ID);
      assertFalse(
          container.isHelpBubbleShowing(), 'unregister hides help bubble');
      assertFalse(container.isHelpBubbleShowingForTesting('list-item'));
    });
  });
});
