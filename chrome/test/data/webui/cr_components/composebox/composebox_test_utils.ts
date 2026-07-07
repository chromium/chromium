// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import type {ComposeboxVoiceSearchElement, VoiceSearchError} from 'chrome://resources/cr_components/composebox/composebox_voice_search.js';
import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {ContextUploadStatus, InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

// TODO(crbug.com/452983498): This is a copy of MockInputState from
// searchbox_test_utils.ts to decouple composebox test builds from searchbox
// and the Desktop New Tab Page bundle, which cannot be built on Android yet.
// LINT.IfChange
export class MockInputState implements InputState {
  allowedTools: number[] = [];
  disabledTools: number[] = [];
  activeTool: number = 0;
  toolConfigs: any[] = [
    {
      tool: ToolMode.kDeepSearch,
      hintText: 'Research anything',
      menuLabel: '',
      chipLabel: '',
      disableActiveModelSelection: false,
      aimUrlParams: [],
      menuTooltip: '',
    },
    {
      tool: ToolMode.kImageGen,
      hintText: 'Describe your image',
      menuLabel: '',
      chipLabel: '',
      disableActiveModelSelection: false,
      aimUrlParams: [],
      menuTooltip: '',
    },
    {
      tool: ToolMode.kCanvas,
      hintText: 'Create anything',
      menuLabel: '',
      chipLabel: '',
      disableActiveModelSelection: false,
      aimUrlParams: [],
      menuTooltip: '',
    },
  ];
  toolsSectionConfig: any|null = null;

  allowedModels: number[] = [];
  disabledModels: number[] = [];
  activeModel: number = 0;
  modelConfigs: any[] = [];
  modelSectionConfig: any|null = null;

  allowedInputTypes: number[] = [];
  disabledInputTypes: number[] = [];
  inputTypeConfigs: any[] = [];
  maxInputsByType: {[key: number]: number} = {};
  maxTotalInputs: number = 0;
  isCanvasQuerySubmitted: boolean = false;

  hintText: string = '';

  constructor(overrides?: Partial<InputState>) {
    Object.assign(this, overrides);
  }
}
// LINT.ThenChange(//chrome/test/data/webui/cr_components/searchbox/searchbox_test_utils.ts)
export type {InputState};

const THOUSANDTHS = 0.001;

/**
 * Asserts the computed style value for an element.
 * @param name The name of the style to assert.
 * @param expected The expected style value.
 */
export function assertStyle(element: Element, name: string, expected: string) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual);
}

import type {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';

export class MockSpeechRecognition {
  voiceSearchInProgress: boolean = false;
  startCount: number = 0;
  onresult: ((this: MockSpeechRecognition, ev: any) => void)|null = null;
  onend: (() => void)|null = null;
  onerror: ((this: MockSpeechRecognition, ev: any) => void)|null = null;
  onaudiostart: ((this: MockSpeechRecognition, ev: any) => void)|null = null;
  onspeechstart: ((this: MockSpeechRecognition, ev: any) => void)|null = null;
  onnomatch: ((this: MockSpeechRecognition, ev: any) => void)|null = null;
  interimResults = true;
  continuous = false;
  constructor() {
    mockSpeechRecognition = this;
  }
  start() {
    this.voiceSearchInProgress = true;
    this.startCount++;
  }
  stop() {
    this.voiceSearchInProgress = false;
  }
  abort() {
    this.voiceSearchInProgress = false;
    this.onend!();
  }
}

export let mockSpeechRecognition: MockSpeechRecognition;

type Constructor<T> = new (...args: any[]) => T;
type Installer<T> = (instance: T) => void;

export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): T&TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);

  if (clazz.name === 'WindowProxy') {
    const windowProxy = mock as unknown as TestMock<WindowProxy>;
    windowProxy.setResultMapperFor('createSpeechRecognition', () => {
      return new MockSpeechRecognition() as unknown as SpeechRecognition;
    });
  }

  installer(mock);
  return mock;
}

export function assertAlmostEquals(
    expected: number, actual: number, delta: number = THOUSANDTHS) {
  const diff = Math.abs(actual - expected);
  assertTrue(
      diff <= delta,
      `Mismatch in almostEquals. Expected ${expected} with delta: ${
          delta}. Got ${actual}\
       with diff ${diff}.`);
}

export function createValidInputState(): InputState {
  return new MockInputState({
    allowedModels: [ModelMode.kGeminiPro],
    allowedTools: [ToolMode.kDeepSearch],
    allowedInputTypes: [InputType.kBrowserTab],
    activeModel: ModelMode.kUnspecified,
    activeTool: ToolMode.kUnspecified,
    toolConfigs: [
      {
        tool: ToolMode.kDeepSearch,
        menuLabel: 'Deep Search',
        chipLabel: 'Deep Search',
        hintText: 'Deep Search hint',
        disableActiveModelSelection: false,
        aimUrlParams: [],
        menuTooltip: '',
      },
    ],
    toolsSectionConfig: {header: ''},
    modelConfigs: [
      {
        model: ModelMode.kGeminiPro,
        menuLabel: 'Gemini Pro',
        hintText: 'Gemini Pro hint',
        aimUrlParams: [],
        menuTooltip: '',
      },
    ],
    modelSectionConfig: {
      header: 'Models',
    },
  });
}

import type {PageCallbackRouter as SearchboxPageCallbackRouter} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

export type MockComposebox = Omit<
    ComposeboxElement,
    'transcript'|'inVoiceSearchMode'|'searchboxCallbackRouter_'>&{
  inVoiceSearchMode: boolean,
  transcript: string,
  searchboxCallbackRouter_: SearchboxPageCallbackRouter,
};

// Chose type extension instead of just making separate mock public interface.
// This is done to keep the rest of the class.
export type MockComposeboxVoiceSearch = Omit<
    ComposeboxVoiceSearchElement,
    'state_'|'voiceRecognition_'|'onFinalResult_'|'onCloseClick_'|'onEnd_'|
    'onTryAgainClick_'|'onLinkClick_'|'errorMessage_'|'voiceModeEndCleanup_'|
    'detailedError_'>&{
  state_: number,
  metricSource_: string,
  voiceRecognition_: MockSpeechRecognition,
  errorMessage_: string,
  detailedError_: VoiceSearchError | null,
  voiceModeEndCleanup_: () => void,
  onFinalResult_: (result: string, forceSubmit?: boolean) => void,
  onCloseClick_: () => void,
  onEnd_: () => void,
  onTryAgainClick_: (e: Event) => void,
  onLinkClick_: (e: Event) => void,
};

export function disableTransitionsRecursively(
    root: HTMLElement|null|undefined) {
  if (!root) {
    return;
  }
  const queue: Array<HTMLElement|ShadowRoot> = [root];
  while (queue.length > 0) {
    const current = queue.shift()!;
    if (current instanceof HTMLElement) {
      current.style.transition = 'none';
      current.style.animation = 'none';
      current.style.animationDuration = '0s';
      current.style.transitionDuration = '0s';
      for (let i = 0; i < current.children.length; i++) {
        queue.push(current.children[i] as HTMLElement);
      }
      if (current.shadowRoot) {
        queue.push(current.shadowRoot);
      }
    } else if (current instanceof ShadowRoot) {
      const style = document.createElement('style');
      style.textContent = `
        * {
          transition: none !important;
          transition-duration: 0s !important;
          animation: none !important;
          animation-duration: 0s !important;
        }
      `;
      current.appendChild(style);
      for (let i = 0; i < current.children.length; i++) {
        queue.push(current.children[i] as HTMLElement);
      }
    }
  }
}

export function createFile(
    index: number, override: Partial<ComposeboxFile> = {}): ComposeboxFile {
  return Object.assign(
      {
        name: `file${index}.txt`,
        type: 'text/plain',
        inputType: InputType.kLensFile,
        objectUrl: null,
        dataUrl: null,
        uuid: {high: 0n, low: BigInt(index)} as unknown as UnguessableToken,
        status: ContextUploadStatus.kUploadSuccessful,
        url: null,
        tabId: null,
        isDeletable: true,
        iconName: null,
        supportsUnimodal: true,
      },
      override);
}
