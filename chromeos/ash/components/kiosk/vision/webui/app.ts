// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import { loadTimeData } from '//resources/js/load_time_data.js';
import { PolymerElement } from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
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
      message_: {
        type: String,
        value: () => loadTimeData.getString('message'),
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'kiosk-vision-internals-app': KioskVisionInternalsAppElement;
  }
}

customElements.define(
  KioskVisionInternalsAppElement.is, KioskVisionInternalsAppElement);


