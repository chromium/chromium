// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.system.display for testing.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';

type SystemDisplayApi = typeof chrome.system.display;
type DisplayLayout = chrome.system.display.DisplayLayout;
type DisplayProperties = chrome.system.display.DisplayProperties;
type DisplayUnitInfo = chrome.system.display.DisplayUnitInfo;
type MirrorModeInfo = chrome.system.display.MirrorModeInfo;
type GetInfoFlags = chrome.system.display.GetInfoFlags;

/**
 * Fake of the chrome.system.display API.
 */
export class FakeSystemDisplay implements SystemDisplayApi {
  fakeDisplays: DisplayUnitInfo[] = [];
  fakeLayouts: DisplayLayout[] = [];
  getInfoCalled = new PromiseResolver();
  getLayoutCalled = new PromiseResolver();
  overscanCalibrationStartCalled = 0;
  overscanCalibrationResetCalled = 0;
  overscanCalibrationCompleteCalled = 0;
  onDisplayChanged = new FakeChromeEvent();

  // The following properties mirror the necessary enum members.
  /* eslint-disable @typescript-eslint/naming-convention */
  LayoutPosition = chrome.system.display.LayoutPosition;
  ActiveState = chrome.system.display.ActiveState;
  MirrorMode = chrome.system.display.MirrorMode;
  /* eslint-enable @typescript-eslint/naming-convention */

  addDisplayForTest(display: DisplayUnitInfo): void {
    this.fakeDisplays.push(display);
    this.updateLayouts_();
  }

  getInfo(_flags?: GetInfoFlags): Promise<DisplayUnitInfo[]> {
    return new Promise((resolve) => {
      setTimeout(() => {
        // Create a shallow copy to trigger Polymer data binding updates.
        let displays: DisplayUnitInfo[] = [];
        if (this.fakeDisplays.length > 0 &&
            this.fakeDisplays[0]!.mirroringSourceId) {
          // When mirroring is enabled, send the info for the displays not in
          // destination.
          const display =
              this.getFakeDisplay_(this.fakeDisplays[0]!.mirroringSourceId);
          assert(display);
          for (const fakeDisplay of this.fakeDisplays) {
            if (display.mirroringDestinationIds &&
                !display.mirroringDestinationIds.includes(fakeDisplay.id)) {
              displays.push(fakeDisplay);
            }
          }
        } else {
          displays = this.fakeDisplays.slice();
        }
        resolve(displays);

        this.getInfoCalled.resolve(null);
        // Reset the promise resolver.
        this.getInfoCalled = new PromiseResolver();
      });
    });
  }

  setDisplayProperties(id: string, info: DisplayProperties): Promise<void> {
    const display = this.getFakeDisplay_(id);
    if (!display) {
      chrome.runtime.lastError = {message: 'Display not found.'};
      return Promise.reject();
    }

    if (info.mirroringSourceId !== undefined) {
      for (const d of this.fakeDisplays) {
        d.mirroringSourceId = info.mirroringSourceId;
      }
    }

    if (info.isPrimary !== undefined) {
      let havePrimary = info.isPrimary;
      for (const fakeDisplay of this.fakeDisplays) {
        if (fakeDisplay.id === id) {
          fakeDisplay.isPrimary = info.isPrimary;
        } else if (havePrimary) {
          fakeDisplay.isPrimary = false;
        } else {
          fakeDisplay.isPrimary = true;
          havePrimary = true;
        }
      }
      this.updateLayouts_();
    }
    if (info.rotation !== undefined) {
      display.rotation = info.rotation;
    }
    return Promise.resolve();
  }

  getDisplayLayout(): Promise<DisplayLayout[]> {
    return new Promise((resolve) => {
      setTimeout(() => {
        // Create a shallow copy to trigger Polymer data binding updates.
        resolve(this.fakeLayouts.slice());
        this.getLayoutCalled.resolve(null);
        // Reset the promise resolver.
        this.getLayoutCalled = new PromiseResolver();
      });
    });
  }

  async setDisplayLayout(layouts: DisplayLayout[]): Promise<void> {
    this.fakeLayouts = layouts;
  }

  async setMirrorMode(info: MirrorModeInfo): Promise<void> {
    let mirroringSourceId = '';
    let mirroringDestinationIds: string[] = [];
    if (info.mode === this.MirrorMode.NORMAL) {
      // Select the primary display as the mirroring source.
      for (const fakeDisplay of this.fakeDisplays) {
        if (fakeDisplay.isPrimary) {
          mirroringSourceId = fakeDisplay.id;
        } else {
          mirroringDestinationIds.push(fakeDisplay.id);
        }
      }
    } else if (info.mode === this.MirrorMode.MIXED) {
      mirroringSourceId = info.mirroringSourceId as string;
      mirroringDestinationIds = info.mirroringDestinationIds as string[];
    }

    for (const fakeDisplay of this.fakeDisplays) {
      fakeDisplay.mirroringSourceId = mirroringSourceId;
      fakeDisplay.mirroringDestinationIds = mirroringDestinationIds;
    }
  }

  // The below method is overridden to provide TS compatibility for tests.
  // But this is an unused method and hence doesn't have any implementation.
  overscanCalibrationAdjust(_id: string): void {}

  async overscanCalibrationStart(): Promise<void> {
    this.overscanCalibrationStartCalled++;
  }

  async overscanCalibrationReset(): Promise<void> {
    this.overscanCalibrationResetCalled++;
  }

  async overscanCalibrationComplete(): Promise<void> {
    this.overscanCalibrationCompleteCalled++;
  }

  async showNativeTouchCalibration(_id: string): Promise<boolean> {
    return true;
  }

  private getFakeDisplay_(id: string): DisplayUnitInfo|undefined {
    return this.fakeDisplays.find((display) => {
      return display.id === id;
    });
  }

  private updateLayouts_(): void {
    this.fakeLayouts = [];
    let primaryId = '';
    for (const fakeDisplay of this.fakeDisplays) {
      if (fakeDisplay.isPrimary) {
        primaryId = fakeDisplay.id;
        break;
      }
    }

    this.fakeLayouts = this.fakeDisplays.map((fakeDisplay) => {
      return {
        id: fakeDisplay.id,
        parentId: fakeDisplay.isPrimary ? '' : primaryId,
        position: this.LayoutPosition.RIGHT,
        offset: 0,
      };
    });
  }
}
