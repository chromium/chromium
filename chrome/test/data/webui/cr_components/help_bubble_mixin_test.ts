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
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

const EXAMPLE_NATIVE_IDENTIFIER: string =
    'kHelpBubbleMixinTestElementIdentifier';

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
      <help-bubble id='helpBubble'></help-bubble>
      <ul id='bulletList'>
        <li>List item 1</li>
        <li>List item 2</li>
      </ul>
    </div>`;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.registerHelpBubbleIdentifier(EXAMPLE_NATIVE_IDENTIFIER, 'p1');
    // TODO(dfried): register additional anchors when we support them
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
      'helpBubbleHostVisibilityChanged',
      'helpBubbleButtonPressed',
      'helpBubbleClosed',
    ]);
  }

  helpBubbleHostVisibilityChanged(visible: boolean) {
    this.methodCalled('helpBubbleHostVisibilityChanged', visible);
  }

  helpBubbleButtonPressed(button: number) {
    this.methodCalled('helpBubbleButtonPressed', button);
  }

  helpBubbleClosed(byUser: boolean) {
    this.methodCalled('helpBubbleClosed', byUser);
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
  let helpBubble: HelpBubbleElement;

  setup(() => {
    testProxy = new TestHelpBubbleProxy();
    HelpBubbleProxyImpl.setInstance(testProxy);

    document.body.innerHTML = '';
    container = document.createElement('help-bubble-mixin-test-element');
    document.body.appendChild(container);
    helpBubble = container.$.helpBubble;
    return waitAfterNextRender(container);
  });

  test('help bubble mixin reports bubble closed', () => {
    assertFalse(container.isHelpBubbleShowing());
  });

  test('help bubble mixin reports bubble open', () => {
    helpBubble.anchorId = 'p1';
    helpBubble.position = HelpBubblePosition.BELOW;
    helpBubble.body = 'help bubble body';
    helpBubble.show();
    assertTrue(container.isHelpBubbleShowing());
  });

  const defaultParams: HelpBubbleParams = new HelpBubbleParams();
  defaultParams.nativeIdentifier = EXAMPLE_NATIVE_IDENTIFIER;
  defaultParams.position = HelpBubblePosition.ABOVE;
  defaultParams.bodyText = 'This is a help bubble.';
  defaultParams.buttons = [];

  test('help bubble mixin shows bubble when called directly', () => {
    container.showHelpBubble('p1', defaultParams);
    assertTrue(container.isHelpBubbleShowing());
    assertEquals(container.$.p1, helpBubble.getAnchorElement());
  });

  test('help bubble mixin hides bubble when called directly', () => {
    container.showHelpBubble('p1', defaultParams);
    assertTrue(container.hideHelpBubble());
    assertFalse(container.isHelpBubbleShowing());
  });

  test(
      'help bubble mixin shows help bubble when called via proxy', async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        assertTrue(container.isHelpBubbleShowing());
        assertEquals(container.$.p1, helpBubble.getAnchorElement());
      });

  test(
      'help bubble mixin hides help bubble when called via proxy', async () => {
        testProxy.getCallbackRouterRemote().showHelpBubble(defaultParams);
        await waitAfterNextRender(container);
        testProxy.getCallbackRouterRemote().hideHelpBubble();
        await waitAfterNextRender(container);
        assertFalse(container.isHelpBubbleShowing());
      });

  test('help bubble mixin sends event on initially visible', async () => {
    await waitAfterNextRender(container);
    assertEquals(
        1,
        testProxy.getHandler().getCallCount('helpBubbleHostVisibilityChanged'));
    assertDeepEquals(
        [true],
        testProxy.getHandler().getArgs('helpBubbleHostVisibilityChanged'));
  });

  test('help bubble mixin sends event on lost visibility', async () => {
    // Already waited for the container to render, but intersection events won't
    // be sent until the following frame.
    await waitAfterNextRender(container);

    container.style.display = 'none';

    // The same applies here.
    await waitAfterNextRender(container);
    await waitAfterNextRender(container);

    assertEquals(
        2,
        testProxy.getHandler().getCallCount('helpBubbleHostVisibilityChanged'));
    assertDeepEquals(
        [true, false],
        testProxy.getHandler().getArgs('helpBubbleHostVisibilityChanged'));
  });

  test(
      'help bubble mixin sends event on closed due to anchor losing visibility',
      async () => {
        // Already waited for the container to render, but intersection events
        // won't be sent until the following frame.
        await waitAfterNextRender(container);
        container.showHelpBubble('p1', defaultParams);

        // Hiding the container will cause the bubble to be closed.
        container.style.display = 'none';

        // The same applies here.
        await waitAfterNextRender(container);
        await waitAfterNextRender(container);

        assertEquals(
            1, testProxy.getHandler().getCallCount('helpBubbleClosed'));
        assertDeepEquals(
            [false], testProxy.getHandler().getArgs('helpBubbleClosed'));
        assertFalse(container.isHelpBubbleShowing());
      });
});
