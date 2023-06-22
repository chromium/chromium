// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PrintManagementHandlerInterface} from 'chrome://print-management/printing_manager.mojom-webui.js';

export class FakePrintManagementHandler implements
    PrintManagementHandlerInterface {
  private launchPrinterSettingsCount: number = 0;

  constructor() {
    this.resetForTest();
  }

  launchPrinterSettings(): void {
    ++this.launchPrinterSettingsCount;
  }

  getLaunchPrinterSettingsCount(): number {
    return this.launchPrinterSettingsCount;
  }

  resetForTest(): void {
    this.launchPrinterSettingsCount = 0;
  }
}
