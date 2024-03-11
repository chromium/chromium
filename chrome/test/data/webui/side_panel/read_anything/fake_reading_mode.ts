// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class FakeReadingMode {
  // The root AXNodeID of the tree to be displayed.
  rootId: number = 1;

  startNodeId: number = 0;
  startOffset: number = 0;
  endNodeId: number = 0;
  endOffset: number = 0;

  // Items in the ReadAnythingTheme struct, see read_anything.mojom for info.
  fontName: string = 'MyFont';
  fontSize: number = 0;
  linksEnabled: boolean = true;
  foregroundColor: number = 0;
  backgroundColor: number = 0;
  lineSpacing: number = 0;
  letterSpacing: number = 0;

  // The current color theme value.
  colorTheme: number = 0;

  // Current audio settings values.
  speechRate: number = 1;
  highlightGranularity: number = 1;

  // Enum values for various visual theme changes.
  standardLineSpacing: number = 0;
  looseLineSpacing: number = 0;
  veryLooseLineSpacing: number = 0;
  standardLetterSpacing: number = 0;
  wideLetterSpacing: number = 0;
  veryWideLetterSpacing: number = 0;
  defaultTheme: number = 0;
  lightTheme: number = 0;
  darkTheme: number = 0;
  yellowTheme: number = 0;
  blueTheme: number = 0;
  highlightOn: number = 0;

  // Whether the WebUI toolbar feature flag is enabled.
  isWebUIToolbarVisible: boolean = true;

  // Whether the Read Aloud feature flag is enabled.
  isReadAloudEnabled: boolean = false;

  // Indicates if select-to-distill works on the web page. Used to
  // determine which empty state to display.
  isSelectable: boolean = false;

  // Fonts supported by the browser's preferred language.
  supportedFonts: string[] = ['roboto'];

  // The language code that should be used for speech synthesis voices.
  speechSynthesisLanguageCode: string = '';

  private maxNodeId: number = 5;

  // Returns the stored user voice preference for the given language.
  getStoredVoice(_lang: string): string {
    return 'abc';
  }

  // Returns a list of AXNodeIDs corresponding to the unignored children of
  // the AXNode for the provided AXNodeID. If there is a selection contained
  // in this node, only returns children which are partially or entirely
  // contained within the selection.
  getChildren(nodeId: number): number[] {
    return (nodeId > this.maxNodeId) ? [] : [nodeId + 1];
  }

  // Returns the HTML tag of the AXNode for the provided AXNodeID. For testing,
  // odd numbered nodes are divs and even numbered nodes are text.
  getHtmlTag(nodeId: number): string {
    return (nodeId % 2 === 0) ? '' : 'div';
  }

  // Returns the language of the AXNode for the provided AXNodeID.
  getLanguage(_nodeId: number): string {
    return 'en-us';
  }

  // Returns the text content of the AXNode for the provided AXNodeID. If a
  // selection begins or ends in this node, truncates the text to only return
  // the selected text.
  getTextContent(nodeId: number): string {
    return 'super awesome text content' + nodeId;
  }

  // Returns the text direction of the AXNode for the provided AXNodeID.
  getTextDirection(_nodeId: number): string {
    return 'ltr';
  }

  // Returns the url of the AXNode for the provided AXNodeID.
  getUrl(_nodeId: number): string {
    return 'foo';
  }

  // Returns true if the text node / element should be bolded.
  shouldBold(_nodeId: number): boolean {
    return false;
  }

  // Returns true if the element has overline text styling.
  isOverline(_nodeId: number): boolean {
    return false;
  }

  // Returns true if the element is a leaf node.
  isLeafNode(nodeId: number): boolean {
    return nodeId === this.maxNodeId;
  }

  // Returns true if the webpage corresponds to a Google Doc.
  isGoogleDocs(): boolean {
    return false;
  }

  // Connects to the browser process. Called by ts when the read anything
  // element is added to the document.
  onConnected() {}

  // Called when a user tries to copy text from reading mode with keyboard
  // shortcuts.
  onCopy() {}

  // Called when the Read Anything panel is scrolled.
  onScroll(_onSelection: boolean) {}

  // Called when a user clicks a link. NodeID is an AXNodeID which identifies
  // the link's corresponding AXNode in the main pane.
  onLinkClicked(_nodeId: number) {}

  // Called when the line spacing is changed via the webui toolbar.
  onStandardLineSpacing() {}
  onLooseLineSpacing() {}
  onVeryLooseLineSpacing() {}

  // Called when a user makes a font size change via the webui toolbar.
  onFontSizeChanged(_increase: boolean) {
    this.fontSize = this.fontSize + (_increase ? 1 : -1);
  }
  onFontSizeReset() {
    this.fontSize = 0;
  }

  // Called when a user toggles links via the webui toolbar.
  onLinksEnabledToggled() {
    this.linksEnabled = !this.linksEnabled;
  }

  // Called when the letter spacing is changed via the webui toolbar.
  onStandardLetterSpacing() {}
  onWideLetterSpacing() {}
  onVeryWideLetterSpacing() {}

  // Called when the color theme is changed via the webui toolbar.
  onDefaultTheme() {}
  onLightTheme() {}
  onDarkTheme() {}
  onYellowTheme() {}
  onBlueTheme() {}

  // Called when the font is changed via the webui toolbar.
  onFontChange(font: string) {
    this.fontName = font;
  }

  // Called when the speech rate is changed via the webui toolbar.
  onSpeechRateChange(rate: number) {
    this.speechRate = rate;
  }

  // Called when the voice used for speech is changed via the webui toolbar.
  onVoiceChange(_voice: string, _lang: string) {}

  // Called when the highlight granularity is changed via the webui toolbar.
  turnedHighlightOn() {
    this.highlightGranularity = 1;
  }
  turnedHighlightOff() {
    this.highlightGranularity = 0;
  }

  // Returns the actual spacing value to use based on the given lineSpacing
  // category.
  getLineSpacingValue(lineSpacing: number): number {
    return lineSpacing;
  }

  // Returns the actual spacing value to use based on the given letterSpacing
  // category.
  getLetterSpacingValue(letterSpacing: number): number {
    return letterSpacing;
  }

  // Called when a user makes a selection change. AnchorNodeID and
  // focusAXNodeID are AXNodeIDs which identify the anchor and focus AXNodes
  // in the main pane. The selection can either be forward or backwards.
  onSelectionChange(
      _anchorNodeId: number, _anchorOffset: number, _focusNodeId: number,
      _focusOffset: number) {}

  // Called when a user collapses the selection. This is usually accomplished
  // by clicking.
  onCollapseSelection() {}

  // Set the content. Used by tests only.
  // SnapshotLite is a data structure which resembles an AXTreeUpdate. E.g.:
  //   const axTree = {
  //     rootId: 1,
  //     nodes: [
  //       {
  //         id: 1,
  //         role: 'rootWebArea',
  //         childIds: [2],
  //       },
  //       {
  //         id: 2,
  //         role: 'staticText',
  //         name: 'Some text.',
  //       },
  //     ],
  //   };
  setContentForTesting(_snapshotLite: Object, _contentNodeIds: number[]) {}

  // Set the theme. Used by tests only.
  setThemeForTesting(
      _fontName: string, _fontSize: number, _linksEnabled: boolean,
      _foregroundColor: number, _backgroundColor: number, _lineSpacing: number,
      _letterSpacing: number) {}

  // Sets the default language. Used by tests only.
  setLanguageForTesting(_code: string) {}

  // Called when the side panel has finished loading and it's safe to call
  // SidePanelWebUIView::ShowUI
  shouldShowUi(): boolean {
    return true;
  }

  ////////////////////////////////////////////////////////////////
  // Implemented in read_anything/app.ts and called by native c++.
  ////////////////////////////////////////////////////////////////

  // Display a loading screen to tell the user we are distilling the page.
  showLoading() {}

  // Display the empty state page to tell the user we can't distill the page.
  showEmpty() {}

  // Ping that an AXTree has been distilled for the active tab's render frame
  // and is available to consume.
  updateContent() {}

  // Ping that the selection has been updated.
  updateSelection() {}

  // Ping that the theme choices of the user have been changed using the
  // toolbar and are ready to consume.
  updateTheme() {}

  // Ping that the theme choices of the user have been retrieved from
  // preferences and can be used to set up the page.
  restoreSettingsFromPrefs() {}

  // Inits the AXPosition instance in ReadAnythingAppController with the
  // starting node. Currently needed to orient the AXPosition to the correct
  // position, but we should be able to remove this in the future.
  initAxPositionWithNode(_startingNodeId: number): void {}

  // Gets the starting text index for the current Read Aloud text segment
  // for the given node. nodeId should be a node returned by getCurrentText.
  // Returns -1 if the node is invalid.
  getCurrentTextStartIndex(_nodeId: number): number {
    return 0;
  }

  // Gets the ending text index for the current Read Aloud text segment
  // for the given node. nodeId should be a node returned by getCurrentText or
  // getPreviousText. Returns -1 if the node is invalid.
  getCurrentTextEndIndex(_nodeId: number): number {
    return 5;
  }

  // Gets the nodes of the  next text that should be spoken and highlighted.
  // Use getCurrentTextStartIndex and getCurrentTextEndIndex to get the bounds
  // for text associated with these nodes.
  getCurrentText(): number[] {
    return [2];
  }

  // Increments the processed_granularity_index_ in ReadAnythingAppModel,
  // effectively updating ReadAloud's state of the current granularity to
  // refer to the next granularity.
  movePositionToNextGranularity(): void {}

  // Decrements the processed_granularity_index_ in ReadAnythingAppModel,
  // effectively updating ReadAloud's state of the current granularity to
  // refer to the previous granularity.
  movePositionToPreviousGranularity(): void {}

  // Returns the index of the next sentence of the given text, such that the
  // next sentence is equivalent to text.substr(0, <returned_index>).
  // If the sentence exceeds the maximum text length, the sentence will be
  // cropped to the nearest word boundary that doesn't exceed the maximum
  // text length.
  getNextSentence(_value: string, _maxTextLength: number): number {
    return 0;
  }

  // Signal that the supported fonts should be updated i.e. that the brower's
  // preferred language has changed.
  updateFonts() {}

  getDisplayNameForLocale(_locale: string, _displayLocale: string): string {
    return '';
  }
}
