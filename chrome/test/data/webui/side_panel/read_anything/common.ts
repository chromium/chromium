// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {flush} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {AppElement} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {MetricsBrowserProxyImpl, playFromSelectionTimeout} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {FakeSpeechSynthesis} from './fake_speech_synthesis.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

export function mockMetrics(): TestMetricsBrowserProxy {
  const metrics = new TestMetricsBrowserProxy();
  MetricsBrowserProxyImpl.setInstance(metrics);
  return metrics;
}

// TODO(crbug.com/40927698): Remove this function.
export function emitEvent(app: AppElement, name: string, options?: any): void {
  emitEventWithTarget(app.$.toolbar, name, options);
}

export function emitEventWithTarget(
    target: HTMLElement, name: string, options?: any): void {
  target.dispatchEvent(new CustomEvent(name, options));
  flush();
}

/**
 * Suppresses harmless ResizeObserver errors due to a browser bug.
 * yaqs/2300708289911980032
 */
export function suppressInnocuousErrors() {
  const onerror = window.onerror;
  window.onerror = (message, url, lineNumber, column, error) => {
    if ([
          'ResizeObserver loop limit exceeded',
          'ResizeObserver loop completed with undelivered notifications.',
        ].includes(message.toString())) {
      console.info('Suppressed ResizeObserver error: ', message);
      return;
    }
    if (onerror) {
      onerror.apply(window, [message, url, lineNumber, column, error]);
    }
  };
}

// Runs the requestAnimationFrame callback immediately
export function stubAnimationFrame() {
  window.requestAnimationFrame = (callback) => {
    callback(0);
    return 0;
  };
}

export async function waitForPlayFromSelection(): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, playFromSelectionTimeout));
}

// Returns the list of items in the given dropdown menu
export function getItemsInMenu(
    lazyMenu: CrLazyRenderElement<CrActionMenuElement>): HTMLButtonElement[] {
  // We need to call menu.get here to ensure the menu has rendered before we
  // query the dropdown item elements.
  const menu = lazyMenu.get();
  flush();
  return Array.from(menu.querySelectorAll<HTMLButtonElement>('.dropdown-item'));
}

// Creates SpeechSynthesisVoices and sets them on the given FakeSpeechSynthesis.
export function createAndSetVoices(
    app: AppElement, speechSynthesis: FakeSpeechSynthesis,
    overrides: Array<Partial<SpeechSynthesisVoice>>) {
  const voices: SpeechSynthesisVoice[] = [];
  overrides.forEach(partialVoice => {
    voices.push(createSpeechSynthesisVoice(partialVoice));
  });
  setVoices(app, speechSynthesis, voices);
}

export function setVoices(
    app: AppElement, speechSynthesis: FakeSpeechSynthesis,
    voices: SpeechSynthesisVoice[]) {
  speechSynthesis.setVoices(voices);
  app.onVoicesChanged();
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


export function setSimpleAxTreeWithText(text: string) {
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2],
      },
      {id: 2, role: 'staticText', name: text},
    ],
  };
  chrome.readingMode.setContentForTesting(axTree, [2]);
}
