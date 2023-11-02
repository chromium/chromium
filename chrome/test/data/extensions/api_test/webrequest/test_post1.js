// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

runTests([
  // Navigates to a page with a form and submits it, generating a POST request.
  // A form in default encoding (which is "urlencoded").
  sendPost('no-enctype.html', true /*parseableForm*/),
  // Urlencoded form.
  sendPost('urlencoded.html', true /*parseableForm*/),
]);
