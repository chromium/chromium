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
    paper-spinner-lite {
      display: none;
      height: 28px;
      margin: 150px auto;
    }
    paper-spinner-lite[active] {
      display: block;
    }
    /* There is a corresponding media query for iframe grids because media
     * queries inside iframes reference width of the frame, not the entire
     * window. Use !important to make sure there are no css ordering issues. */
    @media (min-width: 720px) {
      .photo-container {
        width: 25% !important;
      }
    }
    iframe {
      height: 100%;
      width: 100%;
    }
  </style>
</template>`;

styles.register('trusted-style');
