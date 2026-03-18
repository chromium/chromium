// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: Using `var` because multiple scripts are injected that append divs.
var div = document.createElement('div');
div.id = 'injected_3';
document.body.appendChild(div);
