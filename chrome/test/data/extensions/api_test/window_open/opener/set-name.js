// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.name = 'target-window';
chrome.extension.getBackgroundPage().onSetNameLoaded(window);
