// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {displaySettingsProviderMojom} from 'chrome://os-settings/os_settings.js';

/**
 * @fileoverview
 * Implements a fake version of the DisplaySettingsProvider mojo
 * interface.
 */

type DisplaySettingsProviderInterface =
    displaySettingsProviderMojom.DisplaySettingsProviderInterface;
type TabletModeObserverInterface =
    displaySettingsProviderMojom.TabletModeObserverInterface;
type DisplayConfigurationObserverInterface =
    displaySettingsProviderMojom.DisplayConfigurationObserverInterface;
type DisplaySettingsType = displaySettingsProviderMojom.DisplaySettingsType;
type DisplaySettingsValue = displaySettingsProviderMojom.DisplaySettingsValue;

export class FakeDisplaySettingsProvider implements
    DisplaySettingsProviderInterface {
  private tabletModeObservers: TabletModeObserverInterface[] = [];
  private displayConfigurationObservers:
      DisplayConfigurationObserverInterface[] = [];
  private isTabletMode: boolean = false;
  private internalDisplayHistogram = new Map<DisplaySettingsType, number>();
  private externalDisplayHistogram = new Map<DisplaySettingsType, number>();
  private displayHistogram = new Map<DisplaySettingsType, number>();

  // Implement DisplaySettingsProviderInterface.
  observeTabletMode(observer: TabletModeObserverInterface):
      Promise<{isTabletMode: boolean}> {
    this.tabletModeObservers.push(observer);
    this.notifyTabletModeChanged();

    return Promise.resolve({isTabletMode: this.isTabletMode});
  }

  // Implement DisplaySettingsProviderInterface.
  observeDisplayConfiguration(observer: DisplayConfigurationObserverInterface):
      Promise<void> {
    this.displayConfigurationObservers.push(observer);

    return Promise.resolve();
  }

  notifyTabletModeChanged(): void {
    for (const observer of this.tabletModeObservers) {
      observer.onTabletModeChanged(this.isTabletMode);
    }
  }

  setTabletMode(isTabletMode: boolean): void {
    this.isTabletMode = isTabletMode;
    this.notifyTabletModeChanged();
  }

  getIsTabletMode(): boolean {
    return this.isTabletMode;
  }

  // Implement DisplaySettingsProviderInterface.
  recordChangingDisplaySettings(
      type: DisplaySettingsType, value: DisplaySettingsValue) {
    let histogram: Map<DisplaySettingsType, number>;
    if (value.isInternalDisplay === undefined) {
      histogram = this.displayHistogram;
    } else if (value.isInternalDisplay) {
      histogram = this.internalDisplayHistogram;
    } else {
      histogram = this.externalDisplayHistogram;
    }
    histogram.set(type, (histogram.get(type) || 0) + 1);
  }

  getInternalDisplayHistogram(): Map<DisplaySettingsType, number> {
    return this.internalDisplayHistogram;
  }

  getExternalDisplayHistogram(): Map<DisplaySettingsType, number> {
    return this.externalDisplayHistogram;
  }

  getDisplayHistogram(): Map<DisplaySettingsType, number> {
    return this.displayHistogram;
  }
}
