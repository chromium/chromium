// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ContentBrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestContentBrowserProxy extends TestBrowserProxy implements
    ContentBrowserProxy {
  startNodeId: number = -1;
  startOffset: number = -1;
  endNodeId: number = -1;
  endOffset: number = -1;
  hasValidSelectionVal: boolean = true;
  isReadabilitySelectTextEnabledFlag: boolean = false;
  textContentMap: {[key: number]: string} = {};
  prefixText: string = '';

  constructor() {
    super([
      'getStartNodeId',
      'getStartOffset',
      'getEndNodeId',
      'getEndOffset',
      'hasValidSelection',
      'isReadabilitySelectTextEnabled',
      'onConnected',
      'onCollapseSelection',
      'attemptLogEarlySelection',
      'onSelectionChange',
      'onScroll',
      'getTextContent',
      'getPrefixText',
    ]);
  }

  getStartNodeId(): number {
    this.methodCalled('getStartNodeId');
    return this.startNodeId;
  }

  getStartOffset(): number {
    this.methodCalled('getStartOffset');
    return this.startOffset;
  }

  getEndNodeId(): number {
    this.methodCalled('getEndNodeId');
    return this.endNodeId;
  }

  getEndOffset(): number {
    this.methodCalled('getEndOffset');
    return this.endOffset;
  }

  hasValidSelection(): boolean {
    this.methodCalled('hasValidSelection');
    return this.hasValidSelectionVal;
  }

  isReadabilitySelectTextEnabled(): boolean {
    this.methodCalled('isReadabilitySelectTextEnabled');
    return this.isReadabilitySelectTextEnabledFlag;
  }

  onConnected(): void {
    this.methodCalled('onConnected');
  }

  onCollapseSelection(): void {
    this.methodCalled('onCollapseSelection');
  }

  attemptLogEarlySelection(fromSidePanel: boolean): void {
    this.methodCalled('attemptLogEarlySelection', fromSidePanel);
  }

  onSelectionChange(
      anchorNodeId: number, anchorOffset: number, focusNodeId: number,
      focusOffset: number): void {
    this.methodCalled(
        'onSelectionChange', anchorNodeId, anchorOffset, focusNodeId,
        focusOffset);
  }

  onScroll(scrollingOnSelection: boolean): void {
    this.methodCalled('onScroll', scrollingOnSelection);
  }

  getTextContent(nodeId: number): string {
    this.methodCalled('getTextContent', nodeId);
    return this.textContentMap[nodeId] || '';
  }

  getPrefixText(nodeId: number): string {
    this.methodCalled('getPrefixText', nodeId);
    return this.prefixText;
  }
}
