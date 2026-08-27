// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OrganizerListSectionDelegate, OrganizerListSectionItem} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';

export class TestSectionDelegate implements OrganizerListSectionDelegate {
  private header_: string;
  private items_: OrganizerListSectionItem[];

  constructor(header: string, items: OrganizerListSectionItem[] = []) {
    this.header_ = header;
    this.items_ = items;
  }

  getHeader(): string {
    return this.header_;
  }

  getItems(): OrganizerListSectionItem[] {
    return this.items_;
  }
}
