// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ReadAloudModelBrowserProxy, ReadAloudNode, Segment} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestReadAloudModelBrowserProxy extends TestBrowserProxy implements
    ReadAloudModelBrowserProxy {
  private currentTextSegments_: Segment[] = [];
  private highlightsForCurrentSegmentIndex_: Segment[] = [];
  private currentTextContent_: string = '';
  private isInitialized_: boolean = false;

  constructor() {
    super([
      'getHighlightForCurrentSegmentIndex',
      'getCurrentTextSegments',
      'getCurrentTextContent',
      'getAccessibleText',
      'resetSpeechToBeginning',
      'moveSpeechForward',
      'moveSpeechBackwards',
      'isInitialized',
      'init',
      'resetModel',
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
    return this.currentTextContent_;
  }

  getAccessibleText(text: string, maxSpeechLength: number): string {
    this.methodCalled('getAccessibleText', text, maxSpeechLength);
    return text.substring(0, maxSpeechLength);
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
    return this.isInitialized_;
  }

  init(context: ReadAloudNode): void {
    this.methodCalled('init', context);
    this.isInitialized_ = true;
  }

  resetModel(): void {
    this.methodCalled('resetModel');
  }

  setCurrentTextContent(content: string) {
    this.currentTextContent_ = content;
  }

  setInitialized(isInitialized: boolean) {
    this.isInitialized_ = isInitialized;
  }
}
