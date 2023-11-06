// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Changes the document's title based on the existence/value of
// window.mainWorldFlag, which is set by a script that's part of a web page.
document.title = window.mainWorldFlag === 'from main world' ?
  'MAIN_WORLD' :
  'USER_SCRIPTS_WORLD';
