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

// Tests that array of available routines can be successfully
// requested from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestAvailableRoutines', async () => {
  const response = await chromeos.diagnostics.getAvailableRoutines();
  assertDeepEquals(response, [
    'battery-capacity',
    'battery-health',
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
    'battery-charge',
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

// Tests that runBatteryCapacityRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryCapacityRoutine', async () => {
      const response =
          await chromeos.diagnostics.runBatteryCapacityRoutine(3000, 4000);
      assertDeepEquals(response, {id: 123456789, status: 'ready'});
    });

// Tests that runBatteryHealthRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryHealthRoutine', async () => {
      const response =
          await chromeos.diagnostics.runBatteryHealthRoutine(10, 5);
      assertDeepEquals(response, {id: 123456789, status: 'ready'});
    });

// Tests that runSmartctlCheckRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunSmartctlCheckRoutine', async () => {
      const response = await chromeos.diagnostics.runSmartctlCheckRoutine();
      assertDeepEquals(response, {id: 123456789, status: 'ready'});
    });

// Tests that runAcPowerRoutine throws the correct error
// when invalid enum is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunAcPowerRoutineInvalidInput', async () => {
      let caughtError;
      try {
        await chromeos.diagnostics.runAcPowerRoutine('this-does-not-exist');
      } catch (error) {
        caughtError = error;
      }

      assertEquals(caughtError.name, 'TypeError');
      assertEquals(
          caughtError.message,
          `Diagnostic expected status \'this-does-not-exist\' is unknown.`);
    });

// Tests that runAcPowerRoutine returns the correct Object when one or two
// parameters are given as input.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunAcPowerRoutine', async () => {
  const response1 = await chromeos.diagnostics.runAcPowerRoutine('connected');
  assertDeepEquals(response1, {id: 123456789, status: 'ready'});

  const response2 =
      await chromeos.diagnostics.runAcPowerRoutine('connected', 'Mains');
  assertDeepEquals(response2, {id: 123456789, status: 'ready'});

  const response3 =
      await chromeos.diagnostics.runAcPowerRoutine('disconnected', 'Battery');
  assertDeepEquals(response3, {id: 123456789, status: 'ready'});
});

// Tests that runCpuCacheRoutine throws the correct error
// when invalid number is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunCpuCacheRoutineInvalidInput', async () => {
      let caughtError;
      try {
        await chromeos.diagnostics.runCpuCacheRoutine(0);
      } catch (error) {
        caughtError = error;
      }

      assertEquals(caughtError.name, 'RangeError');
      assertEquals(caughtError.message, `Parameter must be positive.`);
    });

// Tests that runCpuCacheRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunCpuCacheRoutine', async () => {
  const response = await chromeos.diagnostics.runCpuCacheRoutine(10);
  assertDeepEquals(response, {id: 123456789, status: 'ready'});
});

// Tests that runCpuStressRoutine throws the correct error when invalid number
// is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunCpuStressRoutineInvalidInput', async () => {
      let caughtError;
      try {
        await chromeos.diagnostics.runCpuStressRoutine(0);
      } catch (error) {
        caughtError = error;
      }

      assertEquals(caughtError.name, 'RangeError');
      assertEquals(caughtError.message, `Parameter must be positive.`);
    });

// Tests that runCpuStressRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunCpuStressRoutine', async () => {
  const response = await chromeos.diagnostics.runCpuStressRoutine(5);
  assertDeepEquals(response, {id: 123456789, status: 'ready'});
});

// Tests that runFloatingPointAccuracyRoutine throws the correct error when
// invalid number is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunFPAccuracyRoutineInvalidInput', async () => {
      let caughtError1;
      try {
        await chromeos.diagnostics.runFloatingPointAccuracyRoutine(0);
      } catch (error) {
        caughtError1 = error;
      }

      assertEquals(caughtError1.name, 'RangeError');
      assertEquals(caughtError1.message, `Parameter must be positive.`);

      let caughtError2;
      try {
        await chromeos.diagnostics.runFloatingPointAccuracyRoutine(-2147483648);
      } catch (error) {
        caughtError2 = error;
      }

      assertEquals(caughtError2.name, 'RangeError');
      assertEquals(caughtError2.message, `Parameter must be positive.`);
    });

// Tests that runFloatingPointAccuracyRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunFPAccuracyRoutine', async () => {
  const response =
      await chromeos.diagnostics.runFloatingPointAccuracyRoutine(5);
  assertDeepEquals(response, {id: 123456789, status: 'ready'});
});

// Tests that runNVMEWearLevelRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunNvmeWearLevelRoutine', async () => {
      const response = await chromeos.diagnostics.runNvmeWearLevelRoutine(25);
      assertDeepEquals(response, {id: 123456789, status: 'ready'});
    });

// Tests that runNvmeSelfTestRoutine throws the correct error when invalid enum
// is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunNvmeSelfTestRoutineInvalidInput',
    async () => {
      let caughtError;
      try {
        await chromeos.diagnostics.runNvmeSelfTestRoutine(
            'this-does-not-exist');
      } catch (error) {
        caughtError = error;
      }

      assertEquals(caughtError.name, 'TypeError');
      assertEquals(
          caughtError.message,
          `Diagnostic NVMe self test type \'this-does-not-exist\' is unknown.`);
    });

// Tests that runNvmeSelfTestRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunNvmeSelfTestRoutine', async () => {
      const response1 =
          await chromeos.diagnostics.runNvmeSelfTestRoutine('short-self-test');
      assertDeepEquals(response1, {id: 123456789, status: 'ready'});

      const response2 =
          await chromeos.diagnostics.runNvmeSelfTestRoutine('long-self-test');
      assertDeepEquals(response2, {id: 123456789, status: 'ready'});
    });

// Tests that runDiskReadRoutine throws the correct error when invalid enum
// is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunDiskReadRoutineInvalidInput', async () => {
      let caughtError1;
      try {
        await chromeos.diagnostics.runDiskReadRoutine(
            'this-does-not-exist', 10, 10);
      } catch (error) {
        caughtError1 = error;
      }

      assertEquals(caughtError1.name, 'TypeError');
      assertEquals(
          caughtError1.message,
          `Diagnostic disk read type \'this-does-not-exist\' is unknown.`);

      let caughtError2;
      try {
        await chromeos.diagnostics.runDiskReadRoutine('linear-read', 0, 10);
      } catch (error) {
        caughtError2 = error;
      }
      let caughtError3;
      try {
        await chromeos.diagnostics.runDiskReadRoutine(
            'random-read', -2147483648, 10);
      } catch (error) {
        caughtError3 = error;
      }

      assertEquals(caughtError3.name, 'RangeError');
      assertEquals(caughtError3.message, `Parameter must be positive.`);

      let caughtError4;
      try {
        await chromeos.diagnostics.runDiskReadRoutine(
            'random-read', 10, 987654321);
      } catch (error) {
        caughtError4 = error;
      }

      assertEquals(caughtError4.name, 'RangeError');
      assertEquals(
          caughtError4.message,
          `Diagnostic disk read routine does not allow file sizes greater ` +
              `than '10000'.`);
    });

// Tests that runDiskReadRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunDiskReadRoutine', async () => {
  const response1 =
      await chromeos.diagnostics.runDiskReadRoutine('linear-read', 12, 20);
  assertDeepEquals(response1, {id: 123456789, status: 'ready'});

  const response2 =
      await chromeos.diagnostics.runDiskReadRoutine('random-read', 20, 10);
  assertDeepEquals(response2, {id: 123456789, status: 'ready'});
});

// Tests that runPrimeSearchRoutine throws the correct error when invalid enum
// is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunPrimeSearchRoutineInvalidInput',
    async () => {
      let caughtError1;
      try {
        await chromeos.diagnostics.runPrimeSearchRoutine(0, 10);
      } catch (error) {
        caughtError1 = error;
      }

      assertEquals(caughtError1.name, 'RangeError');
      assertEquals(caughtError1.message, `Parameter must be positive.`);

      let caughtError2;
      try {
        await chromeos.diagnostics.runPrimeSearchRoutine(-2147483648, 128);
      } catch (error) {
        caughtError2 = error;
      }

      assertEquals(caughtError2.name, 'RangeError');
      assertEquals(caughtError2.message, `Parameter must be positive.`);
    });

// Tests that runPrimeSearchRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunPrimeSearchRoutine', async () => {
  const response =
      await chromeos.diagnostics.runPrimeSearchRoutine(12, 1110987654321);
  assertDeepEquals(response, {id: 123456789, status: 'ready'});
});

// Tests that runBatteryDischargeRoutine throws the correct error when invalid
// enum is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryDischargeRoutineInvalidInput',
    async () => {
      let caughtError1;
      try {
        await chromeos.diagnostics.runBatteryDischargeRoutine(0, 10);
      } catch (error) {
        caughtError1 = error;
      }

      assertEquals(caughtError1.name, 'RangeError');
      assertEquals(caughtError1.message, `Parameter must be positive.`);

      let caughtError2;
      try {
        await chromeos.diagnostics.runBatteryDischargeRoutine(-2147483648, 5);
      } catch (error) {
        caughtError2 = error;
      }

      assertEquals(caughtError2.name, 'RangeError');
      assertEquals(caughtError2.message, `Parameter must be positive.`);
    });

// Tests that runBatteryDischargeRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryDischargeRoutine', async () => {
      const response =
          await chromeos.diagnostics.runBatteryDischargeRoutine(12, 2);
      assertDeepEquals(response, {id: 123456789, status: 'ready'});
    });

// Tests that runBatteryChargeRoutine throws the correct error when invalid
// enum is passed as input.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryChargeRoutineInvalidInput',
    async () => {
      let caughtError1;
      try {
        await chromeos.diagnostics.runBatteryChargeRoutine(0, 23);
      } catch (error) {
        caughtError1 = error;
      }

      assertEquals(caughtError1.name, 'RangeError');
      assertEquals(caughtError1.message, `Parameter must be positive.`);

      let caughtError2;
      try {
        await chromeos.diagnostics.runBatteryChargeRoutine(-2147483648, 1);
      } catch (error) {
        caughtError2 = error;
      }

      assertEquals(caughtError2.name, 'RangeError');
      assertEquals(caughtError2.message, `Parameter must be positive.`);
    });

// Tests that runBatteryChargeRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryChargeRoutine', async () => {
      const response =
          await chromeos.diagnostics.runBatteryChargeRoutine(12, 5);
      assertDeepEquals(response, {id: 123456789, status: 'ready'});
    });

// Tests that addEventListener receives system lid events.
UNTRUSTED_TEST('UntrustedLidEventListener', async () => {
  await Promise.all([
    new Promise(
        (resolve) =>
            chromeos.telemetry.addEventListener('lid-closed', resolve)),
    new Promise(
        (resolve) =>
            chromeos.telemetry.addEventListener('lid-opened', resolve)),
  ]);
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

// Tests that TelemetryInfo can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfo', async () => {
  const response = await chromeos.telemetry.probeTelemetryInfo([
    'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
    'timezone', 'memory', 'backlight', 'fan', 'stateful-partition', 'bluetooth'
  ]);

  // Rounded down to the nearest 100MiB due to privacy requirement.
  const availableSpace =
      Math.floor(1125899906842624 / (100 * 1024 * 1024)) * (100 * 1024 * 1024);

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
    vpdResult: {vpdInfo: {skuNumber: 'sku-18'}},
    cpuResult: {
      cpuInfo: {
        numTotalThreads: 2147483759,
        architecture: 'Armv7l',
        physicalCpus: [
          {
            modelName: 'i9',
            logicalCpus: [
              {
                maxClockSpeedKhz: 2147494759,
                scalingMaxFrequencyKhz: 1073764046,
                scalingCurrentFrequencyKhz: 536904245,
                idleTimeMs: 0,
                cStates: [
                  {name: 'C1', timeInStateSinceLastBootUs: 1125899906875957},
                  {name: 'C2', timeInStateSinceLastBootUs: 1125899906877777}
                ]
              },
              {
                maxClockSpeedKhz: 1147494759,
                scalingMaxFrequencyKhz: 1063764046,
                scalingCurrentFrequencyKhz: 936904246,
                idleTimeMs: 0,
                cStates: []
              }
            ]
          },
          {modelName: 'i9-low-powered', logicalCpus: []}
        ]
      }
    },
    timezoneResult: {
      timezoneInfo: {
        posix: 'MST7MDT,M3.2.0,M11.1.0',
        region: 'America/Denver',
      }
    },
    memoryResult: {
      memoryInfo: {
        totalMemoryKib: 2147483648,
        freeMemoryKib: 2147573648,
        availableMemoryKib: 2147571148,
        pageFaultsSinceLastBoot: 2199971148
      }
    },
    backlightResult: {
      backlightInfo: [{
        path: '/sys/backlight',
        maxBrightness: 536880912,
        brightness: 436880912,
      }]
    },
    fanResult: {
      fanInfo: [{
        speedRpm: 999880912,
      }]
    },
    statefulPartitionResult: {
      partitionInfo: {
        availableSpace: availableSpace,
        totalSpace: 1125900006842624,
      }
    },
    bluetoothResult: {
      bluetoothAdapterInfo: [{
        name: 'hci0',
        address: 'ab:cd:ef:12:34:56',
        powered: true,
        numConnectedDevices: 4294967295
      }]
    }
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

// Tests that TelemetryInfo with errors can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfoWithErrors', async () => {
  const response = await chromeos.telemetry.probeTelemetryInfo([
    'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
    'timezone', 'memory', 'backlight', 'fan', 'stateful-partition', 'bluetooth'
  ]);

  assertDeepEquals(response, {
    batteryResult: {
      error: {
        type: 'file-read-error',
        msg: 'battery error',
      }
    },
    blockDeviceResult: {
      error: {
        type: 'parse-error',
        msg: 'block device error',
      }
    },
    vpdResult: {
      error: {
        type: 'system-utility-error',
        msg: 'vpd error',
      }
    },
    cpuResult: {
      error: {
        type: 'service-unavailable',
        msg: 'cpu error',
      }
    },
    timezoneResult: {
      error: {
        type: 'file-read-error',
        msg: 'timezone error',
      }
    },
    memoryResult: {
      error: {
        type: 'parse-error',
        msg: 'memory error',
      }
    },
    backlightResult: {
      error: {
        type: 'system-utility-error',
        msg: 'backlight error',
      }
    },
    fanResult: {
      error: {
        type: 'service-unavailable',
        msg: 'fan error',
      }
    },
    statefulPartitionResult: {
      error: {
        type: 'file-read-error',
        msg: 'partition error',
      }
    },
    bluetoothResult: {
      error: {
        type: 'parse-error',
        msg: 'bluetooth error',
      }
    }
  });
});
