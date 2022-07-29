// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://resources/cr_components/help_bubble/help_bubble.js';

import {HelpBubbleElement} from 'chrome://resources/cr_components/help_bubble/help_bubble.js';
import {HelpBubbleClientCallbackRouter, HelpBubbleClientRemote, HelpBubbleHandlerInterface, HelpBubbleParams, HelpBubblePosition} from 'chrome://resources/cr_components/help_bubble/help_bubble.mojom-webui.js';
import {HelpBubbleMixin, HelpBubbleMixinInterface} from 'chrome://resources/cr_components/help_bubble/help_bubble_mixin.js';
import {HelpBubbleProxy, HelpBubbleProxyImpl} from 'chrome://resources/cr_components/help_bubble/help_bubble_proxy.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible, waitAfterNextRender} from 'chrome://webui-test/test_util.js';

const TITLE_NATIVE_ID: string = 'kHelpBubbleMixinTestTitleElementId';
const PARAGRAPH_NATIVE_ID: string = 'kHelpBubbleMixinTestParagraphElementId';
const LIST_NATIVE_ID: string = 'kHelpBubbleMixinTestListElementId';
const CLOSE_BUTTON_ALT_TEXT: string = 'Close help bubble.';

const HelpBubbleMixinTestElementBase = HelpBubbleMixin(PolymerElement) as {
  new (): PolymerElement & HelpBubbleMixinInterface,
};

export interface HelpBubbleMixinTestElement {
  $: {
    bulletList: HTMLElement,
    container: HTMLElement,
    helpBubble: HelpBubbleElement,
    p1: HTMLElement,
    title: HTMLElement,
  };
}

export class HelpBubbleMixinTestElement extends HelpBubbleMixinTestElementBase {
  static get is() {
    return 'help-bubble-mixin-test-element';
  }

  static get template() {
    return html`
    <div id='container'>
      <h1 id='title'>This is the title</h1>
      <p id='p1'>Some paragraph text</p>
      <ul id='bulletList'>
        <li>List item 1</li>
        <li>List item 2</li>
      </ul>
    </div>`;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.registerHelpBubbleIdentifier(TITLE_NATIVE_ID, 'title');
    this.registerHelpBubbleIdentifier(PARAGRAPH_NATIVE_ID, 'p1');
    this.registerHelpBubbleIdentifier(LIST_NATIVE_ID, 'bulletList');
  }


  getHelpBubbleFor(anchorId: string): HelpBubbleElement|null {
    return this.shadowRoot!.querySelector(
        `help-bubble[anchor-id='${anchorId}']`);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble-mixin-test-element': HelpBubbleMixinTestElement;
  }
}

customElements.define(
    HelpBubbleMixinTestElement.is, HelpBubbleMixinTestElement);

class TestHelpBubbleHandler extends TestBrowserProxy implements
    HelpBubbleHandlerInterface {
  constructor() {
    super([
      'helpBubbleAnchorVisibilityChanged',
      'helpBubbleButtonPressed',
      'helpBubbleClosed',
    ]);
  }

  helpBubbleAnchorVisibilityChanged(
      nativeIdentifier: string, visible: boolean) {
    this.methodCalled(
        'helpBubbleAnchorVisibilityChanged', nativeIdentifier, visible);
  }

  helpBubbleButtonPressed(nativeIdentifier: string, button: number) {
    this.methodCalled('helpBubbleButtonPressed', nativeIdentifier, button);
  }

  helpBubbleClosed(nativeIdentifier: string, byUser: boolean) {
    this.methodCalled('helpBubbleClosed', nativeIdentifier, byUser);
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

  setup(() => {
    testProxy = new TestHelpBubbleProxy();
    HelpBubbleProxyImpl.setInstance(testProxy);

    document.body.innerHTML = '';
    container = document.createElement('help-bubble-mixin-test-element');
    document.body.appendChild(container);
    return waitForVisibilityEvents();
  });

  test('help bubble mixin reports bubble closed', () => {
    assertFalse(container.isHelpBubbleShowing());
  });

  const defaultParams: HelpBubbleParams = new HelpBubbleParams();
  defaultParams.nativeIdentifier = PARAGRAPH_NATIVE_ID;
  defaultParams.closeButtonAltText = CLOSE_BUTTON_ALT_TEXT;
  defaultParams.position = HelpBubblePosition.ABOVE;
  defaultParams.bodyText = 'This is a help bubble.';
  defaultParams.buttons = [];

  test('help bubble mixin shows bubble when called directly', () => {
    assertFalse(container.isHelpBubbleShowing());
    assertFalse(container.isHelpBubbleShowingFor('p1'));
    container.showHelpBubble('p1', defaultParams);
    assertTrue(container.isHelpBubbleShowing());
    assertTrue(container.isHelpBubbleShowingFor('p1'));
  });

  test('help bubble mixin reports not open for other elements', () => {
    // Valid but not open.
    assertFalse(container.isHelpBubbleShowingFor('title'));
    // Not valid (and not open).
    assertFalse(container.isHelpBubbleShowingFor('foo'));
  });

  test('help bubble mixin hides bubble when called directly', () => {
    container.showHelpBubble('p1', defaultParams);
    assertTrue(container.hideHelpBubble('p1'));
    assertFalse(container.isHelpBubbleShowing());
  });

  test('help bubble mixin called directly doesn\'t hide wrong bubble', () => {
    container.showHelpBubble('p1', defaultParams);
    assertFalse(container.hideHelpBubble('title'));
    assertTrue(container.isHelpBubbleShowing());
  });

  test('help bubble mixin show and hide multiple bubbles directly', () => {
    container.showHelpBubble('p1', defaultParams);
    assertTrue(container.isHelpBubbleShowingFor('p1'));
    assertFalse(container.isHelpBubbleShowingFor('title'));
    assertTrue(container.isHelpBubbleShowing());

    container.showHelpBubble('title', defaultParams);
    assertTrue(container.isHelpBubbleShowingFor('p1'));
    assertTrue(container.isHelpBubbleShowingFor('title'));
    assertTrue(container.isHelpBubbleShowing());

    container.hideHelpBubble('p1');
    assertFalse(container.isHelpBubbleShowingFor('p1'));
    assertTrue(container.isHelpBubbleShowingFor('title'));
    assertTrue(container.isHelpBubbleShowing());

    container.hideHelpBubble('title');
    assertFalse(container.isHelpBubbleShowingFor('p1'));
    assertFalse(container.isHelpBubbleShowingFor('title'));
    assertFalse(container.isHelpBubbleShowing());
  });

  test(
      'help bubble mixin shows help bubble when called via proxy', async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        assertTrue(container.isHelpBubbleShowing());
        const bubble = container.getHelpBubbleFor('p1');
        assertTrue(!!bubble);
        assertEquals(container.$.p1, bubble.getAnchorElement());
        assertTrue(isVisible(bubble));
      });

  test('help bubble mixin uses close button alt text', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    const bubble = container.getHelpBubbleFor('p1')!;
    const closeButton = bubble.shadowRoot!.querySelector<HTMLElement>('#close');
    assertTrue(!!closeButton);
    assertEquals(CLOSE_BUTTON_ALT_TEXT, closeButton.getAttribute('aria-label'));
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
        const params: HelpBubbleParams = new HelpBubbleParams();
        params.nativeIdentifier = 'This is an unregistered identifier';
        params.closeButtonAltText = CLOSE_BUTTON_ALT_TEXT;
        params.position = HelpBubblePosition.ABOVE;
        params.bodyText = 'This is a help bubble.';
        params.buttons = [];

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
    // Since we're watching three elements, we get events for all three.
    assertEquals(
        3,
        testProxy.getHandler().getCallCount(
            'helpBubbleAnchorVisibilityChanged'));
    assertDeepEquals(
        [
          [TITLE_NATIVE_ID, true],
          [PARAGRAPH_NATIVE_ID, true],
          [LIST_NATIVE_ID, true],
        ],
        testProxy.getHandler().getArgs('helpBubbleAnchorVisibilityChanged'));
  });

  test('help bubble mixin sends event on lost visibility', async () => {
    container.style.display = 'none';
    await waitForVisibilityEvents();
    assertEquals(
        6,
        testProxy.getHandler().getCallCount(
            'helpBubbleAnchorVisibilityChanged'));
    assertDeepEquals(
        [
          [TITLE_NATIVE_ID, true],
          [PARAGRAPH_NATIVE_ID, true],
          [LIST_NATIVE_ID, true],
          [TITLE_NATIVE_ID, false],
          [PARAGRAPH_NATIVE_ID, false],
          [LIST_NATIVE_ID, false],
        ],
        testProxy.getHandler().getArgs('helpBubbleAnchorVisibilityChanged'));
  });

  test(
      'help bubble mixin sends event on closed due to anchor losing visibility',
      async () => {
        container.showHelpBubble('p1', defaultParams);

        // Hiding the container will cause the bubble to be closed.
        container.$.p1.style.display = 'none';
        await waitForVisibilityEvents();

        assertEquals(
            1, testProxy.getHandler().getCallCount('helpBubbleClosed'));
        assertDeepEquals(
            [[PARAGRAPH_NATIVE_ID, false]],
            testProxy.getHandler().getArgs('helpBubbleClosed'));
        assertFalse(container.isHelpBubbleShowing());
      });

  test(
      'help bubble mixin does not send event when non-anchor loses visibility',
      async () => {
        container.showHelpBubble('p1', defaultParams);

        // This is not the current bubble anchor, so should not send an event.
        container.$.title.style.display = 'none';
        await waitForVisibilityEvents();
        assertEquals(
            0, testProxy.getHandler().getCallCount('helpBubbleClosed'));
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
    const bubble = container.getHelpBubbleFor('p1');
    assertTrue(!!bubble);
    assertEquals(container.$.p1, bubble.getAnchorElement());
    assertTrue(isVisible(bubble));
  });

  const paramsWithTitle: HelpBubbleParams = new HelpBubbleParams();
  paramsWithTitle.nativeIdentifier = TITLE_NATIVE_ID;
  paramsWithTitle.closeButtonAltText = CLOSE_BUTTON_ALT_TEXT;
  paramsWithTitle.position = HelpBubblePosition.BELOW;
  paramsWithTitle.bodyText = 'This is another help bubble.';
  paramsWithTitle.titleText = 'This is a title';
  paramsWithTitle.buttons = [];

  test('help bubble mixin shows multiple bubbles', async () => {
    testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
    await waitAfterNextRender(container);
    testProxy.getCallbackRouterRemote().showHelpBubble(paramsWithTitle);
    await waitAfterNextRender(container);
    assertTrue(container.isHelpBubbleShowing());
    const bubble1 = container.getHelpBubbleFor('title');
    const bubble2 = container.getHelpBubbleFor('p1');
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
    const titleBubble = container.getHelpBubbleFor('title')!;
    const paragraphBubble = container.getHelpBubbleFor('p1')!;
    // Testing that setting `titleText` will cause the title to display
    // correctly is present in help_bubble_test.ts, so it is sufficient to
    // verify that the property is set correctly.
    assertEquals('', paragraphBubble.titleText);
    assertEquals(paramsWithTitle.titleText, titleBubble.titleText);
  });

  const paramsWithProgress: HelpBubbleParams = new HelpBubbleParams();
  paramsWithProgress.nativeIdentifier = LIST_NATIVE_ID;
  paramsWithProgress.closeButtonAltText = CLOSE_BUTTON_ALT_TEXT;
  paramsWithProgress.position = HelpBubblePosition.BELOW;
  paramsWithProgress.bodyText = 'This is another help bubble.';
  paramsWithProgress.progress = {current: 1, total: 3};
  paramsWithProgress.buttons = [];

  test(
      'help bubble mixin shows bubbles with and without progress', async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        testProxy.getCallbackRouterRemote().showHelpBubble(paramsWithProgress);
        await waitAfterNextRender(container);
        assertTrue(container.isHelpBubbleShowing());
        const paragraphBubble = container.getHelpBubbleFor('p1')!;
        const progressBubble = container.getHelpBubbleFor('bulletList')!;
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
        container.getHelpBubbleFor('title')?.getAnchorElement());
    assertEquals(null, container.getHelpBubbleFor('p1'));

    testProxy.getCallbackRouterRemote().hideHelpBubble(
        paramsWithTitle.nativeIdentifier);
    await waitAfterNextRender(container);
    assertFalse(container.isHelpBubbleShowing());
    assertEquals(null, container.getHelpBubbleFor('title'));
    assertEquals(null, container.getHelpBubbleFor('p1'));
  });

  test('help bubble mixin sends event on closed via button', async () => {
    container.showHelpBubble('p1', defaultParams);

    // Click the close button.
    container.shadowRoot!.querySelector('help-bubble')!.$.close.click();
    await waitForVisibilityEvents();
    assertEquals(1, testProxy.getHandler().getCallCount('helpBubbleClosed'));
    assertDeepEquals(
        [[PARAGRAPH_NATIVE_ID, true]],
        testProxy.getHandler().getArgs('helpBubbleClosed'));
    assertFalse(container.isHelpBubbleShowing());
  });

  const buttonParams: HelpBubbleParams = new HelpBubbleParams();
  buttonParams.nativeIdentifier = PARAGRAPH_NATIVE_ID;
  buttonParams.closeButtonAltText = CLOSE_BUTTON_ALT_TEXT;
  buttonParams.position = HelpBubblePosition.BELOW;
  buttonParams.bodyText = 'This is another help bubble.';
  buttonParams.titleText = 'This is a title';
  buttonParams.buttons = [
    {
      text: 'button1',
      isDefault: false,
    },
    {
      text: 'button2',
      isDefault: true,
    },
  ];

  test('help bubble mixin sends action button clicked event', async () => {
    container.showHelpBubble('p1', buttonParams);
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
});
