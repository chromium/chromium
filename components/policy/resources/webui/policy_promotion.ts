// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getTemplate} from './policy_promotion.html.js';


export class PolicyPromotionElement extends CustomElement{
    static override get template() {
        return getTemplate();
      }
}

declare global {
    interface HTMLElementTagNameMap {
      'promotion-banner-section-container': PolicyPromotionElement;
    }
  }
  customElements.define('promotion-banner-section-container', PolicyPromotionElement);
