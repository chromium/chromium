// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {ToolMode as ComposeboxToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals} from '//webui-test/chai_assert.js';

export type {InputState};

/**
 * Asserts the computed style value for an element.
 * @param name The name of the style to assert.
 * @param expected The expected style value.
 */
export function assertStyle(element: Element, name: string, expected: string) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual);
}

/**
 * Waits for the specified attribute of a given element to change.
 * @param attributeName The attribute to observe.
 * @param initialValue The value to compare against to detect the attribute
 *     change.
 */
export function waitForAttributeChange(
    element: HTMLElement, attributeName: string, initialValue: string) {
  return new Promise((resolve) => {
    // Create a MutationObserver to watch for attribute changes.
    const observer = new MutationObserver((mutations) => {
      for (const mutation of mutations) {
        if (mutation.type === 'attributes' &&
            mutation.attributeName === attributeName) {
          // Check if the value actually changed.
          const newValue = (element as any)[attributeName];
          if (newValue !== initialValue) {
            observer.disconnect();
            resolve(newValue);
            return;
          }
        }
      }
    });

    // Configure the observer to watch for attribute changes.
    observer.observe(element, {
      attributes: true,
      attributeFilter: [attributeName],
    });
  });
}

// TODO(crbug.com/452983498): This class is mirrored in composebox_test_utils.ts
// to allow composebox tests to build on Android without depending on the
// Desktop New Tab Page bundle.
// LINT.IfChange
export class MockInputState implements InputState {
  allowedTools: number[] = [];
  disabledTools: number[] = [];
  activeTool: number = 0;
  toolConfigs: any[] = [
    {
      tool: ComposeboxToolMode.kDeepSearch,
      hintText: 'Research anything',
      menuLabel: '',
      chipLabel: '',
      disableActiveModelSelection: false,
      aimUrlParams: [],
      menuTooltip: '',
    },
    {
      tool: ComposeboxToolMode.kImageGen,
      hintText: 'Describe your image',
      menuLabel: '',
      chipLabel: '',
      disableActiveModelSelection: false,
      aimUrlParams: [],
      menuTooltip: '',
    },
    {
      tool: ComposeboxToolMode.kCanvas,
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
// LINT.ThenChange(//chrome/test/data/webui/cr_components/composebox/composebox_test_utils.ts)
