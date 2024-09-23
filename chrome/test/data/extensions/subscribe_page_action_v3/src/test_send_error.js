// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Used to communicate feed parsing errors to the testing framework.

if (window.domAutomationController) {
  window.domAutomationController.send('Error');
}
