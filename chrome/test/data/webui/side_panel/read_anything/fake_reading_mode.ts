// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class FakeReadingMode {
  // The root AXNodeID of the tree to be displayed.
  rootId: number = 0;

  startNodeId: number = 0;
  startOffset: number = 0;
  endNodeId: number = 0;
  endOffset: number = 0;

  // Items in the ReadAnythingTheme struct, see read_anything.mojom for info.
  fontName: string = 'MyFont';
  fontSize: number = 0;
  foregroundColor: number = 0;
  backgroundColor: number = 0;
  lineSpacing: number = 0;
  letterSpacing: number = 0;

  // The current color theme value.
  colorTheme: number = 0;

  // Current audio settings values.
  speechRate: number = 0;
  highlightGranularity: number = 0;

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

  // Returns the stored user voice preference for the given language.
  getStoredVoice(_lang: string): string {
    return 'abc';
  }

  // Returns a list of AXNodeIDs corresponding to the unignored children of
  // the AXNode for the provided AXNodeID. If there is a selection contained
  // in this node, only returns children which are partially or entirely
  // contained within the selection.
  getChildren(_nodeId: number): number[] {
    return [];
  }

  // Returns the HTML tag of the AXNode for the provided AXNodeID.
  getHtmlTag(_nodeId: number): string {
    return 'div';
  }

  // Returns the language of the AXNode for the provided AXNodeID.
  getLanguage(_nodeId: number): string {
    return 'en-us';
  }

  // Returns the text content of the AXNode for the provided AXNodeID. If a
  // selection begins or ends in this node, truncates the text to only return
  // the selected text.
  getTextContent(_nodeId: number): string {
    return 'foo';
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
  onFontSizeChanged(_increase: boolean) {}
  onFontSizeReset() {}

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
  onFontChange(_font: string) {}

  // Called when the speech rate is changed via the webui toolbar.
  onSpeechRateChange(_rate: number) {}

  // Called when the voice used for speech is changed via the webui toolbar.
  onVoiceChange(_voice: string, _lang: string) {}

  // Called when the highlight granularity is changed via the webui toolbar.
  turnedHighlightOn() {}
  turnedHighlightOff() {}

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
      _fontName: string, _fontSize: number, _foregroundColor: number,
      _backgroundColor: number, _lineSpacing: number, _letterSpacing: number) {}

  // Sets the default language. Used by tests only.
  setLanguageForTesting(_code: string) {}

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
}
