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

  constructor() {
    super([
      'getInSidePanelPresentationState',
      'getInImmersiveOverlayPresentationState',
      'getFontName',
      'getSupportedFonts',
      'onFontChange',
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

  onFontChange(font: string): void {
    this.methodCalled('onFontChange', font);
  }

  togglePresentation(): void {
    this.methodCalled('togglePresentation');
  }
}
