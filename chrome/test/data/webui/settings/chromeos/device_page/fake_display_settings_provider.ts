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

export class FakeDisplaySettingsProvider implements
    DisplaySettingsProviderInterface {
  private tabletModeObservers: TabletModeObserverInterface[] = [];
  private isTabletMode: boolean = false;

  observeTabletMode(observer: TabletModeObserverInterface):
      Promise<{isTabletMode: boolean}> {
    this.tabletModeObservers.push(observer);
    this.notifyTabletModeChanged();

    return Promise.resolve({isTabletMode: this.isTabletMode});
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
}
