// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
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

type Constructor<T> = new (...args: any[]) => T;
type Installer<T> = (instance: T) => void;

export function installMock<T extends object>(
    clazz: Constructor<T>, installer?: Installer<T>): T&TestMock<T> {
  installer = installer ||
      (clazz as unknown as {setInstance: Installer<T>}).setInstance;
  const mock = TestMock.fromClass(clazz);
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
