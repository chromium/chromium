// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OrganizerListSectionClient, OrganizerListSectionDelegate, OrganizerListSectionItem} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';

export class TestSectionDelegate implements
    OrganizerListSectionDelegate<unknown> {
  private header_: string;
  private items_: Array<OrganizerListSectionItem<unknown>>;

  private lastClickedItem_?: OrganizerListSectionItem<unknown>;
  private clickCount_: number = 0;

  private lastActionButtonClickedItem_?: OrganizerListSectionItem<unknown>;
  private lastActionButtonElement_?: HTMLElement;
  private actionButtonClickCount_: number = 0;

  constructor(
      header: string, items: Array<OrganizerListSectionItem<unknown>> = []) {
    this.header_ = header;
    this.items_ = items;
  }

  init(_sectionClient: OrganizerListSectionClient) {}

  getHeader(): string {
    return this.header_;
  }

  getItems(): Promise<Array<OrganizerListSectionItem<unknown>>> {
    return Promise.resolve(this.items_);
  }

  onItemClick(item: OrganizerListSectionItem<unknown>) {
    this.lastClickedItem_ = item;
    this.clickCount_++;
  }

  getLastClickedItem(): OrganizerListSectionItem<unknown>|undefined {
    return this.lastClickedItem_;
  }

  getClickCount(): number {
    return this.clickCount_;
  }

  onItemActionButtonClicked(
      item: OrganizerListSectionItem<unknown>, buttonElement: HTMLElement) {
    this.lastActionButtonClickedItem_ = item;
    this.lastActionButtonElement_ = buttonElement;
    this.actionButtonClickCount_++;
  }

  getLastActionButtonClickedItem():
      OrganizerListSectionItem<unknown>|undefined {
    return this.lastActionButtonClickedItem_;
  }

  getLastActionButtonElement(): HTMLElement|undefined {
    return this.lastActionButtonElement_;
  }

  getActionButtonClickCount(): number {
    return this.actionButtonClickCount_;
  }
}
