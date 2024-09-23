// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LaunchSource, PrintManagementHandlerInterface} from 'chrome://print-management/printing_manager.mojom-webui.js';

export class FakePrintManagementHandler implements
    PrintManagementHandlerInterface {
  private launchPrinterSettingsCount: number = 0;
  private lastLaunchSource: LaunchSource|null = null;

  constructor() {
    this.resetForTest();
  }

  launchPrinterSettings(source: LaunchSource): void {
    ++this.launchPrinterSettingsCount;
    this.lastLaunchSource = source;
  }

  recordGetPrintJobsRequestDuration(): void {}

  getLaunchPrinterSettingsCount(): number {
    return this.launchPrinterSettingsCount;
  }

  getLastLaunchSource(): LaunchSource|null {
    return this.lastLaunchSource;
  }

  resetForTest(): void {
    this.launchPrinterSettingsCount = 0;
    this.lastLaunchSource = null;
  }
}
