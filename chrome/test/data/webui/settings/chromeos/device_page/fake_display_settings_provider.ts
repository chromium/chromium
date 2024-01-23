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
type DisplaySettingsOrientationOption =
    displaySettingsProviderMojom.DisplaySettingsOrientationOption;

export class FakeDisplaySettingsProvider implements
    DisplaySettingsProviderInterface {
  private tabletModeObservers: TabletModeObserverInterface[] = [];
  private displayConfigurationObservers:
      DisplayConfigurationObserverInterface[] = [];
  private isTabletMode: boolean = false;
  private internalDisplayHistogram = new Map<DisplaySettingsType, number>();
  private externalDisplayHistogram = new Map<DisplaySettingsType, number>();
  private displayHistogram = new Map<DisplaySettingsType, number>();
  // First key indicates internal or external display. Second key indicates the
  // orientation. The value indicates the histogram count.
  private displayOrientationHistogram =
      new Map<boolean, Map<DisplaySettingsOrientationOption, number>>();

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

    if (type ===
            displaySettingsProviderMojom.DisplaySettingsType.kOrientation &&
        value.isInternalDisplay !== undefined &&
        value.orientation !== undefined) {
      const orientationHistogram =
          this.getDisplayOrientationHistogram(value.isInternalDisplay);
      orientationHistogram.set(
          value.orientation,
          (orientationHistogram.get(value.orientation) || 0) + 1);
      this.displayOrientationHistogram.set(
          value.isInternalDisplay, orientationHistogram);
    }
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

  getDisplayOrientationHistogram(isInternalDisplay: boolean):
      Map<DisplaySettingsOrientationOption, number> {
    return this.displayOrientationHistogram.get(isInternalDisplay) ||
        new Map<DisplaySettingsOrientationOption, number>();
  }
}
