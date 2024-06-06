// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import { BrowserProxy } from './browser_proxy.js';
import { Status, type State } from './kiosk_vision_internals.mojom-webui.js';
import {
  PolymerElement,
} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import { getTemplate } from './app.html.js';

export class KioskVisionInternalsAppElement extends PolymerElement {
  static get is() {
    return 'kiosk-vision-internals-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      state_: Object,
    };
  }

  private browserProxy_: BrowserProxy;
  private state_: State;

  constructor() {
    super();
    this.browserProxy_ = BrowserProxy.getInstance();
    this.browserProxy_.callbackRouter.display.addListener(
      (state: State) => { this.state_ = state; });
    this.state_ = { status: Status.kUnknown, boxes: [] };
  }

  private statusIs_(state: State, status: keyof typeof Status): boolean {
    return state.status === Status[status];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'kiosk-vision-internals-app': KioskVisionInternalsAppElement;
  }
}

customElements.define(
  KioskVisionInternalsAppElement.is, KioskVisionInternalsAppElement);

