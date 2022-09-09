// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// If the script was really injected at document_start, then document.body will
// be null. If it's not null, then we didn't inject at document_start.
var isDocumentStart = !document.body;

// Set the title of the document to the success state (so that it's easily
// readable from the C++ side). Of course, since this is (hopefully!)
// document start, we need to wait for the page to load before setting
// it. That's fine; we calculated the success value at the right moment.
window.onload = () => {
  document.title = isDocumentStart ? 'success' : 'failure';
};
