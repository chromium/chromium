// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {readPasswordAttributes} from 'chrome://chrome-signin/gaia_auth_host/saml_password_attributes.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

let xmlTestData;
let xmlTestDataWithAttributesRemoved;

function initializeTestData(xmlTestDataInput) {
  xmlTestData = xmlTestDataInput;
  // Strips out the timestamps that are in the <AttributeValue> tags.
  xmlTestDataWithAttributesRemoved =
      xmlTestData.replace(/<AttributeValue>[^<>]+/g, '<AttributeValue>');
}


suite('SamlPasswordAttributesSuite', function() {
  // Fetch the SAML XML test file and make it available for the tests.
  setup(function(done) {
    const xmlTestDataUrl =
        'chrome://webui-test/gaia_auth_host/saml_with_password_attributes.xml';
    const xhr = new XMLHttpRequest();
    xhr.responseType = 'text';
    xhr.onreadystatechange = function() {
      if (xhr.readyState === 4 /* DONE */) {
        assertEquals(200, xhr.status);
        initializeTestData(xhr.response);
        done();
      }
    };
    xhr.open('GET', xmlTestDataUrl, true);
    xhr.send();
  });

  test('ReadInvalid', () => {
    // Make sure empty result is returned for empty input:
    let result = readPasswordAttributes('');
    assertEquals('', result.modifiedTime);
    assertEquals('', result.expirationTime);
    assertEquals('', result.passwordChangeUrl);

    // Make sure empty result is returned for random junk:
    result = readPasswordAttributes('<abc></abc>');
    assertEquals('', result.modifiedTime);
    assertEquals('', result.expirationTime);
    assertEquals('', result.passwordChangeUrl);

    // Make sure empty result is returned when the input is almost valid, but
    // not quite:
    result = readPasswordAttributes(xmlTestDataWithAttributesRemoved);
    assertEquals('', result.modifiedTime);
    assertEquals('', result.expirationTime);
    assertEquals('', result.passwordChangeUrl);
  });

  test('ReadValid', () => {
    const result = readPasswordAttributes(xmlTestData);

    assertEquals(
        String(Date.parse('2019-02-22T11:50:58.421Z')), result.modifiedTime);
    assertEquals(
        String(Date.parse('2019-03-06T11:50:58.421Z')), result.expirationTime);
    assertEquals(
        'https://example.com/adfs/portal/updatepassword/',
        result.passwordChangeUrl);
  });
});
