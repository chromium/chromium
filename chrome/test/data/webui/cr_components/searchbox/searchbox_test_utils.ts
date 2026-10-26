// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createAutocompleteMatch, createFormattedString, createSuggestTemplateInfo as createSuggestTemplateInfoBase} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxIconElement} from '//resources/cr_components/searchbox/searchbox_icon.js';
import type {AutocompleteMatch} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ColorType} from '//resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';
import type {FormattedString, FormattedStringFragment, SuggestTemplateInfo} from '//resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {ToolMode as ComposeboxToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertEquals} from '//webui-test/chai_assert.js';

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
      icon: 0,
    },
    {
      tool: ComposeboxToolMode.kImageGen,
      hintText: 'Describe your image',
      menuLabel: '',
      chipLabel: '',
      disableActiveModelSelection: false,
      aimUrlParams: [],
      menuTooltip: '',
      icon: 0,
    },
    {
      tool: ComposeboxToolMode.kCanvas,
      hintText: 'Create anything',
      menuLabel: '',
      chipLabel: '',
      disableActiveModelSelection: false,
      aimUrlParams: [],
      menuTooltip: '',
      icon: 0,
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

export function createClipboardEvent(name: string): ClipboardEvent {
  return new ClipboardEvent(
      name, {cancelable: true, clipboardData: new DataTransfer()});
}

/**
 * Like `Partial<SuggestTemplateInfo>`, but the primary and secondary text can
 * also be plain strings.
 */
export interface SuggestTemplateInfoModifiers extends
    Omit<Partial<SuggestTemplateInfo>, 'primaryText'|'secondaryText'> {
  primaryText?: string|FormattedString;
  secondaryText?: string|FormattedString|null;
}

/**
 * Like createSuggestTemplateInfo() in searchbox_browser_proxy.ts, but plain
 * string text becomes an unstyled FormattedString.
 */
export function createSuggestTemplateInfo(
    modifiers: SuggestTemplateInfoModifiers = {}): SuggestTemplateInfo {
  const {primaryText = '', secondaryText = null, ...otherModifiers} = modifiers;
  return createSuggestTemplateInfoBase({
    ...otherModifiers,
    primaryText: toFormattedString(primaryText),
    secondaryText: secondaryText === null ? null :
                                            toFormattedString(secondaryText),
  });
}

function toFormattedString(text: string|FormattedString): FormattedString {
  return typeof text === 'string' ? createFormattedString(text) : text;
}

export function createFormattedStringFragment(
    modifiers: Partial<FormattedStringFragment> = {}): FormattedStringFragment {
  return {
    startIndex: 0,
    color: ColorType.kUnknown,
    isBolded: false,
    ...modifiers,
  };
}

export function createUrlMatch(modifiers: Partial<AutocompleteMatch> = {}):
    AutocompleteMatch {
  return createAutocompleteMatch({
    destinationUrl: 'https://helloworld.com/',
    fillIntoEdit: 'https://helloworld.com',
    type: 'url-what-you-typed',
    suggestTemplate: createSuggestTemplateInfo({
      primaryText: createFormattedString(
          'helloworld.com', [createFormattedStringFragment(
                                {color: ColorType.kSuggestActionHigh})]),
    }),
    ...modifiers,
  });
}

export function assertIconMaskImageUrl(
    iconElement: SearchboxIconElement, url: string) {
  assertStyle(
      iconElement.$.icon, '-webkit-mask-image',
      `url("${new URL(url, document.baseURI).href}")`);
  assertStyle(iconElement.$.icon, 'background-image', 'none');
}

export function createKeyboardEvent(
    key: string, modifiers?: Partial<KeyboardEventInit>): KeyboardEvent {
  return new KeyboardEvent('keydown', {
    bubbles: true,
    cancelable: true,
    composed: true,
    key,
    ...modifiers,
  });
}
