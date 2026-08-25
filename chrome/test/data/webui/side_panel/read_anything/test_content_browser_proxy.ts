// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AxSegment, ContentBrowserProxy, SkiaImageBitmap} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {FakeChromeEvent} from 'chrome-untrusted://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestContentBrowserProxy extends TestBrowserProxy implements
    ContentBrowserProxy {
  onAnchorsReadyForReadability = new FakeChromeEvent();
  onNodeWillBeDeleted = new FakeChromeEvent();
  onImageDownloaded = new FakeChromeEvent();
  onMainFrameSameDocumentNavigation = new FakeChromeEvent();
  onRenderedTextMappingReady = new FakeChromeEvent();
  showEmpty = new FakeChromeEvent();
  showLoading = new FakeChromeEvent();
  updateImages = new FakeChromeEvent();
  updateLinks = new FakeChromeEvent();
  updateSelection = new FakeChromeEvent();

  startNodeId: number = -1;
  startOffset: number = -1;
  endNodeId: number = -1;
  endOffset: number = -1;
  hasValidSelectionVal: boolean = true;
  readabilityEnabled: boolean = true;
  isReadabilitySelectTextEnabledFlag: boolean = false;
  textContentMap: {[key: number]: string} = {2: 'some text content'};
  prefixText: string = '';
  rootId: number = 1;
  htmlTitle: string = '';
  htmlContent: string = '';
  documentUrl: string = '';
  googleDocs: boolean = false;
  docsLoadMoreButtonVisible: boolean = false;
  unexpectedUpdateContentStopSource: number = 0;
  axMapping: AxSegment[] = [];
  htmlTagMap: {[key: number]: string} = {1: 'div'};
  leafNodeSet: Set<number> = new Set();
  urlMap: {[key: number]: string} = {};
  htmlIdMap: {[key: number]: string} = {};
  textDirection: string = '';
  altText: string = '';
  language: string = '';
  childrenMap: {[key: number]: number[]} = {1: [2]};
  isOverlineVal: boolean = false;
  shouldBoldVal: boolean = false;
  imageBitmap: SkiaImageBitmap|null = null;
  axTreeAnchorsVal: Record<string, AxTreeAnchorMetadata[]> = {};
  activeDistillationMethod: number = 0;
  distillationTypeReadability: number = 1;
  distillationTypeScreen2x: number = 0;
  requiresDistillationVal: boolean = false;

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
      'onScrolledToBottom',
      'getTextContent',
      'getPrefixText',
      'getRootId',
      'getHtmlTitle',
      'getHtmlContent',
      'getDocumentUrl',
      'isGoogleDocs',
      'isDocsLoadMoreButtonVisible',
      'getUnexpectedUpdateContentStopSource',
      'getAxMapping',
      'getHtmlTag',
      'isLeafNode',
      'getUrl',
      'getHtmlId',
      'getTextDirection',
      'getAltText',
      'getLanguage',
      'getChildren',
      'isOverline',
      'shouldBold',
      'getImageBitmap',
      'getAxTreeAnchors',
      'onNoTextContent',
      'onCopy',
      'requiresDistillation',
      'onDistilled',
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

  getRootId(): number {
    this.methodCalled('getRootId');
    return this.rootId;
  }

  getHtmlTitle(): string {
    this.methodCalled('getHtmlTitle');
    return this.htmlTitle;
  }

  getHtmlContent(): string {
    this.methodCalled('getHtmlContent');
    return this.htmlContent;
  }

  getDocumentUrl(): string {
    this.methodCalled('getDocumentUrl');
    return this.documentUrl;
  }

  isGoogleDocs(): boolean {
    this.methodCalled('isGoogleDocs');
    return this.googleDocs;
  }

  getUnexpectedUpdateContentStopSource(): number {
    this.methodCalled('getUnexpectedUpdateContentStopSource');
    return this.unexpectedUpdateContentStopSource;
  }

  getAxMapping(index: number): AxSegment[] {
    this.methodCalled('getAxMapping', index);
    return this.axMapping;
  }

  getHtmlTag(nodeId: number): string {
    this.methodCalled('getHtmlTag', nodeId);
    return this.htmlTagMap[nodeId] || '';
  }

  isLeafNode(nodeId: number): boolean {
    this.methodCalled('isLeafNode', nodeId);
    return this.leafNodeSet.has(nodeId);
  }

  getUrl(nodeId: number): string {
    this.methodCalled('getUrl', nodeId);
    return this.urlMap[nodeId] || '';
  }

  getHtmlId(nodeId: number): string {
    this.methodCalled('getHtmlId', nodeId);
    return this.htmlIdMap[nodeId] || '';
  }

  getTextDirection(nodeId: number): string {
    this.methodCalled('getTextDirection', nodeId);
    return this.textDirection;
  }

  getAltText(nodeId: number): string {
    this.methodCalled('getAltText', nodeId);
    return this.altText;
  }

  getLanguage(nodeId: number): string {
    this.methodCalled('getLanguage', nodeId);
    return this.language;
  }

  getChildren(nodeId: number): number[] {
    this.methodCalled('getChildren', nodeId);
    return this.childrenMap[nodeId] || [];
  }

  isOverline(nodeId: number): boolean {
    this.methodCalled('isOverline', nodeId);
    return this.isOverlineVal;
  }

  shouldBold(nodeId: number): boolean {
    this.methodCalled('shouldBold', nodeId);
    return this.shouldBoldVal;
  }

  getImageBitmap(nodeId: number): SkiaImageBitmap|null {
    this.methodCalled('getImageBitmap', nodeId);
    return this.imageBitmap;
  }

  getAxTreeAnchors(): Record<string, AxTreeAnchorMetadata[]> {
    this.methodCalled('getAxTreeAnchors');
    return this.axTreeAnchorsVal;
  }

  onNoTextContent(): void {
    this.methodCalled('onNoTextContent');
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

  isDocsLoadMoreButtonVisible(): boolean {
    this.methodCalled('isDocsLoadMoreButtonVisible');
    return this.docsLoadMoreButtonVisible;
  }

  onScrolledToBottom(): void {
    this.methodCalled('onScrolledToBottom');
  }

  onCopy(): void {
    this.methodCalled('onCopy');
  }

  requiresDistillation(): boolean {
    this.methodCalled('requiresDistillation');
    return this.requiresDistillationVal;
  }

  onDistilled(wordCount: number): void {
    this.methodCalled('onDistilled', wordCount);
  }
}
