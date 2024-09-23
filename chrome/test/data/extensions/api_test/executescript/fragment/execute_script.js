// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Notify the background page that we ran.
chrome.runtime.sendMessage("execute_script");
