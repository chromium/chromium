// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://personalization/polymer/v3_0/polymer/polymer_bundled.min.js';

const styles = document.createElement('dom-module');

styles.innerHTML = `
<template>
  <style>
    /* Different breakpoint for inside the iframe. Use !important to make sure
     * there are no css ordering issues. */
    @media (min-width: 688px) {
      .photo-container {
        width: 25% !important;
      }
    }
  </style>
</template>`;

styles.register('untrusted-style');
