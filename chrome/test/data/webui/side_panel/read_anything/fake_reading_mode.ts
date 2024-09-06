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
  highlightGranularity: number = 0;

  // Enum values for various visual theme changes.
  standardLineSpacing: number = 0;
  looseLineSpacing: number = 1;
  veryLooseLineSpacing: number = 2;
  standardLetterSpacing: number = 3;
  wideLetterSpacing: number = 4;
  veryWideLetterSpacing: number = 5;
  defaultTheme: number = 6;
  lightTheme: number = 7;
  darkTheme: number = 8;
  yellowTheme: number = 9;
  blueTheme: number = 10;

  // Enum values for highlight granularity.
  autoHighlighting: number = 0;
  wordHighlighting: number = 1;
  phraseHighlighting: number = 2;
  sentenceHighlighting: number = 3;
  noHighlighting: number = 4;

  // Whether the WebUI toolbar feature flag is enabled.
  isWebUIToolbarVisible: boolean = true;

  // Whether the Read Aloud feature flag is enabled.
  isReadAloudEnabled: boolean = false;

  // Returns true if the webpage corresponds to a Google Doc.
  isGoogleDocs: boolean = false;

  // Fonts supported by the browser's preferred language.
  supportedFonts: string[] = ['roboto'];

  // The base language code that should be used for speech synthesis voices.
  baseLanguageForSpeech: string = '';

  // The fallback language, corresponding to the browser language, that
  // should only be used when baseLanguageForSpeech is unavailable.
  defaultLanguageForSpeech: string = '';

  // TTS voice language preferences saved in database
  savedLanguagePref: Set<string> = new Set<string>();

  private maxNodeId: number = 5;

  // Returns whether the reading highlight is currently on.
  isHighlightOn(): boolean {
    return this.highlightGranularity !== this.noHighlighting;
  }

  // Returns the stored user voice preference for the current language.
  getStoredVoice(): string {
    return 'abc';
  }

  // Returns a list of AXNodeIDs corresponding to the unignored children of
  // the AXNode for the provided AXNodeID. If there is a selection contained
  // in this node, only returns children which are partially or entirely
  // contained within the selection.
  getChildren(nodeId: number): number[] {
    return (nodeId > this.maxNodeId) ? [] : [nodeId + 1];
  }

  // Returns content of "data-font-css" html attribute. This is needed for
  // rendering content from annotated canvas in Google Docs.
  getDataFontCss(_nodeId: number): string {
    return '400 14.6667px "Courier New"';
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

  // Connects to the browser process. Called by ts when the read anything
  // element is added to the document.
  onConnected() {}

  // Called when a user tries to copy text from reading mode with keyboard
  // shortcuts.
  onCopy() {}

  // Called when speech is paused or played.
  onSpeechPlayingStateChanged(_isSpeechActive: boolean) {}

  // Called when the Read Anything panel is scrolled.
  onScroll(_onSelection: boolean) {}

  // Called when a user clicks a link. NodeID is an AXNodeID which identifies
  // the link's corresponding AXNode in the main pane.
  onLinkClicked(_nodeId: number) {}

  // Called when the line spacing is changed via the webui toolbar.
  onLineSpacingChange(value: number) {
    this.lineSpacing = value;
  }

  // Called when a user makes a font size change via the webui toolbar.
  onFontSizeChanged(_increase: boolean) {
    this.fontSize = this.fontSize + (_increase ? 1 : -1);
  }
  onFontSizeReset() {
    this.fontSize = 0;
  }

  onHighlightGranularityChanged(value: number) {
    this.highlightGranularity = value;
  }

  // Called when a user toggles a switch in the language menu
  onLanguagePrefChange(lang: string, enabled: boolean) {
    if(enabled) {
      this.savedLanguagePref.add(lang);
    } else {
      this.savedLanguagePref.delete(lang);
    }
  }


  // Called when a user toggles links via the webui toolbar.
  onLinksEnabledToggled() {
    this.linksEnabled = !this.linksEnabled;
  }

  // Called when the letter spacing is changed via the webui toolbar.
  onLetterSpacingChange(value: number) {
    this.letterSpacing = value;
  }

  // Called when the color theme is changed via the webui toolbar.
  onThemeChange(value: number) {
    this.colorTheme = value;
  }

  // Returns the css name of the given font, or the default if it's not valid.
  getValidatedFontName(font: string) {
    return font;
  }

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

  // Called when a tracked count-based metric is incremented.
  incrementMetricCount(_metric: string) {}

  // Called when the highlight granularity is changed via the webui toolbar.
  turnedHighlightOn() {
    this.highlightGranularity = this.autoHighlighting;
  }

  turnedHighlightOff() {
    this.highlightGranularity = this.noHighlighting;
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

  // Returns the actual enabled languages in preference
  getLanguagesEnabledInPref(): string[] {
    return [...this.savedLanguagePref.values()];
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

  sendGetVoicePackInfoRequest(_: string) {}

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

  // Sets the language. Used by tests only.
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

  // Called with the response of sendGetVoicePackInfoRequest()
  updateVoicePackStatus(_lang: string, _status: string) {}

  // Called with the response of sendInstallVoicePackRequest()
  updateVoicePackStatusFromInstallResponse() {}

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

  // Signal that the page language has changed.
  languageChanged(): void {}

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

  // Begins processing the speech segments on the current page to be used by
  // Read Aloud.
  preprocessTextForSpeech() {}
}
