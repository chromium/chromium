// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {AppElement, SettingsPrefs, SimpleActionMenuElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {DEFAULT_SETTINGS, MetricsBrowserProxyImpl, NodeStore, playFromSelectionTimeout, ReadAloudNode, ReadAnythingLogger, ToolbarEvent, VoiceLanguageController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {Segment} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertNotDeepEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {MockTimer} from 'chrome-untrusted://webui-test/mock_timer.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';
import type {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';
import type {TestSpeechBrowserProxy} from './test_speech_browser_proxy.js';

export const TEST_RANDOM_VALUE_SETTINGS: SettingsPrefs = {
  letterSpacing: 101,
  lineSpacing: 102,
  theme: 103,
  speechRate: 104,
  font: 'font',
  highlightGranularity: 105,
  lineFocus: 106,
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

export function emitEvent(app: AppElement, name: string, options?: any): void {
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

export function assertHeadersForDropdown(
    dropdown: SimpleActionMenuElement, shouldHaveHeaders: boolean): void {
  const headers =
      dropdown.$.lazyMenu.get().querySelector<HTMLElement>('.has-header-true');
  assertEquals(shouldHaveHeaders, !!headers);
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
