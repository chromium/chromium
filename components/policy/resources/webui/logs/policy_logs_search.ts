// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {TemplateResult} from 'chrome://resources/lit/v3_0/lit.rollup.js';

interface Interval {
  start: number;
  end: number;
}

/**
 * Finds all occurrences of the query words in the text, returns them as a
 * list of character intervals, and merges any overlapping or adjacent
 * intervals. This is used to highlight matching text segments as contiguous
 * spans while avoiding nested or duplicate highlight wrappers.
 */
function findMatchesAndMerge(text: string, words: string[]): Interval[] {
  const intervals: Interval[] = [];
  const lowerText = text.toLowerCase();
  for (const word of words) {
    const lowerWord = word.toLowerCase();
    let index = lowerText.indexOf(lowerWord);
    while (index !== -1) {
      intervals.push({start: index, end: index + word.length});
      index = lowerText.indexOf(lowerWord, index + 1);
    }
  }

  if (intervals.length === 0) {
    return [];
  }

  intervals.sort((a, b) => a.start - b.start);

  const merged: Interval[] = [intervals[0]!];
  for (let i = 1; i < intervals.length; i++) {
    const curr = intervals[i]!;
    const last = merged[merged.length - 1]!;
    if (curr.start <= last.end) {
      last.end = Math.max(last.end, curr.end);
    } else {
      merged.push(curr);
    }
  }
  return merged;
}

export function highlightText(text: string, words: string[]): TemplateResult|
    string {
  const intervals = findMatchesAndMerge(text, words);
  if (intervals.length === 0) {
    return text;
  }
  const result: Array<string|TemplateResult> = [];
  let lastIndex = 0;
  for (const interval of intervals) {
    const start = interval.start;
    const end = interval.end;
    if (start > lastIndex) {
      result.push(text.substring(lastIndex, start));
    }
    result.push(
        html`<span class="highlight">${text.substring(start, end)}</span>`);
    lastIndex = end;
  }
  if (lastIndex < text.length) {
    result.push(text.substring(lastIndex));
  }
  return html`${result}`;
}
