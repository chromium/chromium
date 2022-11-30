// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

runTests([
  // Navigates to a page with a form and submits it, generating a POST request.
  // Multipart-encoded form.
  sendPost('multipart.html', true /*parseableForm*/),
  // An unparseable form, thus only raw POST data are extracted.
  sendPost('plaintext.html', false /*parseableForm*/),
]);
