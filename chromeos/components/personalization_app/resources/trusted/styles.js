// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

const styles = document.createElement('dom-module');

styles.innerHTML = `
<template>
  <style>
    /* There is a corresponding media query for iframe grids because media
     * queries inside iframes reference width of the frame, not the entire
     * window. Use !important to make sure there are no css ordering issues.
     * Subtract 0.25px to fix subpixel rounding issues with iron-list. This
     * makes sure all photo containers on a row add up to at least 1px smaller
     * than the parent width.*/

    @media (min-width: 720px) {
      .photo-container {
        width: calc(25% - 0.25px) !important;
      }
    }
    main,
    iframe {
      height: 100%;
      width: 100%;
    }
    main:focus,
    main:focus-visible,
    main:focus-within,
    iframe:focus,
    iframe:focus-visible,
    iframe:focus-within {
      outline: none;
    }
  </style>
</template>`;

styles.register('trusted-style');
