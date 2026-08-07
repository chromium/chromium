// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Marks the document to signal that the content script was injected. The test
// checks document.title to detect injection.
document.title = 'INJECTED';
