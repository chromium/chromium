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

  constructor() {
    super([
      'getInSidePanelPresentationState',
      'getInImmersiveOverlayPresentationState',
      'getFontName',
      'getSupportedFonts',
      'getStandardLineSpacing',
      'getLooseLineSpacing',
      'getVeryLooseLineSpacing',
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
      'onFontChange',
      'onLineSpacingChange',
      'onLetterSpacingChange',
      'onThemeChange',
      'togglePresentation',
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

  togglePresentation(): void {
    this.methodCalled('togglePresentation');
  }
}
