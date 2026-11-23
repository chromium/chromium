// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import type {OrganizerListSectionClient, OrganizerListSectionDelegate, OrganizerListSectionItem} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';
import {OrganizerPanelAction, SectionId} from 'chrome://organizer-panel.top-chrome/organizer_panel.js';

export class TestSectionDelegate implements
    OrganizerListSectionDelegate<unknown> {
  private id_: SectionId;
  private header_: string;
  private items_: Array<OrganizerListSectionItem<unknown>>;
  getZeroState?: () => TemplateResult;
  shouldAlwaysShowZeroState?: () => boolean;
  shouldHide?: () => boolean;
  getActionButtonClickAction?:
      () => OrganizerPanelAction = () => OrganizerPanelAction.kCloseTab;

  private lastClickedItem_?: OrganizerListSectionItem<unknown>;
  private clickCount_: number = 0;

  private lastActionButtonClickedItem_?: OrganizerListSectionItem<unknown>;
  private lastActionButtonElement_?: HTMLElement;
  private actionButtonClickCount_: number = 0;

  private lastContextMenuClickedItem_?: OrganizerListSectionItem<unknown>;
  private lastContextMenuCoordinates_?: {x: number, y: number};
  private contextMenuClickCount_: number = 0;

  private initCount_: number = 0;
  private destroyCount_: number = 0;

  constructor(
      header: string, items: Array<OrganizerListSectionItem<unknown>> = [],
      zeroState?: TemplateResult, id: SectionId = SectionId.kOpenTabs,
      alwaysShowZeroState: boolean = false) {
    this.id_ = id;
    this.header_ = header;
    this.items_ = items;
    if (zeroState) {
      this.getZeroState = () => zeroState;
    }
    if (alwaysShowZeroState) {
      this.shouldAlwaysShowZeroState = () => alwaysShowZeroState;
    }
  }

  init(_sectionClient: OrganizerListSectionClient) {
    this.initCount_++;
  }

  destroy() {
    this.destroyCount_++;
  }

  getInitCount(): number {
    return this.initCount_;
  }

  getDestroyCount(): number {
    return this.destroyCount_;
  }

  getId(): SectionId {
    return this.id_;
  }

  getHeader(): string {
    return this.header_;
  }

  getItems(): Promise<Array<OrganizerListSectionItem<unknown>>> {
    return Promise.resolve(this.items_);
  }

  setItems(items: Array<OrganizerListSectionItem<unknown>>) {
    this.items_ = items;
  }

  onItemClick(item: OrganizerListSectionItem<unknown>) {
    this.lastClickedItem_ = item;
    this.clickCount_++;
  }

  getItemClickAction(): OrganizerPanelAction {
    return OrganizerPanelAction.kSwitchTab;
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

  onItemContextMenuClicked(
      item: OrganizerListSectionItem<unknown>, x: number, y: number) {
    this.lastContextMenuClickedItem_ = item;
    this.lastContextMenuCoordinates_ = {x, y};
    this.contextMenuClickCount_++;
  }

  getLastContextMenuClickedItem(): OrganizerListSectionItem<unknown>|undefined {
    return this.lastContextMenuClickedItem_;
  }

  getLastContextMenuCoordinates(): {x: number, y: number}|undefined {
    return this.lastContextMenuCoordinates_;
  }

  getContextMenuClickCount(): number {
    return this.contextMenuClickCount_;
  }
}
