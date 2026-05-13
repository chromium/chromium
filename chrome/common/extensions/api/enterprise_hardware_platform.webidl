// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary HardwarePlatformInfo {
  required DOMString model;
  required DOMString manufacturer;
};

// Use the <code>chrome.enterprise.hardwarePlatform</code> API to get the
// manufacturer and model of the hardware platform where the browser runs.
// Note: This API is only available to extensions installed by enterprise
// policy.
interface HardwarePlatform {
  // Obtains the manufacturer and model for the hardware platform and, if
  // the extension is authorized, returns it via |callback|.
  // |Returns|: Returns a Promise which resolves with the hardware platform
  //   info.
  // |PromiseValue|: info
  static Promise<HardwarePlatformInfo> getHardwarePlatformInfo();
};

partial interface Enterprise {
  static attribute HardwarePlatform hardwarePlatform;
};

partial interface Browser {
  static attribute Enterprise enterprise;
};
