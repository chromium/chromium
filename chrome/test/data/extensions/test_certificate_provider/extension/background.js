// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function arrayBufferFromByteList(byteList) {
  return (new Uint8Array(byteList)).buffer;
}

function byteListFromArrayBuffer(arrayBuffer) {
  return Array.from(new Uint8Array(arrayBuffer));
}

// Returns the list of certificateProvider CertificateInfo instances, given the
// parsed JSON value received from the C++ handler.
function certInfoListFromParsedJson(parsedCertInfoList) {
  return parsedCertInfoList.map(parsedCertInfo => {
    const certInfo = Object.assign({}, parsedCertInfo);
    certInfo.certificate = arrayBufferFromByteList(parsedCertInfo.certificate);
    return certInfo;
  });
}

// Transforms the certificateProvider SignRequest instance into a JSON-ifiable
// value that may be sent to the C++ handler.
function jsonifiableFromSignRequest(signRequest) {
  const transformedSignRequest = Object.assign({}, signRequest);
  transformedSignRequest.digest = byteListFromArrayBuffer(signRequest.digest);
  transformedSignRequest.certificate =
      byteListFromArrayBuffer(signRequest.certificate);
  return transformedSignRequest;
}

chrome.certificateProvider.onCertificatesRequested.addListener(
    reportCallback => {
      // Forward the request to the C++ handler.
      chrome.test.sendMessage(
          JSON.stringify(['onCertificatesRequested']), response => {
            const certInfoList =
                certInfoListFromParsedJson(JSON.parse(response));
            reportCallback(certInfoList, rejectedCertificates => {
              if (rejectedCertificates && rejectedCertificates.length) {
                console.error(
                    'Rejected certificates: ' +
                    JSON.stringify(rejectedCertificates));
              }
            });
          });
    });

chrome.certificateProvider.onSignDigestRequested.addListener(
    (request, reportCallback) => {
      // Forward the request to the C++ handler.
      chrome.test.sendMessage(
          JSON.stringify(
              ['onSignDigestRequested', jsonifiableFromSignRequest(request)]),
          response => {
            const parsedResponse = JSON.parse(response);
            const signature = (parsedResponse === null) ?
                undefined :
                arrayBufferFromByteList(parsedResponse);
            reportCallback(signature);
          });
    });
