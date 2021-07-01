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
    :host {
      --personalization-app-font-google-sans: 'Google Sans', 'Noto Sans',
          sans-serif;
      --personalization-app-font-roboto: Roboto, 'Noto Sans', sans-serif;

      --personalization-app-typeface-headline-1: {
        font-family: var(--personalization-app-font-google-sans);
        font-weight: 500;
        font-size: 15px;
        line-height: 22px;
      };
      --personalization-app-typeface-body-2: {
        font-family: var(--personalization-app-font-roboto);
        font-weight: 400;
        font-size: 13px;
        line-height: 20px;
      };
      --personalization-app-typeface-display-6: {
        font-family: var(--personalization-app-font-google-sans);
        font-weight: 500;
        font-size: 22px;
        line-height: 28px;
      };
    }
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
