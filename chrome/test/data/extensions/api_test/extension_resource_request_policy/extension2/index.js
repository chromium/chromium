// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let image = document.createElement('img');
image.onload = () => { document.title = 'Loaded'; };
image.onerror = () => { document.title = 'Image failed to load'; };
image.src = 'chrome-extension://fnbdbepgnidhjejikpionpfohdjjogpm/test.png';
document.body.appendChild(image);
