// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// remove_self.js runs at both document_start and document_end, so we use this
// script (that only runs at document_end) to distinguish between the two.
window.didRunAtDocumentEnd = true;
