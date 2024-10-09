// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


export const IS_IOS = /CriOS/.test(window.navigator.userAgent);

export const IS_HIDPI = window.devicePixelRatio > 1;
