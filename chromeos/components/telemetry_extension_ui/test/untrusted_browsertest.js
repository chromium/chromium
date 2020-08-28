// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for chrome-untrusted://telemetry_extension. */

/**
 * This is used to create TrustedScriptURL.
 * @type {!TrustedTypePolicy}
 */
const workerUrlPolicy = trustedTypes.createPolicy(
    'telemetry-extension-static', {createScriptURL: () => 'worker.js'});

// Tests that web workers can be spawned from
// chrome-untrusted://telemetry_extension.
UNTRUSTED_TEST('UntrustedCanSpawnWorkers', async () => {
  if (!window.Worker) {
    throw 'Worker is not supported!';
  }

  // createScriptURL() always returns a 'untrusted_workjer.js' TrustedScriptURL,
  // so pass an empty string. In the future we might be able to avoid the empty
  // string if https://github.com/w3c/webappsec-trusted-types/issues/278 gets
  // fixed.
  /**
   * Closure Compiler only support string type as an argument to Worker
   * @suppress {checkTypes}
   */
  const worker = new Worker(workerUrlPolicy.createScriptURL(''));

  const workerResponse = new Promise((resolve, reject) => {
    /**
     * Registers onmessage event handler.
     * @param {MessageEvent} event Incoming message event.
     */
    worker.onmessage = function(event) {
      const data = /** @type {string} */ (event.data);
      resolve(data);
    };
    worker.onerror = function() {
      reject('There is an error with your worker');
    };
  });

  const MESSAGE = 'ping/pong message';

  worker.postMessage(MESSAGE);

  const response = /** @type {string} */ (await workerResponse);
  assertEquals(response, MESSAGE);
});

// Tests that TelemetryInfo throws an error if category is unknown.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfoUnknownCategory', async () => {
  let caughtError = {};

  try {
    await chromeos.telemetry.probeTelemetryInfo(['unknown-category']);
  } catch (error) {
    caughtError = error;
  }

  assertEquals(caughtError.name, 'TypeError');
  assertEquals(
      caughtError.message,
      'Telemetry category \'unknown-category\' is unknown.');
});

// Tests that array of available routines can be successfully
// requested from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestAvailableRoutines', async () => {
  const response = await chromeos.diagnostics.getAvailableRoutines();
  assertDeepEquals(response, [
    'battery-capacity',
    'battery-health',
    'urandom',
    'smartctl-check',
    'ac-power',
    'cpu-cache',
    'cpu-stress',
    'floating-point-accuracy',
    'nvme-wear-level',
    'nvme-self-test',
    'disk-read',
    'prime-search',
    'battery-discharge',
  ]);
});

// Tests that sendCommandToRoutine throws the correct errors
// when unknown routines or commands are passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRoutineUpdateUnknownArguments', async () => {
      let caughtError;
      try {
        await chromeos.diagnostics.sendCommandToRoutine(
            9007199254740991, 'remove', true);
      } catch (error) {
        caughtError = error;
      }

      assertEquals(caughtError.name, 'RangeError');
      assertEquals(
          caughtError.message,
          `Diagnostic routine id '9007199254740991' is out of int32 range.`);

      try {
        await chromeos.diagnostics.sendCommandToRoutine(
            -9007199254740991, 'remove', true);
      } catch (error) {
        caughtError = error;
      }

      assertEquals(caughtError.name, 'RangeError');
      assertEquals(
          caughtError.message,
          `Diagnostic routine id '-9007199254740991' is out of int32 range.`);

      try {
        await chromeos.diagnostics.sendCommandToRoutine(
            123456789, 'this-command-must-not-exist', true);
      } catch (error) {
        caughtError = error;
      }

      assertEquals(caughtError.name, 'TypeError');
      assertEquals(
          caughtError.message,
          `Diagnostic command \'this-command-must-not-exist\' is unknown.`);
    });

// Tests that TelemetryInfo can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfo', async () => {
  const response = await chromeos.telemetry.probeTelemetryInfo([
    'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
    'timezone', 'memory', 'backlight', 'fan', 'stateful-partition', 'bluetooth'
  ]);

  assertDeepEquals(response, {
    batteryResult: {
      batteryInfo: {
        cycleCount: 100000000000000,
        voltageNow: 1234567890.123456,
        vendor: 'Google',
        serialNumber: 'abcdef',
        chargeFullDesign: 3000000000000000,
        chargeFull: 9000000000000000,
        voltageMinDesign: 1000000000.1001,
        modelName: 'Google Battery',
        chargeNow: 7777777777.777,
        currentNow: 0.9999999999999,
        technology: 'Li-ion',
        status: 'Charging',
        manufactureDate: '2020-07-30',
        temperature: 7777777777777777,
      }
    },
    blockDeviceResult: {
      blockDeviceInfo: [{
        path: '/dev/device1',
        size: 5555555555555555,
        type: 'NVMe',
        manufacturerId: 200,
        name: 'goog',
        serial: '4287654321',
        bytesReadSinceLastBoot: 9000000000000000,
        bytesWrittenSinceLastBoot: 8000000000000000,
        readTimeSecondsSinceLastBoot: 7000000000000000,
        writeTimeSecondsSinceLastBoot: 6666666666666666,
        ioTimeSecondsSinceLastBoot: 1111111111111,
        discardTimeSecondsSinceLastBoot: 77777777777777
      }]
    },
    vpdResult: {vpdInfo: {skuNumber: 'sku-18'}}
  });
});

// Tests that sendCommandToRoutine returns the correct Object
// for an interactive routine.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestInteractiveRoutineUpdate', async () => {
      const response = await chromeos.diagnostics.sendCommandToRoutine(
          987654321, 'remove', true);
      assertDeepEquals(response, {
        progressPercent: 0,
        output: 'This routine is running!',
        routineUpdateUnion:
            {interactiveUpdate: {userMessage: 'unplug-ac-power'}}
      });
    });

// Tests that sendCommandToRoutine returns the correct Object
// for a non-interactive routine.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestNonInteractiveRoutineUpdate', async () => {
      const response = await chromeos.diagnostics.sendCommandToRoutine(
          135797531, 'remove', true);
      assertDeepEquals(response, {
        progressPercent: 3147483771,
        output: '',
        routineUpdateUnion: {
          noninteractiveUpdate:
              {status: 'ready', statusMessage: 'Routine ran by Google.'}
        }
      });
    });

// Tests that TelemetryInfo can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfoWithInterceptor', async () => {
  const response = await chromeos.telemetry.probeTelemetryInfo([
    'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
    'timezone', 'memory', 'backlight', 'fan', 'stateful-partition', 'bluetooth'
  ]);
  assertDeepEquals(response, {});
});
