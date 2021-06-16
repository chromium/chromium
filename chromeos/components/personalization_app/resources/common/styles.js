// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const styles = document.createElement('dom-module');

styles.innerHTML = `<template>
    <style>
        .photo-container {
            height: 128px;
            width: 128px;
        }
        .photo-container > img {
            height: 100%;
            object-fit: contain;
            width: 100%;
        }
        .photo-container > .collection-name,
        .photo-container > .image-name {
            bottom: 0;
            position: absolute;
            text-align: center;
            width: 100%;
        }
    </style>
  </template>`;

styles.register('common-style');
