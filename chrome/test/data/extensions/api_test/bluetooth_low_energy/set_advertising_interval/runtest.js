// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
    return true;
  }
  return false;
}

function failOnSuccess() {
  if (!chrome.runtime.lastError) {
    chrome.test.fail('lastError not set, operation succeeded.');
    return true;
  }
  return false;
}

// Normal correct input.
chrome.bluetoothLowEnergy.setAdvertisingInterval(100, 200, function() {
  if (failOnError())
    return;
  // Correct input still.
  chrome.bluetoothLowEnergy.setAdvertisingInterval(30, 30, function() {
    if (failOnError())
      return;
    // Incorrect input, out of bounds (min).
    chrome.bluetoothLowEnergy.setAdvertisingInterval(19, 30, function() {
      if (failOnSuccess())
        return;
      // Incorrect input, out of bounds (max).
      chrome.bluetoothLowEnergy.setAdvertisingInterval(100, 10241, function() {
        if (failOnSuccess())
          return;
        // Incorrect input, min > max.
        chrome.bluetoothLowEnergy.setAdvertisingInterval(100, 50, function() {
          if (failOnSuccess())
            return;
          // Correct input, min.
          chrome.bluetoothLowEnergy.setAdvertisingInterval(20, 50, function() {
            if (failOnError())
              return;
            // Correct input, max.
            chrome.bluetoothLowEnergy.setAdvertisingInterval(50, 10240,
                function() {
              if (failOnError())
                return;
              chrome.test.succeed()
            });
          });
        });
      });
    });
  });
});
