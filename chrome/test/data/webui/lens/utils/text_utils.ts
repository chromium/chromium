// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexColorToSkColor} from '//resources/js/color_utils.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {LensPageRemote} from 'chrome-untrusted://lens-overlay/lens.mojom-webui.js';
import type {Line, Paragraph, Text, TranslatedLine, TranslatedParagraph, Word} from 'chrome-untrusted://lens-overlay/text.mojom-webui.js';
import {Alignment, WritingDirection} from 'chrome-untrusted://lens-overlay/text.mojom-webui.js';
import {flushTasks} from 'chrome-untrusted://webui-test/polymer_test_util.js';

import {normalizeBoxInElement} from './selection_utils.js';

/**
 * Adds empty text to the `callbackRouterRemote` provided.
 */
export async function addEmptyTextToPage(callbackRouterRemote: LensPageRemote) {
  const text = createText([]);
  callbackRouterRemote.textReceived(text);
  await flushTasks();
}

/**
 * Adds generic text to the `callbackRouterRemote` provided.
 */
export async function addGenericWordsToPage(
    callbackRouterRemote: LensPageRemote, element: Element) {
  const text = createText([
    createParagraph([
      createLine([
        createWord(
            'hello',
            normalizeBoxInElement(
                {x: 20, y: 20, width: 30, height: 10}, element)),
        createWord(
            'there',
            normalizeBoxInElement(
                {x: 50, y: 20, width: 50, height: 10}, element)),
      ]),
    ]),
    createParagraph([
      createLine([
        createWord(
            'test',
            normalizeBoxInElement(
                {x: 80, y: 20, width: 30, height: 10}, element)),
      ]),
    ]),
  ]);
  callbackRouterRemote.textReceived(text);
  await flushTasks();
}

export function getHighlightedNodesForTesting(textLayerElement: Element):
    NodeListOf<Element> {
  return textLayerElement.shadowRoot!.querySelectorAll('.highlighted-line');
}

export function getWordNodesForTesting(textLayerElement: Element):
    NodeListOf<Element> {
  return textLayerElement.shadowRoot!.querySelectorAll('.word');
}

export function getTranslatedWordNodesForTesting(textLayerElement: Element):
    NodeListOf<Element> {
  return textLayerElement.shadowRoot!.querySelectorAll('.translated-word');
}

export function createText(paragraphs: Paragraph[]): Text {
  return {
    textLayout: {
      paragraphs: paragraphs,
    },
    contentLanguage: null,
  };
}

export function createParagraph(
    lines: Line[], translatedParagraph: TranslatedParagraph|null = null,
    writingDirection = WritingDirection.kLeftToRight): Paragraph {
  return {
    lines,
    translation: translatedParagraph,
    writingDirection,
    geometry: null,
    contentLanguage: null,
  };
}

export function createTranslatedParagraph(
    lines: TranslatedLine[], contentLanguage = 'en',
    alignment = Alignment.kDefaultLeftAlgined,
    writingDirection = WritingDirection.kLeftToRight): TranslatedParagraph {
  return {
    lines,
    resizedBitmapSize: {
      width: 1000,
      height: 1000,
    },
    alignment,
    contentLanguage,
    writingDirection,
  };
}

export function createTranslatedLine(
    words: Word[], translation: string, textHexColor: string,
    backgroundHexColor: string, lineBoundingBox: RectF): TranslatedLine {
  return {
    words,
    translation,
    textColor: hexColorToSkColor(textHexColor),
    backgroundPrimaryColor: hexColorToSkColor(backgroundHexColor),
    backgroundImageData: null,
    geometry: {
      boundingBox: {
        box: lineBoundingBox,
        rotation: 0,
        coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
      },
      segmentationPolygon: [],
    },
  };
}

export function createLine(words: Word[]): Line {
  return {words, geometry: null};
}

export function createWord(
    plainText: string, wordBoundingBox?: RectF, textSeparator: string = ' ',
    writingDirection = WritingDirection.kLeftToRight): Word {
  const geometry = wordBoundingBox ? {
    boundingBox: {
      box: wordBoundingBox,
      rotation: 0,
      coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
    },
    segmentationPolygon: [],
  } :
                                     null;
  return {
    plainText,
    textSeparator,
    writingDirection,
    geometry,
    formulaMetadata: null,
  };
}

export function dispatchTranslateStateEvent(
    target: Element, translateModeEnabled: boolean, targetLanguage: string) {
  target.dispatchEvent(new CustomEvent('translate-mode-state-changed', {
    detail: {translateModeEnabled, targetLanguage},
    bubbles: true,
    composed: true,
  }));
}
