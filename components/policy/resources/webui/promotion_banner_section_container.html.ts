// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {PromotionBannerSectionContainerElement} from './promotion_banner_section_container.js';

export function getHtml(this: PromotionBannerSectionContainerElement) {
  return html`<!--_html_template_start_-->
<section id="promotion-banner-section-main" class="banner-section">
  <div id="promotion-banner-section" class="banner-section-container">
    <div id="promotion-icon" class="promotion-icon-container"
        aria-hidden="true">
      <picture>
        <source
            srcset="chrome://resources/images/dark/promotion_banner_dark.svg"
            media="(prefers-color-scheme: dark)">
        <img src="chrome://resources/images/promotion_banner_light.svg" alt="">
      </picture>
    </div>
    <div id="banner-actions" class="banner-actions-container">
      <div id="promotion-main-text" class="promotion-main-text-container">
        <div id="promotion-text" class="promotion-text-container">
          <h2 id="promotion-banner-title" class="banner-title">
            $i18n{promotionBannerTitle}
          </h2>
          <div id="promotion-banner-description" class="banner-description">
            $i18n{promotionBannerDesc}
          </div>
        </div>
        <!-- TODO(crbug.com/380495561): Replace with redirect logic once
             rpc service is ready -->
        <button id="promotion-redirect-button" class="blue-pill-button"
            @click="${this.onRedirectClick}">
          $i18n{promotionBannerBtn}
        </button>
      </div>
      <button id="promotion-dismiss-button" class="dismiss-button" tabindex="0"
          @click="${this.onDismissClick}">
        <div id="close-icon-container"></div>
      </button>
    </div>
  </div>
</section>
<!--_html_template_end_-->`;
}
