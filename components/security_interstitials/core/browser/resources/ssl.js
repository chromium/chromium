// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupSSLDebuggingInfo() {
  if (loadTimeData.getString('type') !== 'SSL') {
    return;
  }

  // The titles are not internationalized because this is debugging information
  // for bug reports, help center posts, etc.
  appendDebuggingField('Subject', loadTimeData.getString('subject'));
  appendDebuggingField('Issuer', loadTimeData.getString('issuer'));
  appendDebuggingField('Expires on', loadTimeData.getString('expirationDate'));
  appendDebuggingField('Current date', loadTimeData.getString('currentDate'));
  appendDebuggingField('PEM encoded chain', loadTimeData.getString('pem'),
                       true);
  const ctInfo = loadTimeData.getString('ct');
  if (ctInfo) {
    appendDebuggingField('Certificate Transparency', ctInfo);
  }

  $('error-code').addEventListener('click', toggleDebuggingInfo);
  $('error-code').setAttribute('role', 'button');
  $('error-code').setAttribute('aria-expanded', false);
}
