// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import {CenterRotatedBox_CoordinateType} from 'chrome-untrusted://lens/geometry.mojom-webui.js';
import type {Line, Paragraph, Text, Word} from 'chrome-untrusted://lens/text.mojom-webui.js';
import {WritingDirection} from 'chrome-untrusted://lens/text.mojom-webui.js';

export function createText(paragraphs: Paragraph[]): Text {
  return {textLayout: {paragraphs: paragraphs}, contentLanguage: null};
}

export function createParagraph(
    lines: Line[],
    writingDirection = WritingDirection.kLeftToRight): Paragraph {
  return {lines, writingDirection, geometry: null, contentLanguage: null};
}

export function createLine(words: Word[]): Line {
  return {words, geometry: null};
}

export function createWord(
    plainText: string, wordBoundingBox: RectF, textSeparator: string = ' ',
    writingDirection = WritingDirection.kLeftToRight): Word {
  return {
    plainText,
    textSeparator,
    writingDirection,
    geometry: {
      boundingBox: {
        box: wordBoundingBox,
        rotation: 0,
        coordinateType: CenterRotatedBox_CoordinateType.kNormalized,
      },
    },
    formulaMetadata: null,
  };
}
