// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/help_bubble/help_bubble.js';

import type {HelpBubbleElement} from 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import type {HelpBubbleClientRemote, HelpBubbleHandlerInterface, HelpBubbleParams} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {HelpBubbleArrowPosition, HelpBubbleClientCallbackRouter, HelpBubbleClosedReason} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import type {HelpBubbleController} from 'chrome://resources/cr_components/help_bubble/help_bubble_controller.js';
import {ANCHOR_HIGHLIGHT_CLASS} from 'chrome://resources/cr_components/help_bubble/help_bubble_controller.js';
import type {HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {HelpBubbleMixin} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import type {HelpBubbleProxy} from 'chrome://resources/cr_components/help_bubble/help_bubble_proxy.js';
import {HelpBubbleProxyImpl} from 'chrome://resources/cr_components/help_bubble/help_bubble_proxy.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertThrows, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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

const HelpBubbleMixinTestElementBase = HelpBubbleMixin(PolymerElement) as {
  new (): PolymerElement & HelpBubbleMixinInterface,
};

interface HelpBubbleMixinTestElement {
  $: {
    bulletList: HTMLElement,
    container: HTMLElement,
    helpBubble: HelpBubbleElement,
    p1: HTMLElement,
    title: HTMLElement,
  };
}

let titleBubble: HelpBubbleController;
let p1Bubble: HelpBubbleController;
let bulletListBubble: HelpBubbleController;
let spanBubble: HelpBubbleController;
let nestedChildBubble: HelpBubbleController;

// HelpBubbleMixinTestElement
class HelpBubbleMixinTestElement extends HelpBubbleMixinTestElementBase {
  static get is() {
    return 'help-bubble-mixin-test-element';
  }

  static get template() {
    return html`
    <div id='container'>
      <h1 id='title'>This is the title</h1>
      <p id='p1'>Some paragraph text</p>
      <ul id='bulletList'>
        <li id='list-item'>List item 1</li>
        <li>List item 2</li>
      </ul>
      <span style='display: block;'>Span text</span>
      <container-element id='container-element'></container-element>
    </div>`;
  }

  override connectedCallback() {
    super.connectedCallback();

    const spanEl = this.shadowRoot!.querySelector('span');
    assertTrue(spanEl !== null, 'connectedCallback: span element exists');

    titleBubble = this.registerHelpBubble(TITLE_NATIVE_ID, '#title')!;
    p1Bubble = this.registerHelpBubble(PARAGRAPH_NATIVE_ID, '#p1')!;
    bulletListBubble = this.registerHelpBubble(LIST_NATIVE_ID, '#bulletList')!;
    spanBubble = this.registerHelpBubble(SPAN_NATIVE_ID, spanEl)!;

    // using different types of selectors to test query mechanism
    nestedChildBubble = this.registerHelpBubble(
        NESTED_CHILD_NATIVE_ID, ['#container-element', '.child-element'])!;
  }
}

customElements.define(
    HelpBubbleMixinTestElement.is, HelpBubbleMixinTestElement);

// HelpBubbleMixinTestContainerElement
export class HelpBubbleMixinTestContainerElement extends PolymerElement {
  static get is() {
    return 'container-element';
  }

  static get template() {
    return html`
    <div>
      <div class='child-element'>ABCDE</div>
    </div>`;
  }
}

customElements.define(
    HelpBubbleMixinTestContainerElement.is,
    HelpBubbleMixinTestContainerElement);

class TestHelpBubbleHandler extends TestBrowserProxy implements
    HelpBubbleHandlerInterface {
  // Records the current visibility of all known elements.
  // Simply looking at the call logs can produce extraneous results, as
  // visible=true may be generated multiple times if an element e.g. changes
  // position on the page.
  visibility: Map<string, boolean> = new Map();

  constructor() {
    super([
      'helpBubbleAnchorVisibilityChanged',
      'helpBubbleAnchorActivated',
      'helpBubbleAnchorCustomEvent',
      'helpBubbleButtonPressed',
      'helpBubbleClosed',
    ]);
  }

  helpBubbleAnchorVisibilityChanged(
      nativeIdentifier: string, visible: boolean) {
    this.visibility.set(nativeIdentifier, visible);
    this.methodCalled(
        'helpBubbleAnchorVisibilityChanged', nativeIdentifier, visible);
  }

  helpBubbleAnchorActivated(nativeIdentifier: string) {
    this.methodCalled('helpBubbleAnchorActivated', nativeIdentifier);
  }

  helpBubbleAnchorCustomEvent(nativeIdentifier: string, eventName: string) {
    this.methodCalled(
        'helpBubbleAnchorCustomEvent', nativeIdentifier, eventName);
  }

  helpBubbleButtonPressed(nativeIdentifier: string, button: number) {
    this.methodCalled('helpBubbleButtonPressed', nativeIdentifier, button);
  }

  helpBubbleClosed(nativeIdentifier: string, reason: HelpBubbleClosedReason) {
    this.methodCalled('helpBubbleClosed', nativeIdentifier, reason);
  }
}

class TestHelpBubbleProxy extends TestBrowserProxy implements HelpBubbleProxy {
  private testHandler_ = new TestHelpBubbleHandler();
  private callbackRouter_: HelpBubbleClientCallbackRouter =
      new HelpBubbleClientCallbackRouter();
  private callbackRouterRemote_: HelpBubbleClientRemote;

  constructor() {
    super();

    this.callbackRouterRemote_ =
        this.callbackRouter_.$.bindNewPipeAndPassRemote();
  }

  getHandler(): TestHelpBubbleHandler {
    return this.testHandler_;
  }

  getCallbackRouter(): HelpBubbleClientCallbackRouter {
    return this.callbackRouter_;
  }

  getCallbackRouterRemote(): HelpBubbleClientRemote {
    return this.callbackRouterRemote_;
  }
}

interface WaitForSuccessParams {
  retryIntervalMs: number;
  totalMs: number;
  assertionFn: () => void;
}

suite('CrComponentsHelpBubbleMixinTest', () => {
  let testProxy: TestHelpBubbleProxy;
  let container: HelpBubbleMixinTestElement;

  /**
   * Waits for the current frame to render, which queues intersection events,
   * and then waits for the intersection events to propagate to listeners, which
   * triggers visibility messages.
   *
   * This takes a total of two frames. A single frame will cause the layout to
   * be updated, but will not actually propagate the events.
   */
  async function waitForVisibilityEvents() {
    await waitAfterNextRender(container);
    return waitAfterNextRender(container);
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

  setup(() => {
    testProxy = new TestHelpBubbleProxy();
    HelpBubbleProxyImpl.setInstance(testProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    container = document.createElement('help-bubble-mixin-test-element') as
        HelpBubbleMixinTestElement;
    document.body.appendChild(container);
    return waitForVisibilityEvents();
  });

  test('help bubble mixin reports bubble closed', () => {
    assertFalse(container.isHelpBubbleShowing());
  });

  const defaultParams: HelpBubbleParams = {
    nativeIdentifier: PARAGRAPH_NATIVE_ID,
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
  };

  test('help bubble mixin shows bubble when called directly', () => {
    assertFalse(container.isHelpBubbleShowing());
    assertFalse(container.isHelpBubbleShowingForTesting('p1'));
    container.showHelpBubble(p1Bubble, defaultParams);
    assertTrue(container.isHelpBubbleShowing());
    assertTrue(container.isHelpBubbleShowingForTesting('p1'));
  });

  test(
      'help bubble mixin shows bubble anchored to arbitrary HTMLElment', () => {
        assertFalse(container.isHelpBubbleShowing());
        assertFalse(spanBubble.isBubbleShowing());
        container.showHelpBubble(spanBubble, defaultParams);
        assertTrue(container.isHelpBubbleShowing());
        assertTrue(spanBubble.isBubbleShowing());
      });

  test(
      'help bubble mixin can pierce shadow dom to anchor to deep query', () => {
        const containerElement =
            container.shadowRoot!.querySelector('#container-element');
        let childElement =
            container.shadowRoot!.querySelector('.child-element');

        assertTrue(containerElement !== null, 'container element is found');
        assertTrue(
            childElement === null, 'child element is isolated from container');

        childElement =
            containerElement.shadowRoot!.querySelector('.child-element');
        assertTrue(
            childElement !== null, 'child element is rendered in shadow dom');

        assertTrue(
            childElement === nestedChildBubble.getAnchor(),
            'help bubble anchors to correct element in shadow dom');

        assertFalse(container.isHelpBubbleShowing());
        assertFalse(nestedChildBubble.isBubbleShowing());
        container.showHelpBubble(nestedChildBubble, defaultParams);
        assertTrue(container.isHelpBubbleShowing());
        assertTrue(nestedChildBubble.isBubbleShowing());
      });

  test('help bubble mixin reports not open for other elements', () => {
    // Valid but not open.
    assertFalse(container.isHelpBubbleShowingForTesting('title'));
    // Not valid (and not open).
    assertFalse(container.isHelpBubbleShowingForTesting('foo'));
  });

  test('help bubble mixin hides bubble when called directly', () => {
    container.showHelpBubble(p1Bubble, defaultParams);
    assertTrue(container.hideHelpBubble(p1Bubble.getNativeId()));
    assertFalse(container.isHelpBubbleShowing());
  });

  test('help bubble mixin called directly doesn\'t hide wrong bubble', () => {
    container.showHelpBubble(p1Bubble, defaultParams);
    assertFalse(container.hideHelpBubble(titleBubble.getNativeId()));
    assertTrue(container.isHelpBubbleShowing());
  });

  test('help bubble mixin show and hide multiple bubbles directly', () => {
    container.showHelpBubble(p1Bubble, defaultParams);
    assertTrue(container.isHelpBubbleShowingForTesting('p1'));
    assertFalse(container.isHelpBubbleShowingForTesting('title'));
    assertTrue(container.isHelpBubbleShowing());

    container.showHelpBubble(titleBubble, defaultParams);
    assertTrue(container.isHelpBubbleShowingForTesting('p1'));
    assertTrue(container.isHelpBubbleShowingForTesting('title'));
    assertTrue(container.isHelpBubbleShowing());

    container.hideHelpBubble(p1Bubble.getNativeId());
    assertFalse(container.isHelpBubbleShowingForTesting('p1'));
    assertTrue(container.isHelpBubbleShowingForTesting('title'));
    assertTrue(container.isHelpBubbleShowing());

    container.hideHelpBubble(titleBubble.getNativeId());
    assertFalse(container.isHelpBubbleShowingForTesting('p1'));
    assertFalse(container.isHelpBubbleShowingForTesting('title'));
    assertFalse(container.isHelpBubbleShowing());
  });

  test(
      'help bubble mixin shows help bubble when called via proxy', async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        assertTrue(container.isHelpBubbleShowing(), 'a bubble is showing');
        const bubble = container.getHelpBubbleForTesting('p1');
        assertTrue(!!bubble, 'bubble exists');
        assertEquals(
            container.$.p1, bubble.getAnchorElement(),
            'bubble has correct anchor');
        assertTrue(isVisible(bubble), 'bubble is visible');
      });

  test('help bubble mixin uses close button alt text', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    const bubble = container.getHelpBubbleForTesting('p1')!;
    const closeButton = bubble.shadowRoot!.querySelector<HTMLElement>('#close');
    assertTrue(!!closeButton);
    assertEquals(CLOSE_BUTTON_ALT_TEXT, closeButton.getAttribute('aria-label'));
  });

  test('help bubble mixin uses body icon', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    const bubble = container.getHelpBubbleForTesting('p1')!;
    assertEquals(bubble.bodyIconName, defaultParams.bodyIconName);
    const bodyIcon = bubble.shadowRoot!.querySelector<HTMLElement>('#bodyIcon');
    assertTrue(!!bodyIcon);
    const ironIcon = bodyIcon.querySelector('cr-icon');
    assertTrue(!!ironIcon);
    assertEquals(`iph:${defaultParams.bodyIconName}`, ironIcon.icon);
  });

  test(
      'help bubble mixin does not use body icon when not defined', async () => {
        const noIconParams = {...defaultParams, bodyIconName: null};
        testProxy.getCallbackRouterRemote().showHelpBubble(noIconParams);
        await waitAfterNextRender(container);
        assertTrue(container.isHelpBubbleShowing());
        const bubble = container.getHelpBubbleForTesting('p1')!;
        assertEquals(bubble.bodyIconName, null);
        const bodyIcon =
            bubble.shadowRoot!.querySelector<HTMLElement>('#bodyIcon');
        assertTrue(!!bodyIcon);
        assertTrue(bodyIcon.hidden);
      });

  test(
      'help bubble mixin hides help bubble when called via proxy', async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        testProxy.getCallbackRouterRemote().hideHelpBubble(
            defaultParams.nativeIdentifier);
        await waitAfterNextRender(container);
        assertFalse(container.isHelpBubbleShowing());
      });

  test(
      'help bubble adds class to element on external help bubble shown',
      async () => {
        testProxy.getCallbackRouterRemote().externalHelpBubbleUpdated(
            TITLE_NATIVE_ID, true);
        await waitAfterNextRender(container);
        assertTrue(
            container.$.title.classList.contains(ANCHOR_HIGHLIGHT_CLASS));
        testProxy.getCallbackRouterRemote().externalHelpBubbleUpdated(
            TITLE_NATIVE_ID, false);
        await waitAfterNextRender(container);
        assertFalse(
            container.$.title.classList.contains(ANCHOR_HIGHLIGHT_CLASS));
      });

  test(
      'help bubble mixin doesn\'t hide help bubble when called with wrong id',
      async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        testProxy.getCallbackRouterRemote().hideHelpBubble(LIST_NATIVE_ID);
        await waitAfterNextRender(container);
        assertTrue(container.isHelpBubbleShowing());
      });

  test(
      'help bubble ignores unregistered ID in ShowHelpBubble call',
      async () => {
        const params: HelpBubbleParams = {
          nativeIdentifier: 'This is an unregistered identifier',
          closeButtonAltText: CLOSE_BUTTON_ALT_TEXT,
          bodyIconAltText: BODY_ICON_ALT_TEXT,
          position: HelpBubbleArrowPosition.BOTTOM_CENTER,
          bodyText: 'This is a help bubble.',
          buttons: [],
          bodyIconName: null,
          focusOnShowHint: null,
          progress: null,
          timeout: null,
          titleText: null,
        };

        testProxy.getCallbackRouterRemote().showHelpBubble(params);
        await waitAfterNextRender(container);
        assertFalse(container.isHelpBubbleShowing());
      });

  test(
      'help bubble ignores unregistered ID in HideHelpBubble call',
      async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        testProxy.getCallbackRouterRemote().hideHelpBubble(
            'This is an unregistered identifier');
        await waitAfterNextRender(container);
        assertTrue(container.isHelpBubbleShowing());
      });

  test('help bubble ignores unregistered ID in focus call', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    testProxy.getCallbackRouterRemote().toggleFocusForAccessibility(
        'This is an unregistered identifier');
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
  });

  test('help bubble mixin sends events on initially visible', async () => {
    await waitAfterNextRender(container);
    assertDeepEquals(
        new Map<string, boolean>([
          [TITLE_NATIVE_ID, true],
          [PARAGRAPH_NATIVE_ID, true],
          [LIST_NATIVE_ID, true],
          [SPAN_NATIVE_ID, true],
          [NESTED_CHILD_NATIVE_ID, true],
        ]),
        testProxy.getHandler().visibility);
  });

  test('help bubble mixin sends event on lost visibility', async () => {
    await waitAfterNextRender(container);
    container.style.display = 'none';
    await waitForVisibilityEvents();
    assertDeepEquals(
        new Map<string, boolean>([
          [TITLE_NATIVE_ID, false],
          [PARAGRAPH_NATIVE_ID, false],
          [LIST_NATIVE_ID, false],
          [SPAN_NATIVE_ID, false],
          [NESTED_CHILD_NATIVE_ID, false],
        ]),
        testProxy.getHandler().visibility);
  });

  test('help bubble mixin sends event on element activated', async () => {
    container.showHelpBubble(titleBubble, defaultParams);
    container.showHelpBubble(bulletListBubble, defaultParams);
    await waitAfterNextRender(container);
    container.notifyHelpBubbleAnchorActivated(bulletListBubble.getNativeId());
    container.notifyHelpBubbleAnchorActivated(titleBubble.getNativeId());
    assertEquals(
        2, testProxy.getHandler().getCallCount('helpBubbleAnchorActivated'));
    assertDeepEquals(
        [LIST_NATIVE_ID, TITLE_NATIVE_ID],
        testProxy.getHandler().getArgs('helpBubbleAnchorActivated'));
  });

  test('help bubble mixin sends custom events', async () => {
    container.showHelpBubble(p1Bubble, defaultParams);
    container.showHelpBubble(titleBubble, defaultParams);
    await waitAfterNextRender(container);
    container.notifyHelpBubbleAnchorCustomEvent(
        p1Bubble.getNativeId(), EVENT1_NAME);
    container.notifyHelpBubbleAnchorCustomEvent(
        titleBubble.getNativeId(), EVENT2_NAME);
    assertEquals(
        2, testProxy.getHandler().getCallCount('helpBubbleAnchorCustomEvent'));
    assertDeepEquals(
        [
          [PARAGRAPH_NATIVE_ID, EVENT1_NAME],
          [TITLE_NATIVE_ID, EVENT2_NAME],
        ],
        testProxy.getHandler().getArgs('helpBubbleAnchorCustomEvent'));
  });

  test(
      'help bubble mixin sends event on closed due to anchor losing visibility',
      async () => {
        container.showHelpBubble(p1Bubble, defaultParams);

        // Hiding the container will cause the bubble to be closed.
        container.$.p1.style.display = 'none';
        await waitForVisibilityEvents();

        assertEquals(
            1, testProxy.getHandler().getCallCount('helpBubbleClosed'));
        assertDeepEquals(
            [[PARAGRAPH_NATIVE_ID, HelpBubbleClosedReason.kPageChanged]],
            testProxy.getHandler().getArgs('helpBubbleClosed'));
        assertFalse(container.isHelpBubbleShowing());
      });

  test(
      'help bubble mixin does not send event when non-anchor loses visibility',
      async () => {
        container.showHelpBubble(p1Bubble, defaultParams);

        // This is not the current bubble anchor, so should not send an event.
        container.$.title.style.display = 'none';
        await waitForVisibilityEvents();
        assertEquals(
            0, testProxy.getHandler().getCallCount('helpBubbleClosed'));
        assertTrue(container.isHelpBubbleShowing());
      });

  test('help bubble mixin does not timeout by default', async () => {
    container.showHelpBubble(p1Bubble, defaultParams);

    // This is not the current bubble anchor, so should not send an event.
    container.$.title.style.display = 'none';
    await waitForVisibilityEvents();
    assertEquals(0, testProxy.getHandler().getCallCount('helpBubbleClosed'));
    assertTrue(container.isHelpBubbleShowing());
    await sleep(100);  // 100ms
    assertEquals(0, testProxy.getHandler().getCallCount('helpBubbleClosed'));
    assertTrue(container.isHelpBubbleShowing());
  });

  test('help bubble mixin reshow bubble', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    testProxy.getCallbackRouterRemote().hideHelpBubble(
        defaultParams.nativeIdentifier);
    await waitAfterNextRender(container);
    assertFalse(container.isHelpBubbleShowing());
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    const bubble = container.getHelpBubbleForTesting('p1');
    assertTrue(!!bubble);
    assertEquals(container.$.p1, bubble.getAnchorElement());
    assertTrue(isVisible(bubble));
  });

  const paramsWithTitle: HelpBubbleParams = {
    nativeIdentifier: TITLE_NATIVE_ID,
    closeButtonAltText: CLOSE_BUTTON_ALT_TEXT,
    bodyIconAltText: BODY_ICON_ALT_TEXT,
    position: HelpBubbleArrowPosition.TOP_CENTER,
    bodyText: 'This is another help bubble.',
    titleText: 'This is a title',
    buttons: [],
    bodyIconName: null,
    focusOnShowHint: null,
    progress: null,
    timeout: null,
  };

  test('help bubble mixin shows multiple bubbles', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    testProxy.getCallbackRouterRemote().showHelpBubble(paramsWithTitle);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    const bubble1 = container.getHelpBubbleForTesting('title');
    const bubble2 = container.getHelpBubbleForTesting('p1');
    assertTrue(!!bubble1);
    assertTrue(!!bubble2);
    assertEquals(container.$.title, bubble1!.getAnchorElement());
    assertEquals(container.$.p1, bubble2!.getAnchorElement());
    assertTrue(isVisible(bubble1));
    assertTrue(isVisible(bubble2));
  });

  test('help bubble mixin shows bubbles with and without title', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    testProxy.getCallbackRouterRemote().showHelpBubble(paramsWithTitle);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    const titleBubble = container.getHelpBubbleForTesting('title')!;
    const paragraphBubble = container.getHelpBubbleForTesting('p1')!;
    // Testing that setting `titleText` will cause the title to display
    // correctly is present in help_bubble_test.ts, so it is sufficient to
    // verify that the property is set correctly.
    assertEquals('', paragraphBubble.titleText);
    assertEquals(paramsWithTitle.titleText, titleBubble.titleText);
  });

  const paramsWithProgress: HelpBubbleParams = {
    nativeIdentifier: LIST_NATIVE_ID,
    closeButtonAltText: CLOSE_BUTTON_ALT_TEXT,
    bodyIconAltText: BODY_ICON_ALT_TEXT,
    position: HelpBubbleArrowPosition.TOP_CENTER,
    bodyText: 'This is another help bubble.',
    progress: {current: 1, total: 3},
    buttons: [],
    bodyIconName: null,
    focusOnShowHint: null,
    timeout: null,
    titleText: null,
  };

  test(
      'help bubble mixin shows bubbles with and without progress', async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        testProxy.getCallbackRouterRemote().showHelpBubble(paramsWithProgress);
        await waitAfterNextRender(container);
        assertTrue(container.isHelpBubbleShowing());
        const paragraphBubble = container.getHelpBubbleForTesting('p1')!;
        const progressBubble = container.getHelpBubbleForTesting('bulletList')!;
        // Testing that setting `progress` will cause the progress to display
        // correctly is present in help_bubble_test.ts, so it is sufficient to
        // verify that the property is set correctly.
        assertFalse(!!paragraphBubble.progress);
        assertDeepEquals({current: 1, total: 3}, progressBubble.progress);
      });

  test('help bubble mixin hides multiple bubbles', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    testProxy.getCallbackRouterRemote().showHelpBubble(paramsWithTitle);
    await waitAfterNextRender(container);

    testProxy.getCallbackRouterRemote().hideHelpBubble(
        defaultParams.nativeIdentifier);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    assertEquals(
        container.$.title,
        container.getHelpBubbleForTesting('title')?.getAnchorElement());
    assertEquals(null, container.getHelpBubbleForTesting('p1'));

    testProxy.getCallbackRouterRemote().hideHelpBubble(
        paramsWithTitle.nativeIdentifier);
    await waitAfterNextRender(container);
    assertFalse(container.isHelpBubbleShowing());
    assertEquals(null, container.getHelpBubbleForTesting('title'));
    assertEquals(null, container.getHelpBubbleForTesting('p1'));
  });

  test('help bubble mixin sends event on closed via button', async () => {
    container.showHelpBubble(p1Bubble, defaultParams);

    // Click the close button.
    container.shadowRoot!.querySelector('help-bubble')!.$.close.click();
    await waitForVisibilityEvents();
    assertEquals(1, testProxy.getHandler().getCallCount('helpBubbleClosed'));
    assertDeepEquals(
        [[PARAGRAPH_NATIVE_ID, HelpBubbleClosedReason.kDismissedByUser]],
        testProxy.getHandler().getArgs('helpBubbleClosed'));
    assertFalse(container.isHelpBubbleShowing());
  });

  const buttonParams: HelpBubbleParams = {
    nativeIdentifier: PARAGRAPH_NATIVE_ID,
    closeButtonAltText: CLOSE_BUTTON_ALT_TEXT,
    bodyIconAltText: BODY_ICON_ALT_TEXT,
    position: HelpBubbleArrowPosition.TOP_CENTER,
    bodyIconName: null,
    bodyText: 'This is another help bubble.',
    titleText: 'This is a title',
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
    focusOnShowHint: null,
    progress: null,
    timeout: null,
  };

  test('help bubble mixin sends action button clicked event', async () => {
    container.showHelpBubble(p1Bubble, buttonParams);
    await waitAfterNextRender(container);

    // Click one of the action buttons.
    const button =
        container.shadowRoot!.querySelector('help-bubble')!.getButtonForTesting(
            1);
    assertTrue(!!button);
    button.click();
    await waitForVisibilityEvents();
    assertEquals(
        1, testProxy.getHandler().getCallCount('helpBubbleButtonPressed'));
    assertDeepEquals(
        [[PARAGRAPH_NATIVE_ID, 1]],
        testProxy.getHandler().getArgs('helpBubbleButtonPressed'));
    assertFalse(container.isHelpBubbleShowing());
  });

  const timeoutParams: HelpBubbleParams = {
    nativeIdentifier: PARAGRAPH_NATIVE_ID,
    closeButtonAltText: CLOSE_BUTTON_ALT_TEXT,
    bodyIconName: null,
    bodyIconAltText: BODY_ICON_ALT_TEXT,
    position: HelpBubbleArrowPosition.TOP_CENTER,
    bodyText: 'This is another help bubble.',
    titleText: 'This is a title',
    buttons: [],
    focusOnShowHint: null,
    progress: null,
    timeout: null,
  };

  // It is hard to guarantee the correct timing on various test systems,
  // so the 'before timeout' and 'after timeout' tests are split
  // into 2 separate fixtures

  // Before timeout
  // Use a long timeout to test base state that a timeout will
  // not be accidentally triggered when a timeout is set
  test('help bubble mixin does not immediately timeout', async () => {
    const longTimeoutParams = {
      ...timeoutParams,
      timeout: {
        microseconds: BigInt(10 * 1000 * 1000),  // 10s
      },
    };

    container.showHelpBubble(p1Bubble, longTimeoutParams);
    await waitAfterNextRender(container);
    assertEquals(
        0, testProxy.getHandler().getCallCount('helpBubbleClosed'),
        'helpBubbleClosed has not be called');
    assertTrue(container.isHelpBubbleShowing());
  });

  // After timeout
  // Use a short timeout and a retry loop to
  test('help bubble mixin sends timeout event', async () => {
    const timeoutMs = 100;
    const shortTimeoutParams = {
      ...timeoutParams,
      timeout: {
        microseconds: BigInt(timeoutMs * 1000),  // 100ms
      },
    };

    container.showHelpBubble(p1Bubble, shortTimeoutParams);
    await waitAfterNextRender(container);
    await waitForSuccess({
      retryIntervalMs: 50,
      totalMs: 1500,
      assertionFn: () => assertEquals(
          1, testProxy.getHandler().getCallCount('helpBubbleClosed'),
          'helpBubbleClosed has been called'),
    }) as number;
    assertDeepEquals(
        [[PARAGRAPH_NATIVE_ID, HelpBubbleClosedReason.kTimedOut]],
        testProxy.getHandler().getArgs('helpBubbleClosed'),
        'helpBubbleClosed is called with correct arguments');
    assertFalse(container.isHelpBubbleShowing(), 'no bubbles are showing');
  });

  test('help bubble mixin can unregister', () => {
    let listItemBubble =
        container.registerHelpBubble(LIST_ITEM_NATIVE_ID, '#bulletList');
    assertTrue(listItemBubble !== null, 'help bubble is registered');
    assertTrue(
        container.canShowHelpBubble(listItemBubble!),
        'help bubble can be shown');

    // re-register when help bubble is not showing
    listItemBubble =
        container.registerHelpBubble(LIST_ITEM_NATIVE_ID, '#list-item');
    assertTrue(
        listItemBubble !== null,
        'help bubble can be re-registered with same nativeId');
    assertTrue(
        container.canShowHelpBubble(listItemBubble!),
        'help bubble can be shown after re-registering');

    // un-register directly when help bubble is not showing
    container.unregisterHelpBubble(LIST_ITEM_NATIVE_ID);
    assertFalse(
        container.canShowHelpBubble(listItemBubble!),
        'help bubble cannot be shown');
    // unregisterHelpBubble clears out the nativeIds
    assertThrows(
        () => container.showHelpBubble(listItemBubble!, defaultParams),
        'Can\'t show help bubble',
    );
  });

  test('help bubble mixin can unregister when bubble is showing', () => {
    const listItemBubble =
        container.registerHelpBubble(LIST_ITEM_NATIVE_ID, '#list-item');
    assertTrue(listItemBubble !== null, 'help bubble is registered');
    assertTrue(
        container.canShowHelpBubble(listItemBubble!),
        'help bubble can be shown');
    assertFalse(container.isHelpBubbleShowing());
    assertFalse(container.isHelpBubbleShowingForTesting('list-item'));

    container.showHelpBubble(listItemBubble!, defaultParams);
    assertTrue(container.isHelpBubbleShowing());
    assertTrue(container.isHelpBubbleShowingForTesting('list-item'));

    // re-register when help bubble is shown
    const result =
        container.registerHelpBubble(LIST_ITEM_NATIVE_ID, '#list-item');
    assertTrue(
        result === null, 'registerHelpBubble fails when help bubble is shown');
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
