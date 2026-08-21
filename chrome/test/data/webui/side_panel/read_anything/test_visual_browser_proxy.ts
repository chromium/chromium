// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VisualBrowserProxy} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {TestBrowserProxy} from 'chrome-untrusted://webui-test/test_browser_proxy.js';

export class TestVisualBrowserProxy extends TestBrowserProxy implements
    VisualBrowserProxy {
  inSidePanelPresentationState: number = 1;
  inImmersiveOverlayPresentationState: number = 2;
  fontName: string = 'Poppins';
  supportedFonts: string[] = [];
  standardLineSpacing: number = 0;
  looseLineSpacing: number = 1;
  veryLooseLineSpacing: number = 2;
  standardLetterSpacing: number = 0;
  wideLetterSpacing: number = 1;
  veryWideLetterSpacing: number = 2;
  defaultTheme: number = 0;
  lightTheme: number = 1;
  darkTheme: number = 2;
  yellowTheme: number = 3;
  blueTheme: number = 4;
  highContrastTheme: number = 5;
  lowContrastLightTheme: number = 6;
  lowContrastDarkTheme: number = 7;
  fontSize: number = 10;
  lineSpacing: number = 0;
  activeDistillationMethod: number = 0;
  distillationTypeReadability: number = 0;
  lineFocusEnabled: boolean = false;
  lineFocusOff: number = 50;
  lineFocusSmallStaticWindow: number = 51;
  lineFocusMediumStaticWindow: number = 52;
  lineFocusLargeStaticWindow: number = 53;
  lineFocusSmallCursorWindow: number = 54;
  lineFocusMediumCursorWindow: number = 55;
  lineFocusLargeCursorWindow: number = 56;
  lineFocusStaticLine: number = 57;
  lineFocusCursorLine: number = 58;
  immersiveEnabled: boolean = true;
  activePresentationState: number = 1;
  pdf: boolean = false;
  keyPointsSection: boolean = false;
  keyPointsRegex: string = 'key points|summary|the bottom line|why it matters';

  constructor() {
    super([
      'getInSidePanelPresentationState',
      'getInImmersiveOverlayPresentationState',
      'getFontName',
      'getSupportedFonts',
      'getStandardLineSpacing',
      'getLooseLineSpacing',
      'getVeryLooseLineSpacing',
      'getLineSpacing',
      'getLineSpacingValue',
      'getFontSize',
      'getStandardLetterSpacing',
      'getWideLetterSpacing',
      'getVeryWideLetterSpacing',
      'getDefaultTheme',
      'getLightTheme',
      'getDarkTheme',
      'getYellowTheme',
      'getBlueTheme',
      'getHighContrastTheme',
      'getLowContrastLightTheme',
      'getLowContrastDarkTheme',
      'getActiveDistillationMethod',
      'getDistillationTypeReadability',
      'requestImageData',
      'isLineFocusEnabled',
      'getLineFocusOff',
      'getLineFocusSmallStaticWindow',
      'getLineFocusMediumStaticWindow',
      'getLineFocusLargeStaticWindow',
      'getLineFocusSmallCursorWindow',
      'getLineFocusMediumCursorWindow',
      'getLineFocusLargeCursorWindow',
      'getLineFocusStaticLine',
      'getLineFocusCursorLine',
      'onFontChange',
      'onLineSpacingChange',
      'onLetterSpacingChange',
      'onThemeChange',
      'onLineFocusChanged',
      'togglePresentation',
      'isImmersiveEnabled',
      'getActivePresentationState',
      'isPdf',
      'maybeHasKeyPointsSection',
      'getKeyPointsRegex',
    ]);
  }

  getInSidePanelPresentationState(): number {
    this.methodCalled('getInSidePanelPresentationState');
    return this.inSidePanelPresentationState;
  }

  getInImmersiveOverlayPresentationState(): number {
    this.methodCalled('getInImmersiveOverlayPresentationState');
    return this.inImmersiveOverlayPresentationState;
  }

  getFontName(): string {
    this.methodCalled('getFontName');
    return this.fontName;
  }

  getSupportedFonts(): string[] {
    this.methodCalled('getSupportedFonts');
    return this.supportedFonts;
  }

  getStandardLineSpacing(): number {
    this.methodCalled('getStandardLineSpacing');
    return this.standardLineSpacing;
  }

  getLooseLineSpacing(): number {
    this.methodCalled('getLooseLineSpacing');
    return this.looseLineSpacing;
  }

  getVeryLooseLineSpacing(): number {
    this.methodCalled('getVeryLooseLineSpacing');
    return this.veryLooseLineSpacing;
  }

  getLineSpacing(): number {
    this.methodCalled('getLineSpacing');
    return this.lineSpacing;
  }

  getLineSpacingValue(lineSpacing: number): number {
    this.methodCalled('getLineSpacingValue', lineSpacing);
    return lineSpacing + 1;
  }

  getFontSize(): number {
    this.methodCalled('getFontSize');
    return this.fontSize;
  }

  getStandardLetterSpacing(): number {
    this.methodCalled('getStandardLetterSpacing');
    return this.standardLetterSpacing;
  }

  getWideLetterSpacing(): number {
    this.methodCalled('getWideLetterSpacing');
    return this.wideLetterSpacing;
  }

  getVeryWideLetterSpacing(): number {
    this.methodCalled('getVeryWideLetterSpacing');
    return this.veryWideLetterSpacing;
  }

  getDefaultTheme(): number {
    this.methodCalled('getDefaultTheme');
    return this.defaultTheme;
  }

  getLightTheme(): number {
    this.methodCalled('getLightTheme');
    return this.lightTheme;
  }

  getDarkTheme(): number {
    this.methodCalled('getDarkTheme');
    return this.darkTheme;
  }

  getYellowTheme(): number {
    this.methodCalled('getYellowTheme');
    return this.yellowTheme;
  }

  getBlueTheme(): number {
    this.methodCalled('getBlueTheme');
    return this.blueTheme;
  }

  getHighContrastTheme(): number {
    this.methodCalled('getHighContrastTheme');
    return this.highContrastTheme;
  }

  getLowContrastLightTheme(): number {
    this.methodCalled('getLowContrastLightTheme');
    return this.lowContrastLightTheme;
  }

  getLowContrastDarkTheme(): number {
    this.methodCalled('getLowContrastDarkTheme');
    return this.lowContrastDarkTheme;
  }

  getActiveDistillationMethod(): number {
    this.methodCalled('getActiveDistillationMethod');
    return this.activeDistillationMethod;
  }

  getDistillationTypeReadability(): number {
    this.methodCalled('getDistillationTypeReadability');
    return this.distillationTypeReadability;
  }

  requestImageData(nodeId: number): void {
    this.methodCalled('requestImageData', nodeId);
  }

  isLineFocusEnabled(): boolean {
    this.methodCalled('isLineFocusEnabled');
    return this.lineFocusEnabled;
  }

  getLineFocusOff(): number {
    this.methodCalled('getLineFocusOff');
    return this.lineFocusOff;
  }

  getLineFocusSmallStaticWindow(): number {
    this.methodCalled('getLineFocusSmallStaticWindow');
    return this.lineFocusSmallStaticWindow;
  }

  getLineFocusMediumStaticWindow(): number {
    this.methodCalled('getLineFocusMediumStaticWindow');
    return this.lineFocusMediumStaticWindow;
  }

  getLineFocusLargeStaticWindow(): number {
    this.methodCalled('getLineFocusLargeStaticWindow');
    return this.lineFocusLargeStaticWindow;
  }

  getLineFocusSmallCursorWindow(): number {
    this.methodCalled('getLineFocusSmallCursorWindow');
    return this.lineFocusSmallCursorWindow;
  }

  getLineFocusMediumCursorWindow(): number {
    this.methodCalled('getLineFocusMediumCursorWindow');
    return this.lineFocusMediumCursorWindow;
  }

  getLineFocusLargeCursorWindow(): number {
    this.methodCalled('getLineFocusLargeCursorWindow');
    return this.lineFocusLargeCursorWindow;
  }

  getLineFocusStaticLine(): number {
    this.methodCalled('getLineFocusStaticLine');
    return this.lineFocusStaticLine;
  }

  getLineFocusCursorLine(): number {
    this.methodCalled('getLineFocusCursorLine');
    return this.lineFocusCursorLine;
  }

  onFontChange(font: string): void {
    this.methodCalled('onFontChange', font);
  }

  onLineSpacingChange(value: number): void {
    this.methodCalled('onLineSpacingChange', value);
  }

  onLetterSpacingChange(value: number): void {
    this.methodCalled('onLetterSpacingChange', value);
  }

  onThemeChange(theme: number): void {
    this.methodCalled('onThemeChange', theme);
  }

  onLineFocusChanged(value: number, lastNonDisabledValue: number): void {
    this.methodCalled('onLineFocusChanged', value, lastNonDisabledValue);
  }

  togglePresentation(): void {
    this.methodCalled('togglePresentation');
  }

  isImmersiveEnabled(): boolean {
    this.methodCalled('isImmersiveEnabled');
    return this.immersiveEnabled;
  }

  getActivePresentationState(): number {
    this.methodCalled('getActivePresentationState');
    return this.activePresentationState;
  }

  isPdf(): boolean {
    this.methodCalled('isPdf');
    return this.pdf;
  }

  maybeHasKeyPointsSection(): boolean {
    this.methodCalled('maybeHasKeyPointsSection');
    return this.keyPointsSection;
  }

  getKeyPointsRegex(): string {
    this.methodCalled('getKeyPointsRegex');
    return this.keyPointsRegex;
  }
}
