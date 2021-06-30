// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Common styles for polymer components in both trusted and
 * untrusted code. Polymer must be imported before this file. This file cannot
 * import Polymer itself because trusted and untrusted code access polymer at
 * different paths.
 */
const styles = document.createElement('dom-module');

styles.innerHTML = `
<template>
  <style>
    iron-list {
      height: 100%;
    }
    .photo-container {
      box-sizing: border-box;
      /* 8 + 120 + 8 */
      height: 136px;
      overflow: hidden;
      padding: 8px;
      /* Media queries in trusted and untrusted code will resize to 25% at
         correct widths */
      width: calc(100% / 3);
    }
    .photo-container > img {
      height: 100%;
      object-fit: cover;
      width: 100%;
    }
    .photo-text-container {
      bottom: 0;
      position: absolute;
    }
  </style>
</template>`;

styles.register('common-style');
