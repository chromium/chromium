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
      'onFontChange',
      'onLineSpacingChange',
      'onLetterSpacingChange',
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

  onFontChange(font: string): void {
    this.methodCalled('onFontChange', font);
  }

  onLineSpacingChange(value: number): void {
    this.methodCalled('onLineSpacingChange', value);
  }

  onLetterSpacingChange(value: number): void {
    this.methodCalled('onLetterSpacingChange', value);
  }

  togglePresentation(): void {
    this.methodCalled('togglePresentation');
  }
}
