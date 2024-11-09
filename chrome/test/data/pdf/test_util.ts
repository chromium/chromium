// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utilities that are used in multiple tests.

import type {Bookmark, DocumentDimensions, LayoutOptions, PdfViewerElement, ViewerToolbarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {resetForTesting as resetMetricsForTesting, UserAction, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
// <if expr="enable_pdf_ink2">
import type {AnnotationBrush, BeforeUnloadProxy, InkBrushSelectorElement, InkColorSelectorElement, InkSizeSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {AnnotationBrushType, BeforeUnloadProxyImpl, PluginController, PluginControllerEventType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
// </if>
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
// <if expr="enable_pdf_ink2">
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
// </if>
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

export class MockElement {
  dir: string = '';
  offsetWidth: number;
  offsetHeight: number;
  sizer: MockSizer|null;
  scrollLeft: number = 0;
  scrollTop: number = 0;
  scrollCallback: (() => void)|null = null;
  resizeCallback: (() => void)|null = null;

  constructor(width: number, height: number, sizer: MockSizer|null) {
    this.offsetWidth = width;
    this.offsetHeight = height;
    this.sizer = sizer;

    if (sizer) {
      sizer.resizeCallbackImpl = () =>
          this.scrollTo(this.scrollLeft, this.scrollTop);
    }
  }

  get clientWidth(): number {
    return this.offsetWidth;
  }

  get clientHeight(): number {
    return this.offsetHeight;
  }

  addEventListener(e: string, f: () => void) {
    if (e === 'scroll') {
      this.scrollCallback = f;
    }
  }

  setSize(width: number, height: number) {
    this.offsetWidth = width;
    this.offsetHeight = height;
    this.resizeCallback!();
  }

  scrollTo(x: number, y: number) {
    if (this.sizer) {
      x = Math.min(x, parseInt(this.sizer.style.width, 10) - this.offsetWidth);
      y = Math.min(
          y, parseInt(this.sizer.style.height, 10) - this.offsetHeight);
    }
    this.scrollLeft = Math.max(0, x);
    this.scrollTop = Math.max(0, y);
    this.scrollCallback!();
  }
}

export class MockSizer {
  private width_: string = '0px';
  private height_: string = '0px';

  resizeCallbackImpl: (() => void)|null = null;
  style: {
    height: string,
    width: string,
    display?: string,
  };

  constructor() {
    const sizer = this;

    this.style = {
      get height() {
        return sizer.height_;
      },

      set height(height: string) {
        sizer.height_ = height;
        if (sizer.resizeCallbackImpl) {
          sizer.resizeCallbackImpl();
        }
      },

      get width() {
        return sizer.width_;
      },

      set width(width: string) {
        sizer.width_ = width;
        if (sizer.resizeCallbackImpl) {
          sizer.resizeCallbackImpl();
        }
      },
    };
  }
}

export class MockViewportChangedCallback {
  wasCalled: boolean = false;
  callback: () => void;

  constructor() {
    this.callback = this.callback_.bind(this);
  }

  private callback_() {
    this.wasCalled = true;
  }

  reset() {
    this.wasCalled = false;
  }
}

interface PageDimensions {
  x: number;
  y: number;
  width: number;
  height: number;
}

export class MockDocumentDimensions implements DocumentDimensions {
  width: number;
  height: number;
  layoutOptions?: LayoutOptions;
  pageDimensions: PageDimensions[] = [];

  constructor(width?: number, height?: number, layoutOptions?: LayoutOptions) {
    this.width = width || 0;
    this.height = height || 0;
    this.layoutOptions = layoutOptions;
  }

  addPage(w: number, h: number) {
    let y = 0;
    if (this.pageDimensions.length !== 0) {
      y = this.pageDimensions[this.pageDimensions.length - 1]!.y +
          this.pageDimensions[this.pageDimensions.length - 1]!.height;
    }
    this.width = Math.max(this.width, w);
    this.height += h;
    this.pageDimensions.push({x: 0, y: y, width: w, height: h});
  }

  addPageForTwoUpView(x: number, y: number, w: number, h: number) {
    this.width = Math.max(this.width, 2 * w);
    this.height = Math.max(this.height, y + h);
    this.pageDimensions.push({x: x, y: y, width: w, height: h});
  }

  reset() {
    this.width = 0;
    this.height = 0;
    this.pageDimensions = [];
  }
}

export class MockPdfPluginElement extends HTMLEmbedElement {
  private messages_: any[] = [];
  // <if expr="enable_pdf_ink2">
  private messageReply_: Object|null = null;
  private replyType_: string;
  // </if>

  get messages(): any[] {
    return this.messages_;
  }

  clearMessages() {
    this.messages_.length = 0;
  }

  findMessage(type: string): any {
    return this.messages_.find(element => element.type === type);
  }

  postMessage(message: any, _transfer: Transferable[]) {
    assert(message.type);
    // <if expr="enable_pdf_ink2">
    if (message.type === this.replyType_) {
      assert(this.messageReply_);
      assert(message.messageId);

      this.dispatchEvent(new MessageEvent('message', {
        data: {
          messageId: message.messageId,
          ...this.messageReply_,
        },
        origin: '*',
      }));
    }
    // </if>
    this.messages_.push(message);
  }

  // <if expr="enable_pdf_ink2">
  /**
   * Sets what the plugin's reply should be to a message posted using
   * postMessage() with `type`.
   * @param type The message type that should receive a reply.
   * @param reply The reply to the message.
   */
  setMessageReply(type: string, reply: Object) {
    this.replyType_ = type;
    this.messageReply_ = reply;
  }
  // </if>
}
customElements.define(
    'mock-pdf-plugin', MockPdfPluginElement, {extends: 'embed'});

/**
 * Creates a fake element simulating the PDF plugin.
 */
export function createMockPdfPluginForTest(): MockPdfPluginElement {
  return document.createElement('embed', {is: 'mock-pdf-plugin'}) as
      MockPdfPluginElement;
}

class TestBookmarksElement extends CrLitElement {
  static get is() {
    return 'test-bookmarks';
  }

  override render() {
    return this.bookmarks.map(
        item => html`<viewer-bookmark .bookmark="${item}" depth="0">
             </viewer-bookmark>`);
  }

  static override get properties() {
    return {
      bookmarks: {type: Array},
    };
  }

  bookmarks: Bookmark[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'test-bookmarks': TestBookmarksElement;
  }
}

customElements.define(TestBookmarksElement.is, TestBookmarksElement);

/**
 * @return An element containing a dom-repeat of bookmarks, for
 *     testing the bookmarks outside of the toolbar.
 */
export function createBookmarksForTest(): TestBookmarksElement {
  return document.createElement('test-bookmarks');
}

export class MockMetricsPrivate {
  actionCounter: Map<UserAction, number> = new Map();

  recordValue(metric: chrome.metricsPrivate.MetricType, value: number) {
    chrome.test.assertEq('PDF.Actions', metric.metricName);
    chrome.test.assertEq(
        chrome.metricsPrivate.MetricTypeType.HISTOGRAM_LOG, metric.type);
    chrome.test.assertEq(1, metric.min);
    chrome.test.assertEq(UserAction.NUMBER_OF_ACTIONS, metric.max);
    chrome.test.assertEq(UserAction.NUMBER_OF_ACTIONS + 1, metric.buckets);

    const counter = this.actionCounter.get(value) || 0;
    this.actionCounter.set(value, counter + 1);
  }

  assertCount(action: UserAction, count: number) {
    chrome.test.assertEq(count, this.actionCounter.get(action) || 0);
  }

  reset() {
    resetMetricsForTesting();
    this.actionCounter.clear();
  }
}

export function setupMockMetricsPrivate(): MockMetricsPrivate {
  resetMetricsForTesting();
  const mockMetricsPrivate = new MockMetricsPrivate();
  chrome.metricsPrivate.recordValue =
      mockMetricsPrivate.recordValue.bind(mockMetricsPrivate);
  return mockMetricsPrivate;
}

/**
 * Checks if the PDF title matches the expected title.
 * @param expectedTitle The expected title of the PDF.
 * @return True if the PDF title matches the expected title, false otherwise.
 */
export function checkPdfTitleIsExpectedTitle(expectedTitle: string): boolean {
  const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;
  // Tab title is updated only when document.title is called in a top-level
  // document (`main_frame` of `WebContents`). For OOPIF PDF viewer, the current
  // document is the child of a top-level document, hence document.title is not
  // set and therefore validation is unnecessary.
  if (!viewer.isPdfOopifEnabled && expectedTitle !== document.title) {
    return false;
  }

  return expectedTitle === viewer.pdfTitle;
}

/**
 * Create a viewport with basic default zoom values.
 * @param sizer The element which represents the size of the document in the
 *     viewport.
 * @param scrollbarWidth The width of scrollbars on the page
 * @param defaultZoom The default zoom level.
 * @return The viewport object with zoom values set.
 */
export function getZoomableViewport(
    scrollParent: MockElement, sizer: MockSizer, scrollbarWidth: number,
    defaultZoom: number): Viewport {
  document.body.innerHTML = '';
  const dummyContent = document.createElement('div');
  document.body.appendChild(dummyContent);

  const viewport = new Viewport(
      scrollParent as unknown as HTMLElement, sizer as unknown as HTMLElement,
      dummyContent, scrollbarWidth, defaultZoom);
  viewport.setZoomFactorRange([0.25, 0.4, 0.5, 1, 2]);

  const dummyPlugin = document.createElement('embed');
  dummyPlugin.id = 'plugin';
  dummyPlugin.src = 'data:text/plain,plugin-content';
  viewport.setContent(dummyPlugin);
  return viewport;
}

/**
 * Async spin until predicate() returns true.
 */
export function waitFor(predicate: () => boolean): Promise<void> {
  if (predicate()) {
    return Promise.resolve();
  }
  return new Promise(resolve => setTimeout(() => {
                       resolve(waitFor(predicate));
                     }, 0));
}

export function createWheelEvent(
    deltaY: number, position: {clientX: number, clientY: number},
    ctrlKey: boolean): WheelEvent {
  return new WheelEvent('wheel', {
    deltaY,
    clientX: position.clientX,
    clientY: position.clientY,
    ctrlKey,
    // Necessary for preventDefault() to work.
    cancelable: true,
  });
}

/**
 * Helper to always get a non-null child element of `parent`. The element must
 * exist.
 * @param parent The parent to get the child element from.
 * @param query  The query to get the child element.
 * @returns A non-null element that matches `query`.
 */
export function getRequiredElement<K extends keyof HTMLElementTagNameMap>(
    parent: HTMLElement, query: K): HTMLElementTagNameMap[K];
export function getRequiredElement<E extends HTMLElement = HTMLElement>(
    parent: HTMLElement, query: string): E;
export function getRequiredElement(parent: HTMLElement, query: string) {
  const element = parent.shadowRoot!.querySelector(query);
  assert(element);
  return element;
}

/**
 * Open the toolbar menu. Does nothing if the menu is already open.
 * @param toolbar The toolbar containing the menu to open.
 */
export async function openToolbarMenu(toolbar: ViewerToolbarElement) {
  const menu = toolbar.$.menu;
  if (menu.open) {
    return;
  }

  getRequiredElement(toolbar, '#more').click();
  await microtasksFinished();
  assert(menu.open);
}

/**
 * Check that the checkbox menu `button` in `toolbar` matches the `checked`
 * state.
 */
export function assertCheckboxMenuButton(
    toolbar: ViewerToolbarElement, button: HTMLElement, checked: boolean) {
  chrome.test.assertTrue(toolbar.$.menu.open);

  // Check that the check mark visibility matches `checked`.
  chrome.test.assertEq(String(checked), button.getAttribute('aria-checked'));
  chrome.test.assertEq(
      checked, isVisible(button.querySelector('.check-container cr-icon')));
}

export async function ensureFullscreen(): Promise<void> {
  const viewer = document.body.querySelector('pdf-viewer');
  assert(viewer);

  if (document.fullscreenElement !== null) {
    return;
  }

  const toolbar = viewer.shadowRoot!.querySelector('viewer-toolbar');
  assert(toolbar);
  toolbar.dispatchEvent(new CustomEvent('present-click'));
  await eventToPromise('fullscreenchange', viewer.$.scroller);
}

// Subsequent calls to requestFullScreen() fail with an "API can only be
// initiated by a user gesture" error, so we need to run with user
// gesture.
export function enterFullscreenWithUserGesture(): Promise<void> {
  return new Promise(res => {
    chrome.test.runWithUserGesture(() => {
      ensureFullscreen().then(res);
    });
  });
}

// <if expr="enable_pdf_ink2">
/**
 * Helper to simulate the PDF content sending a message to the PDF extension
 * to indicate that a new ink stroke has been drawn.
 */
export function finishInkStroke(controller: PluginController) {
  const eventTarget = controller.getEventTarget();
  const message = {type: 'finishInkStroke'};

  eventTarget.dispatchEvent(new CustomEvent(
      PluginControllerEventType.PLUGIN_MESSAGE, {detail: message}));
}

export class TestBeforeUnloadProxy extends TestBrowserProxy implements
    BeforeUnloadProxy {
  constructor() {
    super(['preventDefault']);
  }

  preventDefault() {
    this.methodCalled('preventDefault');
  }
}

export function getNewTestBeforeUnloadProxy(): TestBeforeUnloadProxy {
  const testProxy = new TestBeforeUnloadProxy();
  BeforeUnloadProxyImpl.setInstance(testProxy);
  return testProxy;
}

export function setupTestMockPluginForInk(): MockPdfPluginElement {
  const controller = PluginController.getInstance();
  const mockPlugin = createMockPdfPluginForTest();
  controller.setPluginForTesting(mockPlugin);
  mockPlugin.setMessageReply('getAnnotationBrush', {
    data: {
      type: AnnotationBrushType.PEN,
      size: 3,
      color: {r: 0, g: 0, b: 0},
    },
  });
  return mockPlugin;
}

/**
 * Sets the reply to any getAnnotationBrush messages to `mockPlugin`.
 * @param mockPlugin The mock plugin receiving and replying to messages.
 * @param type The brush type in the reply message.
 * @param size The brush size in the reply message.
 * @param color The brush color in the reply message.
 */
export function setGetAnnotationBrushReply(
    mockPlugin: MockPdfPluginElement, type: AnnotationBrushType, size: number,
    color?: {r: number, g: number, b: number}) {
  mockPlugin.setMessageReply('getAnnotationBrush', {data: {type, size, color}});
}

/**
 * Tests that the current annotation brush matches `expectedBrush`. Clears all
 * messages from `mockPlugin` after, otherwise subsequent calls would continue
 * to find and use the same message.
 * @param mockPlugin The mock plugin receiving messages.
 * @param expectedBrush The expected brush that the current annotation brush
 * should match.
 */
export function assertAnnotationBrush(
    mockPlugin: MockPdfPluginElement, expectedBrush: AnnotationBrush) {
  const setAnnotationBrushMessage =
      mockPlugin.findMessage('setAnnotationBrush');
  chrome.test.assertTrue(setAnnotationBrushMessage !== undefined);
  chrome.test.assertEq('setAnnotationBrush', setAnnotationBrushMessage.type);
  chrome.test.assertEq(expectedBrush.type, setAnnotationBrushMessage.data.type);
  const hasColor = expectedBrush.color !== undefined;
  chrome.test.assertEq(
      hasColor, setAnnotationBrushMessage.data.color !== undefined);
  if (hasColor) {
    chrome.test.assertEq(
        expectedBrush.color!.r, setAnnotationBrushMessage.data.color.r);
    chrome.test.assertEq(
        expectedBrush.color!.g, setAnnotationBrushMessage.data.color.g);
    chrome.test.assertEq(
        expectedBrush.color!.b, setAnnotationBrushMessage.data.color.b);
  }
  chrome.test.assertEq(expectedBrush.size, setAnnotationBrushMessage.data.size);

  mockPlugin.clearMessages();
}

/**
 * @param parentElement The parent element containing the
 *     InkBrushSelectorElement.
 * @returns The non-null brush type selector.
 */
export function getBrushSelector(parentElement: HTMLElement):
    InkBrushSelectorElement {
  return getRequiredElement(parentElement, 'ink-brush-selector');
}


/**
 * Helper to get a non-empty list of brush size buttons.
 * @param selector The ink size selector element.
 * @returns A list of exactly 5 size buttons.
 */
export function getSizeButtons(selector: InkSizeSelectorElement):
    NodeListOf<HTMLElement> {
  const sizeButtons =
      selector.shadowRoot!.querySelectorAll<HTMLElement>('cr-icon-button');
  assert(sizeButtons);
  assert(sizeButtons.length === 5);
  return sizeButtons;
}

/**
 * Tests that the ink size options have correct values for the selected
 * attribute. The size button with index `buttonIndex` should be selected.
 * @sizeButtons A list of ink size buttons.
 * @param buttonIndex The expected selected size button.
 */
export function assertSelectedSize(
    sizeButtons: NodeListOf<HTMLElement>, buttonIndex: number) {
  for (let i = 0; i < sizeButtons.length; ++i) {
    const buttonSelected = sizeButtons[i].dataset['selected'];
    chrome.test.assertEq(i === buttonIndex ? 'true' : 'false', buttonSelected);
  }
}

/**
 * Helper to get a non-empty list of brush color buttons.
 * @param selector The ink color selector element.
 * @returns A list of color buttons.
 */
export function getColorButtons(selector: InkColorSelectorElement):
    NodeListOf<HTMLElement> {
  const colorButtons = selector.shadowRoot!.querySelectorAll('input');
  assert(colorButtons);
  return colorButtons;
}

/**
 * Tests that the color options have corrected values for the selected
 * attribute. The color button with index `buttonIndex` should be selected.
 * @param colorButtons A list of ink color buttons.
 * @param buttonIndex The expected selected color button.
 */
export function assertSelectedColor(
    colorButtons: NodeListOf<HTMLElement>, buttonIndex: number) {
  for (let i = 0; i < colorButtons.length; ++i) {
    chrome.test.assertEq(
        i === buttonIndex, colorButtons[i].hasAttribute('checked'));
  }
}

// </if>
