// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const styles = document.createElement('dom-module');

styles.innerHTML = `<template>
    <style>
      paper-spinner-lite {
        display: none;
        height: 28px;
        margin: 150px auto;
      }
      paper-spinner-lite[active] {
        display: block;
      }
      iframe, iron-list {
        height: 80vh;
        width: 100%;
      }
    </style>
  </template>`;

styles.register('trusted-style');
