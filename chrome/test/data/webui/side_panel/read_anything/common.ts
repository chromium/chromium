// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {AppElement, SettingsPrefs} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {AudioBrowserProxyImpl, BrowserProxy, ContentBrowserProxyImpl, ContentController, DEFAULT_SETTINGS, LineFocusController, MetricsBrowserProxyImpl, NodeStore, playFromSelectionTimeout, ReadAloudHighlighter, ReadAloudNode, ReadAloudNodeStore, ReadAnythingLogger, SelectionController, setInstance, SpeechBrowserProxyImpl, SpeechController, TextSegmenter, ToolbarEvent, VisualBrowserProxyImpl, VoiceLanguageController, VoiceNotificationManager, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {Segment} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotDeepEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
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
  lineFocusEnabled?: boolean;
}

// Initializes all singletons, mocks, and controllers in their strict
// dependency order without creating an AppElement.
export function setupTestEnvironment(flags?: AppTestFlags): TestSetupResult {
  // Clearing the DOM must always be done first.
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  window.scrollTo(0, 0);

  // Browser Proxies: no dependencies.
  const browserProxy = new TestColorUpdaterBrowserProxy();
  BrowserProxy.setInstance(browserProxy);
  const visualBrowserProxy = new TestVisualBrowserProxy();
  if (flags?.readAnythingImprovedUiEnabled !== undefined) {
    visualBrowserProxy.readAnythingImprovedUiEnabled =
        flags.readAnythingImprovedUiEnabled;
  }
  if (flags?.lineFocusEnabled !== undefined) {
    visualBrowserProxy.lineFocusEnabled = flags.lineFocusEnabled;
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
  setInstance(readAloudModel);
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

export const TEST_RANDOM_VALUE_SETTINGS: SettingsPrefs = {
  letterSpacing: 101,
  lineSpacing: 102,
  theme: 103,
  speechRate: 104,
  font: 'font',
  highlightGranularity: 105,
  linksEnabled: true,
  imagesEnabled: false,
};

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

// Runs the requestAnimationFrame callback immediately
export function stubAnimationFrame() {
  window.requestAnimationFrame = (callback) => {
    callback(0);
    return 0;
  };
}

export function playFromSelectionWithMockTimer(app: AppElement): void {
  const mockTimer = new MockTimer();
  mockTimer.install();
  emitEvent(app, ToolbarEvent.PLAY_PAUSE);
  mockTimer.tick(playFromSelectionTimeout);
  mockTimer.uninstall();
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

function assertCheckMarkVisible(
    checkMarks: NodeListOf<HTMLElement>, expectedIndex: number): void {
  checkMarks.forEach((element, index) => {
    assertEquals(
        index === expectedIndex ? 'visible' : 'hidden',
        element.style.visibility);
  });
}

export function assertCheckMarksForDropdown(dropdown: HTMLElement): void {
  const buttons =
      dropdown.querySelectorAll<HTMLButtonElement>('.dropdown-item');
  const checkMarks = dropdown.querySelectorAll<HTMLElement>('.check-mark');
  assertEquals(buttons.length, checkMarks.length);
  buttons.forEach((button, index) => {
    button.click();
    assertCheckMarkVisible(checkMarks, index);
  });
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
      [{node: ReadAloudNode.create(node)!, start: 0, length: text.length}];
  if (model) {
    model.setCurrentTextSegments(segments);
    model.setCurrentTextContent(text);
  }
  return node;
}

export function assertTestSettingsAreNotDefaultSettings() {
  assertNotDeepEquals(DEFAULT_SETTINGS, TEST_RANDOM_VALUE_SETTINGS);
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
