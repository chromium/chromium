// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ReadAloudModelBrowserProxy, ReadAloudNode, Segment} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestReadAloudModelBrowserProxy extends TestBrowserProxy implements
    ReadAloudModelBrowserProxy {
  private currentTextSegments_: Segment[] = [];
  private highlightsForCurrentSegmentIndex_: Segment[] = [];

  constructor() {
    super([
      'getHighlightForCurrentSegmentIndex',
      'getCurrentTextSegments',
      'getCurrentTextContent',
      'getAccessibleBoundary',
      'resetSpeechToBeginning',
      'moveSpeechForward',
      'moveSpeechBackwards',
      'isInitialized',
      'init',
    ]);
  }

  setHighlightForCurrentSegmentIndex(highlights: Segment[]) {
    this.highlightsForCurrentSegmentIndex_ = highlights;
  }

  getHighlightForCurrentSegmentIndex(index: number, phrases: boolean):
      Array<{node: ReadAloudNode, start: number, length: number}> {
    this.methodCalled('getHighlightForCurrentSegmentIndex', index, phrases);
    return this.highlightsForCurrentSegmentIndex_;
  }

  setCurrentTextSegments(segments: Segment[]) {
    this.currentTextSegments_ = segments;
  }

  getCurrentTextSegments():
      Array<{node: ReadAloudNode, start: number, length: number}> {
    this.methodCalled('getCurrentTextSegments');
    return this.currentTextSegments_;
  }

  getCurrentTextContent(): string {
    this.methodCalled('getCurrentTextContent');
    return '';
  }

  getAccessibleBoundary(text: string, maxSpeechLength: number): number {
    this.methodCalled('getAccessibleBoundary', text, maxSpeechLength);
    return 0;
  }

  resetSpeechToBeginning(): void {
    this.methodCalled('resetSpeechToBeginning');
  }

  moveSpeechForward(): void {
    this.methodCalled('moveSpeechForward');
  }

  moveSpeechBackwards(): void {
    this.methodCalled('moveSpeechBackwards');
  }

  isInitialized(): boolean {
    this.methodCalled('isInitialized');
    return false;
  }

  init(context: ReadAloudNode|string): void {
    this.methodCalled('init', context);
  }
}
