// Copyright 2014 The Chromium Authors
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

  const errorCode = document.querySelector('#error-code');
  errorCode.addEventListener('click', toggleDebuggingInfo);
  errorCode.setAttribute('role', 'button');
  errorCode.setAttribute('aria-expanded', false);
}
