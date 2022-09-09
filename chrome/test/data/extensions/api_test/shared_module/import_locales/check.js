// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.assertEq("Shared Module message successfully substituted.",
                     chrome.i18n.getMessage('shared_module_message'));

chrome.test.assertEq("Extension successfully overrides Shared Module message.",
                     chrome.i18n.getMessage('overriden_message'));

chrome.test.notifyPass();
