// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Icons specific to personalization app.
 * This file is run in both trusted and untrusted code, and therefore
 * cannot import polymer and iron-iconset-svg itself. Any consumer should
 * import necessary dependencies before this file.
 *
 * Following the demo here:
 * @see https://github.com/PolymerElements/iron-iconset-svg/blob/v3.0.1/demo/svg-sample-icons.js
 */

const template = document.createElement('template');

template.innerHTML = `
<iron-iconset-svg name="personalization" size="20">
  <svg>
    <defs>
      <g id="checkmark">
        <!-- this is visually identical to os-settings:ic-checked-filled, but
             that icon is not accessible from chrome-untrusted://personalization
        -->
        <svg width="20" height="20" fill="none"
            xmlns="http://www.w3.org/2000/svg">
          <circle cx="10" cy="10" r="8" fill="white"/>
          <path d="M10 1.66666C5.40001 1.66666 1.66667 5.39999 1.66667 9.99999C1.66667 14.6 5.40001 18.3333 10 18.3333C14.6 18.3333 18.3333 14.6 18.3333 9.99999C18.3333 5.39999 14.6 1.66666 10 1.66666ZM8.33334 14.1667L5.00001 10.8333L6.16667 9.66666L8.33334 11.8333L13.8333 6.33332L15 7.49999L8.33334 14.1667Z" fill="#1A73E8"/>
        </svg>
      </g>
    </defs>
  </svg>
</iron-iconset-svg>`;

document.head.appendChild(template.content);
