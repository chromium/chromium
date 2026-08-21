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
  readabilityEnabled: boolean = true;
  isReadabilitySelectTextEnabledFlag: boolean = false;
  textContentMap: {[key: number]: string} = {};
  prefixText: string = '';
  googleDocs: boolean = false;
  leafNodeSet: Set<number> = new Set();
  isOverlineVal: boolean = false;
  shouldBoldVal: boolean = false;
  activeDistillationMethod: number = 0;
  distillationTypeReadability: number = 1;
  distillationTypeScreen2x: number = 0;

  constructor() {
    super([
      'getStartNodeId',
      'getStartOffset',
      'getEndNodeId',
      'getEndOffset',
      'hasValidSelection',
      'isReadabilityEnabled',
      'isReadabilitySelectTextEnabled',
      'getActiveDistillationMethod',
      'getDistillationTypeReadability',
      'getDistillationTypeScreen2x',
      'onConnected',
      'onCollapseSelection',
      'attemptLogEarlySelection',
      'onSelectionChange',
      'onScroll',
      'getTextContent',
      'getPrefixText',
      'isGoogleDocs',
      'isLeafNode',
      'isOverline',
      'shouldBold',
      'onNoTextContent',
      'updateSelection',
      'onLinkClicked',
      'onRenderedTextBlocksAvailable',
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

  isReadabilityEnabled(): boolean {
    this.methodCalled('isReadabilityEnabled');
    return this.readabilityEnabled;
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

  isGoogleDocs(): boolean {
    this.methodCalled('isGoogleDocs');
    return this.googleDocs;
  }

  isLeafNode(nodeId: number): boolean {
    this.methodCalled('isLeafNode', nodeId);
    return this.leafNodeSet.has(nodeId);
  }

  isOverline(nodeId: number): boolean {
    this.methodCalled('isOverline', nodeId);
    return this.isOverlineVal;
  }

  shouldBold(nodeId: number): boolean {
    this.methodCalled('shouldBold', nodeId);
    return this.shouldBoldVal;
  }

  onNoTextContent(): void {
    this.methodCalled('onNoTextContent');
  }

  updateSelection(): void {
    this.methodCalled('updateSelection');
  }

  onLinkClicked(nodeId: number): void {
    this.methodCalled('onLinkClicked', nodeId);
  }

  onRenderedTextBlocksAvailable(blocks: string[]): void {
    this.methodCalled('onRenderedTextBlocksAvailable', blocks);
  }

  getActiveDistillationMethod(): number {
    this.methodCalled('getActiveDistillationMethod');
    return this.activeDistillationMethod;
  }

  getDistillationTypeReadability(): number {
    this.methodCalled('getDistillationTypeReadability');
    return this.distillationTypeReadability;
  }

  getDistillationTypeScreen2x(): number {
    this.methodCalled('getDistillationTypeScreen2x');
    return this.distillationTypeScreen2x;
  }
}
