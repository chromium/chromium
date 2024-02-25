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
type DisplaySettingsNightLightScheduleOption =
    displaySettingsProviderMojom.DisplaySettingsNightLightScheduleOption;

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
  // First key indicates internal or external display. Second key indicates the
  // night light status. The value indicates the histogram count.
  private displayNightLightStatusHistogram =
      new Map<boolean, Map<boolean, number>>();
  // First key indicates internal or external display. Second key indicates the
  // night light schedule. The value indicates the histogram count.
  private displayNightLightScheduleHistogram =
      new Map<boolean, Map<DisplaySettingsNightLightScheduleOption, number>>();
  // The key is the mirror mode status. The value indicates the histogram count.
  private displayMirrorModeStatusHistogram = new Map<boolean, number>();
  // The key is the unified mode status. The value indicates the histogram
  // count.
  private displayUnifiedModeStatusHistogram = new Map<boolean, number>();

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
    } else if (
        type === displaySettingsProviderMojom.DisplaySettingsType.kNightLight &&
        value.isInternalDisplay !== undefined &&
        value.nightLightStatus !== undefined) {
      const nightLightStatusHistogram =
          this.getDisplayNightLightStatusHistogram(value.isInternalDisplay);
      nightLightStatusHistogram.set(
          value.nightLightStatus,
          (nightLightStatusHistogram.get(value.nightLightStatus) || 0) + 1);
      this.displayNightLightStatusHistogram.set(
          value.isInternalDisplay, nightLightStatusHistogram);
    } else if (
        type ===
            displaySettingsProviderMojom.DisplaySettingsType
                .kNightLightSchedule &&
        value.isInternalDisplay !== undefined &&
        value.nightLightSchedule !== undefined) {
      const nightLightScheduleHistogram =
          this.getDisplayNightLightScheduleHistogram(value.isInternalDisplay);
      nightLightScheduleHistogram.set(
          value.nightLightSchedule,
          (nightLightScheduleHistogram.get(value.nightLightSchedule) || 0) + 1);
      this.displayNightLightScheduleHistogram.set(
          value.isInternalDisplay, nightLightScheduleHistogram);
    } else if (
        type === displaySettingsProviderMojom.DisplaySettingsType.kMirrorMode &&
        value.isInternalDisplay === undefined &&
        value.mirrorModeStatus !== undefined) {
      this.displayMirrorModeStatusHistogram.set(
          value.mirrorModeStatus,
          (this.displayMirrorModeStatusHistogram.get(value.mirrorModeStatus) ||
           0) +
              1);
    } else if (
        type ===
            displaySettingsProviderMojom.DisplaySettingsType.kUnifiedMode &&
        value.isInternalDisplay === undefined &&
        value.unifiedModeStatus !== undefined) {
      this.displayUnifiedModeStatusHistogram.set(
          value.unifiedModeStatus,
          (this.displayUnifiedModeStatusHistogram.get(
               value.unifiedModeStatus) ||
           0) +
              1);
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

  getDisplayNightLightStatusHistogram(isInternalDisplay: boolean):
      Map<boolean, number> {
    return this.displayNightLightStatusHistogram.get(isInternalDisplay) ||
        new Map<boolean, number>();
  }

  getDisplayNightLightScheduleHistogram(isInternalDisplay: boolean):
      Map<DisplaySettingsNightLightScheduleOption, number> {
    return this.displayNightLightScheduleHistogram.get(isInternalDisplay) ||
        new Map<DisplaySettingsNightLightScheduleOption, number>();
  }

  getDisplayMirrorModeStatusHistogram(): Map<boolean, number> {
    return this.displayMirrorModeStatusHistogram;
  }

  getDisplayUnifiedModeStatusHistogram(): Map<boolean, number> {
    return this.displayUnifiedModeStatusHistogram;
  }
}
