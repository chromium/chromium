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
      --personalization-app-text-shadow-elevation-1: 0 1px 3px
          rgba(0, 0, 0, 15%), 0 1px 2px rgba(0, 0, 0, 30%);

      /* copied from |AshColorProvider| |kSecondToneOpacity| constant. */
      --personalization-app-second-tone-opacity: 0.3;

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
      --personalization-app-typeface-annotation-2: {
        font-family: var(--personalization-app-font-roboto);
        font-weight: 400;
        font-size: 11px;
        line-height: 16px;
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
    .photo-container:focus-visible {
      outline: none;
    }
    /* This extra position: relative element corrects for absolutely positioned
       elements ignoring parent interior padding. */
    .photo-inner-container {
      align-items: center;
      display: flex;
      height: 100%;
      justify-content: center;
      position: relative;
      width: 100%;
    }
    .photo-container:focus-visible .photo-inner-container {
      border: 2px solid var(--cros-focus-ring-color);
      border-radius: 14px;
    }

    .photo-images-container {
      border-radius: 12px;
      box-sizing: border-box;
      display: flex;
      flex-flow: row wrap;
      height: 100%;
      /* stop img and gradient-mask from ignoring above border-radius. */
      overflow: hidden;
      width: 100%;
    }
    .photo-images-container img {
      flex: 1 1 0;
      height: 100%;
      min-width: 50%;
      object-fit: cover;
      width: 100%;
    }
    .photo-images-container.photo-images-container-3 img {
      height: 50%;
    }
    .photo-container iron-icon[icon='personalization:checkmark'] {
      --iron-icon-height: 20px;
      --iron-icon-width: 20px;
      left: 8px;
      position: absolute;
      top: 8px;
    }
    .photo-container:not([aria-selected='true'])
    iron-icon[icon='personalization:checkmark'] {
      display: none;
    }
    .photo-container[aria-selected='true'] .photo-inner-container {
      background-color: rgba(var(--cros-icon-color-prominent-rgb),
          var(--personalization-app-second-tone-opacity));
      border-radius: 16px;
    }
    .photo-container[aria-selected='true'] .photo-images-container {
      height: calc(100% - 8px);
      width: calc(100% - 8px);
    }
    .photo-container:focus-visible:not([aria-selected='true'])
    .photo-images-container {
      height: calc(100% - 4px);
      width: calc(100% - 4px);
    }
    .photo-text-container {
      box-sizing: border-box;
      bottom: 8px;
      position: absolute;
      width: 100%;
      z-index: 2;
    }
    .photo-text-container > p {
      @apply --personalization-app-typeface-annotation-2;
      color: white;
      margin: 0;
      max-width: 100%;
      overflow: hidden;
      text-align: center;
      text-overflow: ellipsis;
      text-shadow: var(--personalization-app-text-shadow-elevation-1);
      white-space: nowrap;
    }
    .photo-text-container > p:first-child {
      @apply --personalization-app-typeface-headline-1;
    }
    .photo-gradient-mask {
      border-radius: 12px;
      position: absolute;
      top: 50%;
      left: 0;
      width: 100%;
      height: 50%;
      z-index: 1;
      background: linear-gradient(rgba(var(--google-grey-900-rgb), 0%),
          rgba(var(--google-grey-900-rgb), 50%));
    }
  </style>
</template>`;

styles.register('common-style');
