// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InputState} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

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
    clazz: Constructor<T>, installer?: Installer<T>): TestMock<T> {
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

export function createInputState(overrides?: Partial<InputState>): InputState {
  return Object.assign(
      {
        allowedTools: [],
        disabledTools: [],
        toolConfigs: [],
        toolsSectionConfig: {header: ''},
        allowedModels: [],
        disabledModels: [],
        activeModel: 0,
        activeTool: 0,
        hintText: '',
        modelConfigs: [],
        modelSectionConfig: {header: ''},
        allowedInputTypes: [],
        disabledInputTypes: [],
        inputTypeConfigs: [],
        maxInstances: {},
        maxTotalInputs: 0,
      },
      overrides);
}

export function createValidInputState(): InputState {
  return createInputState({
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
      },
    ],
    modelConfigs: [
      {
        model: ModelMode.kGeminiPro,
        menuLabel: 'Gemini Pro',
        hintText: 'Gemini Pro hint',
        aimUrlParams: [],
      },
    ],
    modelSectionConfig: {
      header: 'Models',
    },
  });
}
