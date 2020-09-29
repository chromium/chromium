// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', () => {
  const mainScript = document.createElement('script');
  mainScript.setAttribute('type', 'module');
  mainScript.setAttribute('src', '/js/main.js');
  document.head.appendChild(mainScript);
});
