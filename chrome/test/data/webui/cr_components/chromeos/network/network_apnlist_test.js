// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/network/network_apnlist.m.js';

// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('NetworkNetworkApnlistTest', function() {
  /** @type {!NetworkApnlist|undefined} */
  let apnlist;

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  /**
   * Note that if |attachStr| is "attach", attach APN will occur. Otherwise,
   * it will not.
   * @param {String} attachStr Value of attach.
   */
  function setCustomApnListWithAttachValue(attachStr) {
    apnlist.managedProperties = Object.assign({}, apnlist.managedProperties, {
      typeProperties: {
        cellular: {
          apnList: {
            activeValue: [
              {
                accessPointName: 'Custom accessPointName',
                name: 'Custom Name',
              },
            ],
          },
          customApnList: [
            {
              accessPointName: 'Custom accessPointName',
              name: 'Custom Name',
              attach: attachStr,
            },
          ],
        },
      },
    });
    Polymer.dom.flush();
  }

  /**
   * @param {String} attachApnStr Expected value of the attach field.
   */
  function apnEventToPromise(attachApnStr) {
    return new Promise(function(resolve, reject) {
      apnlist.addEventListener('apn-change', function f(e) {
        assertEquals(e.detail.attach, attachApnStr);
        apnlist.removeEventListener('apn-change', f);
        resolve(e);
      });
    });
  }

  setup(function() {
    apnlist = document.createElement('network-apnlist');
    apnlist.managedProperties = {
      typeProperties: {
        cellular: {
          apnList: {
            activeValue: [
              {
                accessPointName: 'Access Point',
                name: 'AP-name',
              },
            ],
          },
          lastGoodApn: {
            accessPointName: 'Access Point',
            name: 'AP-name',
          },
        },
      },
    };
    document.body.appendChild(apnlist);
    Polymer.dom.flush();
  });

  test('Last good APN option', function() {
    assertTrue(!!apnlist);

    const selectEl = apnlist.$.selectApn;
    assertTrue(!!selectEl);
    assertEquals(2, selectEl.length);
    assertEquals(0, selectEl.selectedIndex);
    assertEquals('AP-name', selectEl.options.item(0).value);
    assertEquals('Other', selectEl.options.item(1).value);
  });

  test('Disabled UI state', async function() {
    const selectEl = apnlist.$.selectApn;

    // Select 'Other' option.
    selectEl.value = 'Other';
    selectEl.dispatchEvent(new Event('change'));
    await flushAsync();

    const propertyList = apnlist.$$('network-property-list-mojo');
    const button = apnlist.$$('#saveButton');

    assertFalse(selectEl.disabled);
    assertFalse(propertyList.disabled);
    assertFalse(button.disabled);

    apnlist.disabled = true;

    assertTrue(selectEl.disabled);
    assertTrue(propertyList.disabled);
    assertTrue(button.disabled);
  });

  test('Attach APN does not occur', async function() {
    setCustomApnListWithAttachValue('');
    const attachApnToggle =
        apnlist.$$('#otherApnProperties').querySelector('#attachApnControl');
    assertEquals(false, attachApnToggle.checked);

    // Expect that attach APN will not occur
    const expectedNoAttachEvent = apnEventToPromise('');
    apnlist.$$('#saveButton').click();
    await expectedNoAttachEvent;
  });

  test('Attach APN occurs', async function() {
    const attachApnStr = OncMojo.USE_ATTACH_APN_NAME;
    setCustomApnListWithAttachValue(attachApnStr);
    const attachApnToggle =
        apnlist.$$('#otherApnProperties').querySelector('#attachApnControl');
    assertEquals(true, attachApnToggle.checked);

    // Expect that attach APN will occur
    const expectedAttachEvent = apnEventToPromise(OncMojo.USE_ATTACH_APN_NAME);
    apnlist.$$('#saveButton').click();
    await expectedAttachEvent;
  });
});
