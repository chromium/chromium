// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OrganizerListItem} from 'chrome://organizer-panel.top-chrome/organizer_list_item.js';
import type {OrganizerListSectionDelegate} from 'chrome://organizer-panel.top-chrome/organizer_list_section_delegate.js';

export class TestSectionDelegate implements OrganizerListSectionDelegate {
  private header_: string;
  private items_: OrganizerListItem[];

  constructor(header: string, items: OrganizerListItem[] = []) {
    this.header_ = header;
    this.items_ = items;
  }

  getHeader(): string {
    return this.header_;
  }

  getItems(): OrganizerListItem[] {
    return this.items_;
  }
}
