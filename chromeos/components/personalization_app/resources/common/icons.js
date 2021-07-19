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
  <svg>
    <defs>
      <g id="change-daily">
      <svg width="20" height="20" viewBox="0 0 20 20" fill="none"
            xmlns="http://www.w3.org/2000/svg">>
          <rect width="20" height="20" fill="white"/>
          <path d="M17.4989 4.16672H19.1656V15.8334H17.4989V4.16672ZM14.1656
          4.16672H15.8322V15.8334H14.1656V4.16672ZM11.6656
          4.16672H1.66558C1.20724 4.16672 0.832245 4.54172 0.832245
          5.00005V15.0001C0.832245 15.4584 1.20724 15.8334 1.66558
          15.8334H11.6656C12.1239 15.8334 12.4989 15.4584 12.4989
          15.0001V5.00005C12.4989 4.54172 12.1239 4.16672 11.6656
          4.16672ZM10.8322 14.1667H2.49891V5.83338H10.8322V14.1667ZM8.00724
          9.37505L6.04058 11.7084L4.79058 10.0001L2.91558
          12.5001H10.4156L8.00724 9.37505Z" fill="#1A73E8"/>
        </svg>
      </g>
    </defs>
  </svg>
  <svg>
    <defs>
      <g id="refresh">
      <svg width="20" height="20" viewBox="0 0 20 20" fill="none"
          xmlns="http://www.w3.org/2000/svg">
          <rect width="20" height="20" fill="white"/>
          <path d="M10 3C6.136 3 3 6.136 3 10C3 13.864 6.136 17 10 17C12.1865 17
          14.1399 15.9959 15.4239 14.4239L13.9984 12.9984C13.0852 14.2129
          11.6325 15 10 15C7.24375 15 5 12.7563 5 10C5 7.24375 7.24375 5 10
          5C11.6318 5 13.0839 5.78641 13.9972 7H11V9H17V3H15V5.10253C13.7292
          3.80529 11.9581 3 10 3Z" fill="#1A73E8"/>
      </svg>
      </g>
    </defs>
  </svg>
</iron-iconset-svg>`;

document.head.appendChild(template.content);
