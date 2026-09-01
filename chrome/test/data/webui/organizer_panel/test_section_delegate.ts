// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OrganizerListSectionClient, OrganizerListSectionDelegate, OrganizerListSectionItem} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';

export class TestSectionDelegate implements OrganizerListSectionDelegate {
  private header_: string;
  private items_: OrganizerListSectionItem[];

  constructor(header: string, items: OrganizerListSectionItem[] = []) {
    this.header_ = header;
    this.items_ = items;
  }

  init(_sectionClient: OrganizerListSectionClient) {}

  getHeader(): string {
    return this.header_;
  }

  getItems(): Promise<OrganizerListSectionItem[]> {
    return Promise.resolve(this.items_);
  }
}
