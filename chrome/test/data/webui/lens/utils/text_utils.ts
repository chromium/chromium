// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {hexColorToSkColor} from '//resources/js/color_utils.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens-overlay/geometry.mojom-webui.js';
import type {Line, Paragraph, Text, TranslatedLine, TranslatedParagraph, Word} from 'chrome-untrusted://lens-overlay/text.mojom-webui.js';
import {Alignment, WritingDirection} from 'chrome-untrusted://lens-overlay/text.mojom-webui.js';

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
