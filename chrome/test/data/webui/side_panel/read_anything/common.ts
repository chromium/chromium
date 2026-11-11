// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AudioBrowserProxyImpl, BrowserProxy, ContentBrowserProxyImpl, ContentController, LineFocusController, MetricsBrowserProxyImpl, NodeStore, playFromSelectionTimeout, ReadAloudHighlighter, ReadAloudNode, ReadAloudNodeStore, ReadAnythingLogger, SelectionController, setInstance, SpeechBrowserProxyImpl, SpeechController, TextSegmenter, ToolbarEvent, VisualBrowserProxyImpl, VoiceLanguageController, VoiceNotificationManager, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {Segment} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertStringContains, assertStringExcludes, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {TestAudioBrowserProxy} from './test_audio_browser_proxy.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestContentBrowserProxy} from './test_content_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
import {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';
import {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

export interface TestSetupResult {
  audioBrowserProxy: TestAudioBrowserProxy;
  browserProxy: TestColorUpdaterBrowserProxy;
  contentBrowserProxy: TestContentBrowserProxy;
  contentController: ContentController;
  highlighter: ReadAloudHighlighter;
  lineFocusController: LineFocusController;
  logger: ReadAnythingLogger;
  metrics: TestMetricsBrowserProxy;
  nodeStore: NodeStore;
  notificationManager: VoiceNotificationManager;
  readAloudModel: TestReadAloudModelBrowserProxy;
  readAloudNodeStore: ReadAloudNodeStore;
  selectionController: SelectionController;
  speech: TestSpeechBrowserProxy;
  speechController: SpeechController;
  textSegmenter: TextSegmenter;
  visualBrowserProxy: TestVisualBrowserProxy;
  voiceLanguageController: VoiceLanguageController;
  wordBoundaries: WordBoundaries;
}

export interface AppTestSetupResult extends TestSetupResult {
  app: AppElement;
}

export interface AppTestFlags {
  readAnythingImprovedUiEnabled?: boolean;
  // Uses the real ReadAloud model instead of TestReadAloudModelBrowserProxy.
  // Only for integration tests that need text segmented from the DOM.
  useRealReadAloudModel?: boolean;
}

// Saved so stubAnimationFrame() doesn't leak into later tests.
const originalRequestAnimationFrame = window.requestAnimationFrame;

// Clearing the DOM doesn't remove listeners on document, so
// setupTestEnvironment() aborts this to remove ones from earlier tests.
let documentListenerController = new AbortController();

// Initializes all singletons, mocks, and controllers in their strict
// dependency order without creating an AppElement.
export function setupTestEnvironment(flags?: AppTestFlags): TestSetupResult {
  // Remove listeners from earlier tests first, so none of them can run while
  // the DOM is cleared.
  documentListenerController.abort();
  documentListenerController = new AbortController();

  // Clearing the DOM must be done before anything else is set up.
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  window.scrollTo(0, 0);
  window.requestAnimationFrame = originalRequestAnimationFrame;

  // Browser Proxies: no dependencies.
  const browserProxy = new TestColorUpdaterBrowserProxy();
  BrowserProxy.setInstance(browserProxy);
  const visualBrowserProxy = new TestVisualBrowserProxy();
  if (flags?.readAnythingImprovedUiEnabled !== undefined) {
    visualBrowserProxy.readAnythingImprovedUiEnabled =
        flags.readAnythingImprovedUiEnabled;
  }
  VisualBrowserProxyImpl.setInstance(visualBrowserProxy);
  const contentBrowserProxy = new TestContentBrowserProxy();
  ContentBrowserProxyImpl.setInstance(contentBrowserProxy);
  const audioBrowserProxy = new TestAudioBrowserProxy();
  AudioBrowserProxyImpl.setInstance(audioBrowserProxy);
  const speech = new TestSpeechBrowserProxy();
  SpeechBrowserProxyImpl.setInstance(speech);

  // These depend on browser proxies only.
  const metrics = mockMetrics();
  const logger = ReadAnythingLogger.getInstance();
  const readAloudModel = new TestReadAloudModelBrowserProxy();
  // A null instance makes getReadAloudModel() lazily create the real model.
  setInstance(flags?.useRealReadAloudModel ? null : readAloudModel);
  const nodeStore = new NodeStore();
  NodeStore.setInstance(nodeStore);
  const readAloudNodeStore = new ReadAloudNodeStore();
  ReadAloudNodeStore.setInstance(readAloudNodeStore);
  const notificationManager = new VoiceNotificationManager();
  VoiceNotificationManager.setInstance(notificationManager);
  const wordBoundaries = new WordBoundaries();
  WordBoundaries.setInstance(wordBoundaries);
  const textSegmenter = new TextSegmenter();
  TextSegmenter.setInstance(textSegmenter);

  // Controllers that only depend on the above 2 section.
  const selectionController = new SelectionController();
  SelectionController.setInstance(selectionController);
  const voiceLanguageController = new VoiceLanguageController();
  VoiceLanguageController.setInstance(voiceLanguageController);

  // Depends on VoiceLanguageController.
  const highlighter = new ReadAloudHighlighter();
  ReadAloudHighlighter.setInstance(highlighter);

  // Depends on ReadAloudHighlighter.
  const speechController = new SpeechController();
  SpeechController.setInstance(speechController);

  // Controllers that depend on SpeechController.
  const lineFocusController = new LineFocusController();
  LineFocusController.setInstance(lineFocusController);
  const contentController = new ContentController();
  ContentController.setInstance(contentController);

  return {
    audioBrowserProxy,
    browserProxy,
    contentBrowserProxy,
    contentController,
    highlighter,
    lineFocusController,
    logger,
    metrics,
    nodeStore,
    notificationManager,
    readAloudModel,
    readAloudNodeStore,
    selectionController,
    speech,
    speechController,
    textSegmenter,
    visualBrowserProxy,
    voiceLanguageController,
    wordBoundaries,
  };
}

// Initializes all singletons, mocks, and controllers in their strict
// dependency order and creates the AppElement for tests that require
// the full Read Anything app environment.
export async function setupAppTestEnvironment(flags?: AppTestFlags):
    Promise<AppTestSetupResult> {
  const result = setupTestEnvironment(flags);
  const app = await createApp();

  return {
    app,
    ...result,
  };
}

export async function createApp(): Promise<AppElement> {
  const app = document.createElement('read-anything-app');
  document.body.appendChild(app);
  await microtasksFinished();
  return app;
}

export function mockMetrics(): TestMetricsBrowserProxy {
  const metrics = new TestMetricsBrowserProxy();
  MetricsBrowserProxyImpl.setInstance(metrics);
  ReadAnythingLogger.setInstance(new ReadAnythingLogger());
  return metrics;
}

export function emitEvent(
    app: AppElement, name: string, options?: CustomEventInit): void {
  app.$.toolbar.dispatchEvent(new CustomEvent(name, options));
}

// Adds a document listener that the next setupTestEnvironment() call removes.
export function addDocumentListener(type: string, listener: EventListener) {
  document.addEventListener(
      type, listener, {signal: documentListenerController.signal});
}

// Runs the requestAnimationFrame callback immediately
export function stubAnimationFrame() {
  window.requestAnimationFrame = (callback) => {
    callback(0);
    return 0;
  };
}

// Runs `play` and skips the delay before playing from a selection or line
// focus position.
export function playWithMockTimer(play: () => void): void {
  const mockTimer = new MockTimer();
  mockTimer.install();
  play();
  mockTimer.tick(playFromSelectionTimeout);
  mockTimer.uninstall();
}

export function playFromSelectionWithMockTimer(app: AppElement): void {
  playWithMockTimer(() => emitEvent(app, ToolbarEvent.PLAY_PAUSE));
}

// Returns the list of items in the given dropdown menu
export function getItemsInMenu(
    lazyMenu: CrLazyRenderLitElement<CrActionMenuElement>):
    HTMLButtonElement[] {
  // We need to call menu.get here to ensure the menu has rendered before we
  // query the dropdown item elements.
  const menu = lazyMenu.get();
  return Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
}

export function createSpeechErrorEvent(
    utterance: SpeechSynthesisUtterance,
    errorCode: SpeechSynthesisErrorCode): SpeechSynthesisErrorEvent {
  return new SpeechSynthesisErrorEvent(
      'type', {utterance: utterance, error: errorCode});
}

export function createWordBoundaryEvent(
    utterance: SpeechSynthesisUtterance, charIndex: number,
    charLength?: number) {
  return new SpeechSynthesisEvent(
      'type', {name: 'word', utterance, charIndex, charLength});
}

export function setupBasicSpeech(speech: TestSpeechBrowserProxy) {
  VoiceLanguageController.getInstance().enableLang('en');
  createAndSetVoices(
      speech, [{lang: 'en', name: 'Google Basic', default: true}]);
}

// Creates SpeechSynthesisVoices and sets them on the given
// TestSpeechBrowserProxy.
export function createAndSetVoices(
    speech: TestSpeechBrowserProxy,
    overrides: Array<Partial<SpeechSynthesisVoice>>) {
  const voices: SpeechSynthesisVoice[] = [];
  overrides.forEach(partialVoice => {
    voices.push(createSpeechSynthesisVoice(partialVoice));
  });
  setVoices(speech, voices);
}

export function setVoices(
    speech: TestSpeechBrowserProxy, voices: SpeechSynthesisVoice[]) {
  speech.setVoices(voices);
  VoiceLanguageController.getInstance().onVoicesChanged();
}

export function createSpeechSynthesisVoice(
    overrides?: Partial<SpeechSynthesisVoice>): SpeechSynthesisVoice {
  return Object.assign(
      {
        default: false,
        name: '',
        lang: 'en-us',
        localService: false,
        voiceURI: '',
      },
      overrides || {});
}

export function setContent(
    text: string, model: TestReadAloudModelBrowserProxy): Node {
  const id = 2;
  const node = document.createTextNode(text);
  NodeStore.getInstance().setDomNode(node, id);
  const segments: Segment[] =
      [{node: ReadAloudNode.create(node), start: 0, length: text.length}];
  if (model) {
    model.setCurrentTextSegments(segments);
    model.setCurrentTextContent(text);
  }
  return node;
}

// Stores a new <p> element in the NodeStore for each of the given ids.
export function setDomNodes(axNodeIds: number[]): Node[] {
  return axNodeIds.map(id => {
    const element = document.createElement('p');
    NodeStore.getInstance().setDomNode(element, id);
    return element;
  });
}

// Plays or pauses speech for a new <p> with the given text. If a model is
// given, also sets the text as the current content.
export function onPlayPauseToggle(
    text: string, model?: TestReadAloudModelBrowserProxy): HTMLElement {
  if (model) {
    setContent(text, model);
  }
  const element = document.createElement('p');
  element.textContent = text;
  SpeechController.getInstance().onPlayPauseToggle(element);
  return element;
}

// Returns the trimmed text of the speak call at `callIndex`, which must be
// the latest call.
export function getSpokenText(
    speech: TestSpeechBrowserProxy, callIndex = 0): string {
  assertEquals(callIndex + 1, speech.getCallCount('speak'));
  return speech.getArgs('speak')[callIndex].text.trim();
}

function getInnerHtml(axNodeId: number): string {
  const node = NodeStore.getInstance().getDomNode(axNodeId);
  assertTrue(node instanceof Element);
  return node.innerHTML;
}

export function assertHtml(html: string, axNodeId: number) {
  assertEquals(html, getInnerHtml(axNodeId));
}

export function assertHtmlContains(partialHtml: string, axNodeId: number) {
  assertStringContains(getInnerHtml(axNodeId), partialHtml);
}

export function assertHtmlExcludes(partialHtml: string, axNodeId: number) {
  assertStringExcludes(getInnerHtml(axNodeId), partialHtml);
}

// Compares font names, ignoring case, quotes, and surrounding whitespace.
export function assertFontsEqual(actual: string, expected: string): void {
  assertEquals(
      expected.trim().toLowerCase().replaceAll('"', ''),
      actual.trim().toLowerCase().replaceAll('"', ''));
}

export function getLineFocusPadding(app: AppElement): number {
  const val = app.style.getPropertyValue('--line-focus-padding');
  return val ? parseInt(val) : 0;
}

// Styles a line focus test container so each text line is 40px tall.
export function applyContainerStyles(container: HTMLElement): void {
  container.style.whiteSpace = 'pre';
  container.style.width = 'max-content';
  container.style.margin = '0';
  container.style.fontSize = '20px';
  container.style.lineHeight = '2';
}

// Adds a styled <p> with 3 lines of text to the page for line focus tests.
export function createShortContainer(): HTMLElement {
  const container = document.createElement('p');
  container.innerText =
      'I\'ve heard it said\nThat people come into our lives\nfor a reason.\n';
  applyContainerStyles(container);
  document.body.appendChild(container);
  return container;
}

export function setWindowSize(height: number, width: number) {
  if (Object.getOwnPropertyDescriptor(window, 'innerHeight')?.configurable !==
      false) {
    Object.defineProperty(window, 'innerHeight', {
      value: height,
      configurable: true,
    });
  }
  if (Object.getOwnPropertyDescriptor(window, 'innerWidth')?.configurable !==
      false) {
    Object.defineProperty(window, 'innerWidth', {
      value: width,
      configurable: true,
    });
  }
}
