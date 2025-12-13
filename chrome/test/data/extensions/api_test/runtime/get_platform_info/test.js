// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are found here:
//   https://developer.chrome.com/extensions/runtime#type-PlatformOs
let platformOsList =
    ['mac', 'win', 'android', 'cros', 'linux', 'openbsd'];
let platformArchList =
  ['arm', 'arm64', 'x86-32', 'x86-64', 'mips', 'mips64', 'riscv64'];
let platformNaclArchList = ['arm', 'x86-32', 'x86-64', 'mips', 'mips64'];

chrome.test.runTests([

  function testGetPlatformInfo() {
    chrome.test.getConfig(function(config) {
      chrome.runtime.getPlatformInfo(function(platformInfo) {
        // It would be possible for the C++ side of the test to assemble
        // the expected information and supplied it as a custom param, but
        // it should be enough to assert that the values are within those
        // expected.
        chrome.test.assertTrue(platformOsList.includes(platformInfo.os));
        chrome.test.assertTrue(platformArchList.includes(platformInfo.arch));

        if (config.customArg === 'NaCl Arch unavailable' ||
            platformInfo.os === chrome.runtime.PlatformOs.ANDROID ||
            platformInfo.arch === chrome.runtime.PlatformArch.RISCV64) {
          // Native Client never supported Android OS and RISC-V ISA.
          chrome.test.assertFalse('nacl_arch' in platformInfo);
        } else {
          chrome.test.assertTrue(
            platformNaclArchList.includes(platformInfo.nacl_arch));
        }
        chrome.test.succeed();
      });
    });
  },

]);
