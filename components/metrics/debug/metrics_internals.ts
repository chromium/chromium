// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.addEventListener('beforeunload', function(event) {
  // Since leaving the page discards the accumulated logs, show a confirmation
  // dialog when the user attempts to leave.
  event.returnValue = '';
});
