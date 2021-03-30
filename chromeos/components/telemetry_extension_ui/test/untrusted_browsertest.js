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
  assertEquals(MESSAGE, response);
});

// Tests that array of available routines can be successfully
// requested from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestAvailableRoutines', async () => {
  const expectedResult = [
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
  ];

  const chromeosResponse = await chromeos.diagnostics.getAvailableRoutines();
  assertDeepEquals(expectedResult, chromeosResponse);

  const dpslResponse = await dpsl.diagnostics.getAvailableRoutines();
  assertDeepEquals(expectedResult, dpslResponse);
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

      assertEquals('RangeError', caughtError.name);
      assertEquals(
          `Diagnostic routine id '9007199254740991' is out of int32 range.`,
          caughtError.message);

      try {
        await chromeos.diagnostics.sendCommandToRoutine(
            -9007199254740991, 'remove', true);
      } catch (error) {
        caughtError = error;
      }

      assertEquals('RangeError', caughtError.name);
      assertEquals(
          `Diagnostic routine id '-9007199254740991' is out of int32 range.`,
          caughtError.message);

      try {
        await chromeos.diagnostics.sendCommandToRoutine(
            123456789, 'this-command-must-not-exist', true);
      } catch (error) {
        caughtError = error;
      }

      assertEquals('TypeError', caughtError.name);
      assertEquals(
          `Diagnostic command \'this-command-must-not-exist\' is unknown.`,
          caughtError.message);
    });

// Tests that runBatteryCapacityRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryCapacityRoutine', async () => {
      const response =
        await chromeos.diagnostics.runBatteryCapacityRoutine();
      assertDeepEquals({id: 123456789, status: 'ready'}, response);
    });

// Tests that a routine is created and routine.{getStatus(), resume(), stop()}
// return correct responses.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRoutineCommandWithInterceptor', async () => {
      let expectedRoutineStatus = {
        progressPercent: 0,
        output: '',
        status: 'ready',
        statusMessage: 'Routine ran by Google.',
        userMessage: ''
      };

      // The order must be kept.
      // See UntrustedDiagnosticsRoutineCommandWithInterceptor test in
      // telemetry_extension_ui_browsertests.js.

      const routine = await dpsl.diagnostics.battery.runCapacityRoutine();
      const routineGetStatus = await routine.getStatus();
      const routineResume = await routine.resume();
      const routineStop = await routine.stop();

      assertDeepEquals(expectedRoutineStatus, routineGetStatus);
      assertDeepEquals(expectedRoutineStatus, routineResume);
      assertDeepEquals(expectedRoutineStatus, routineStop);
    });

// Tests that diagnostics routines are successfully created and run.
UNTRUSTED_TEST('UntrustedDiagnosticsRunRoutineWithInterceptor', async () => {
  // The order must be kept.
  // See UntrustedDiagnosticsRunRoutineWithInterceptor test in
  // telemetry_extension_ui_browsertests.js.

  // dpsl.diagnostics.battery.* routines
  await dpsl.diagnostics.battery.runCapacityRoutine();
  await dpsl.diagnostics.battery.runHealthRoutine();
  await dpsl.diagnostics.battery.runDischargeRoutine(
      {lengthSeconds: 7, maximumDischargePercentAllowed: 50});
  await dpsl.diagnostics.battery.runChargeRoutine(
      {lengthSeconds: 13, minimumChargePercentRequired: 87});

  // dpsl.diangostics.nvme.* routines
  await dpsl.diagnostics.nvme.runSmartctlCheckRoutine();
  await dpsl.diagnostics.nvme.runWearLevelRoutine({wearLevelThreshold: 37});
  await dpsl.diagnostics.nvme.runShortSelfTestRoutine();
  await dpsl.diagnostics.nvme.runLongSelfTestRoutine();

  // dpsl.diangostics.power.* routines
  await dpsl.diagnostics.power.runAcConnectedRoutine();
  await dpsl.diagnostics.power.runAcDisconnectedRoutine();
  await dpsl.diagnostics.power.runAcConnectedRoutine(
      {expectedPowerType: 'Mains'});
  await dpsl.diagnostics.power.runAcDisconnectedRoutine(
      {expectedPowerType: 'Battery'});

  // dpsl.diangostics.cpu.* tests
  await dpsl.diagnostics.cpu.runCacheRoutine({duration: 30});
  await dpsl.diagnostics.cpu.runStressRoutine({duration: 17});
  await dpsl.diagnostics.cpu.runFloatingPointAccuracyRoutine({duration: 94});
  await dpsl.diagnostics.cpu.runPrimeSearchRoutine(
      {lengthSeconds: 45, maximumNumber: 1110987654321});

  // dpsl.diangostics.disk.* tests
  await dpsl.diagnostics.disk.runLinearReadRoutine(
      {lengthSeconds: 44, fileSizeMB: 135});
  await dpsl.diagnostics.disk.runRandomReadRoutine(
      {lengthSeconds: 23, fileSizeMB: 1749});
});

// Tests that runBatteryHealthRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryHealthRoutine', async () => {
      const response =
        await chromeos.diagnostics.runBatteryHealthRoutine();
      assertDeepEquals({id: 123456789, status: 'ready'}, response);
    });

// Tests that runSmartctlCheckRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunSmartctlCheckRoutine', async () => {
      const response = await chromeos.diagnostics.runSmartctlCheckRoutine();
      assertDeepEquals({id: 123456789, status: 'ready'}, response);
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

      assertEquals('TypeError', caughtError.name);
      assertEquals(
          `Diagnostic expected status \'this-does-not-exist\' is unknown.`,
          caughtError.message);
    });

// Tests that runAcPowerRoutine returns the correct Object when one or two
// parameters are given as input.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunAcPowerRoutine', async () => {
  const response1 = await chromeos.diagnostics.runAcPowerRoutine('connected');
  assertDeepEquals({id: 123456789, status: 'ready'}, response1);

  const response2 =
      await chromeos.diagnostics.runAcPowerRoutine('connected', 'Mains');
  assertDeepEquals({id: 123456789, status: 'ready'}, response2);

  const response3 =
      await chromeos.diagnostics.runAcPowerRoutine('disconnected', 'Battery');
  assertDeepEquals({id: 123456789, status: 'ready'}, response3);
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

      assertEquals('RangeError', caughtError.name);
      assertEquals(`Parameter must be positive.`, caughtError.message);
    });

// Tests that runCpuCacheRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunCpuCacheRoutine', async () => {
  const response = await chromeos.diagnostics.runCpuCacheRoutine(10);
  assertDeepEquals({id: 123456789, status: 'ready'}, response);
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

      assertEquals('RangeError', caughtError.name);
      assertEquals(`Parameter must be positive.`, caughtError.message);
    });

// Tests that runCpuStressRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunCpuStressRoutine', async () => {
  const response = await chromeos.diagnostics.runCpuStressRoutine(5);
  assertDeepEquals({id: 123456789, status: 'ready'}, response);
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

      assertEquals('RangeError', caughtError1.name);
      assertEquals(`Parameter must be positive.`, caughtError1.message);

      let caughtError2;
      try {
        await chromeos.diagnostics.runFloatingPointAccuracyRoutine(-2147483648);
      } catch (error) {
        caughtError2 = error;
      }

      assertEquals('RangeError', caughtError2.name);
      assertEquals(`Parameter must be positive.`, caughtError2.message);
    });

// Tests that runFloatingPointAccuracyRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunFPAccuracyRoutine', async () => {
  const response =
      await chromeos.diagnostics.runFloatingPointAccuracyRoutine(5);
  assertDeepEquals({id: 123456789, status: 'ready'}, response);
});

// Tests that runNVMEWearLevelRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunNvmeWearLevelRoutine', async () => {
      const response = await chromeos.diagnostics.runNvmeWearLevelRoutine(25);
      assertDeepEquals({id: 123456789, status: 'ready'}, response);
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

      assertEquals('TypeError', caughtError.name);
      assertEquals(
          `Diagnostic NVMe self test type \'this-does-not-exist\' is unknown.`,
          caughtError.message);
    });

// Tests that runNvmeSelfTestRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunNvmeSelfTestRoutine', async () => {
      const response1 =
          await chromeos.diagnostics.runNvmeSelfTestRoutine('short-self-test');
      assertDeepEquals({id: 123456789, status: 'ready'}, response1);

      const response2 =
          await chromeos.diagnostics.runNvmeSelfTestRoutine('long-self-test');
      assertDeepEquals({id: 123456789, status: 'ready'}, response2);
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

      assertEquals('TypeError', caughtError1.name);
      assertEquals(
          `Diagnostic disk read type \'this-does-not-exist\' is unknown.`,
          caughtError1.message);

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

      assertEquals('RangeError', caughtError3.name);
      assertEquals(`Parameter must be positive.`, caughtError3.message);

      let caughtError4;
      try {
        await chromeos.diagnostics.runDiskReadRoutine(
            'random-read', 10, 987654321);
      } catch (error) {
        caughtError4 = error;
      }

      assertEquals('RangeError', caughtError4.name);
      assertEquals(
          `Diagnostic disk read routine does not allow file sizes greater ` +
              `than '10000'.`,
          caughtError4.message);
    });

// Tests that runDiskReadRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunDiskReadRoutine', async () => {
  const response1 =
      await chromeos.diagnostics.runDiskReadRoutine('linear-read', 12, 20);
  assertDeepEquals({id: 123456789, status: 'ready'}, response1);

  const response2 =
      await chromeos.diagnostics.runDiskReadRoutine('random-read', 20, 10);
  assertDeepEquals({id: 123456789, status: 'ready'}, response2);
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

      assertEquals('RangeError', caughtError1.name);
      assertEquals(`Parameter must be positive.`, caughtError1.message);

      let caughtError2;
      try {
        await chromeos.diagnostics.runPrimeSearchRoutine(-2147483648, 128);
      } catch (error) {
        caughtError2 = error;
      }

      assertEquals('RangeError', caughtError2.name);
      assertEquals(`Parameter must be positive.`, caughtError2.message);
    });

// Tests that runPrimeSearchRoutine returns the correct Object.
UNTRUSTED_TEST('UntrustedDiagnosticsRequestRunPrimeSearchRoutine', async () => {
  const response =
      await chromeos.diagnostics.runPrimeSearchRoutine(12, 1110987654321);
  assertDeepEquals({id: 123456789, status: 'ready'}, response);
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

      assertEquals('RangeError', caughtError1.name);
      assertEquals(`Parameter must be positive.`, caughtError1.message);

      let caughtError2;
      try {
        await chromeos.diagnostics.runBatteryDischargeRoutine(-2147483648, 5);
      } catch (error) {
        caughtError2 = error;
      }

      assertEquals('RangeError', caughtError2.name);
      assertEquals(`Parameter must be positive.`, caughtError2.message);
    });

// Tests that runBatteryDischargeRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryDischargeRoutine', async () => {
      const response =
          await chromeos.diagnostics.runBatteryDischargeRoutine(12, 2);
      assertDeepEquals({id: 123456789, status: 'ready'}, response);
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

      assertEquals('RangeError', caughtError1.name);
      assertEquals(`Parameter must be positive.`, caughtError1.message);

      let caughtError2;
      try {
        await chromeos.diagnostics.runBatteryChargeRoutine(-2147483648, 1);
      } catch (error) {
        caughtError2 = error;
      }

      assertEquals('RangeError', caughtError2.name);
      assertEquals(`Parameter must be positive.`, caughtError2.message);
    });

// Tests that runBatteryChargeRoutine returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsRequestRunBatteryChargeRoutine', async () => {
      const response =
          await chromeos.diagnostics.runBatteryChargeRoutine(12, 5);
      assertDeepEquals({id: 123456789, status: 'ready'}, response);
    });

// Tests that:
//   1) addEventListener receives system bluetooth adapter added event.
//   2) removeEventListener stops receiving system bluetooth adapter added
//      event.
UNTRUSTED_TEST('UntrustedBluetoothAdapterAddedEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener(
          'bluetooth-adapter-added', resolve));

  dpsl.system_events.bluetooth.addOnAdapterAddedListener(
      shouldNeverBeCalledCallback);
  dpsl.system_events.bluetooth.removeOnAdapterAddedListener(
      shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) =>
          dpsl.system_events.bluetooth.addOnAdapterAddedListener(resolve));
});

// Tests that:
//   1) addEventListener receives system bluetooth adapter removed event.
//   2) removeEventListener stops receiving system bluetooth adapter removed
//      event.
UNTRUSTED_TEST('UntrustedBluetoothAdapterRemovedEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener(
          'bluetooth-adapter-removed', resolve));

  dpsl.system_events.bluetooth.addOnAdapterRemovedListener(
      shouldNeverBeCalledCallback);
  dpsl.system_events.bluetooth.removeOnAdapterRemovedListener(
      shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) =>
          dpsl.system_events.bluetooth.addOnAdapterRemovedListener(resolve));
});

// Tests that:
//   1) addEventListener receives system bluetooth adapter property changed
//      event.
//   2) removeEventListener stops receiving system bluetooth adapter property
//      changed event.
UNTRUSTED_TEST(
    'UntrustedBluetoothAdapterPropertyChangedEventListener', async () => {
      await new Promise(
          (resolve) => chromeos.telemetry.addEventListener(
              'bluetooth-adapter-property-changed', resolve));

      dpsl.system_events.bluetooth.addOnAdapterPropertyChangedListener(
          shouldNeverBeCalledCallback);
      dpsl.system_events.bluetooth.removeOnAdapterPropertyChangedListener(
          shouldNeverBeCalledCallback);

      await new Promise(
          (resolve) =>
              dpsl.system_events.bluetooth.addOnAdapterPropertyChangedListener(
                  resolve));
    });

// Tests that:
//   1) addEventListener receives system bluetooth device added event.
//   2) removeEventListener stops receiving system bluetooth device added event.
UNTRUSTED_TEST('UntrustedBluetoothDeviceAddedEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener(
          'bluetooth-device-added', resolve));

  dpsl.system_events.bluetooth.addOnDeviceAddedListener(
      shouldNeverBeCalledCallback);
  dpsl.system_events.bluetooth.removeOnDeviceAddedListener(
      shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) =>
          dpsl.system_events.bluetooth.addOnDeviceAddedListener(resolve));
});

// Tests that:
//   1) addEventListener receives system bluetooth device removed event.
//   2) removeEventListener stops receiving system bluetooth device removed
//      event.
UNTRUSTED_TEST('UntrustedBluetoothDeviceRemovedEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener(
          'bluetooth-device-removed', resolve));

  dpsl.system_events.bluetooth.addOnDeviceRemovedListener(
      shouldNeverBeCalledCallback);
  dpsl.system_events.bluetooth.removeOnDeviceRemovedListener(
      shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) =>
          dpsl.system_events.bluetooth.addOnDeviceRemovedListener(resolve));
});

// Tests that:
//   1) addEventListener receives system bluetooth device property changed
//      event.
//   2) removeEventListener stops receiving system bluetooth device property
//      changed event.
UNTRUSTED_TEST(
    'UntrustedBluetoothDevicePropertyChangedEventListener', async () => {
      await new Promise(
          (resolve) => chromeos.telemetry.addEventListener(
              'bluetooth-device-property-changed', resolve));

      dpsl.system_events.bluetooth.addOnDevicePropertyChangedListener(
          shouldNeverBeCalledCallback);
      dpsl.system_events.bluetooth.removeOnDevicePropertyChangedListener(
          shouldNeverBeCalledCallback);

      await new Promise(
          (resolve) =>
              dpsl.system_events.bluetooth.addOnDevicePropertyChangedListener(
                  resolve));
    });

// Tests that:
//   1) addEventListener receives system lid closed event.
//   2) removeEventListener stops receiving system lid closed event.
UNTRUSTED_TEST('UntrustedLidClosedEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener('lid-closed', resolve));

  dpsl.system_events.lid.addOnLidClosedListener(shouldNeverBeCalledCallback);
  dpsl.system_events.lid.removeOnLidClosedListener(shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) => dpsl.system_events.lid.addOnLidClosedListener(resolve));
});

// Tests that:
//   1) addEventListener receives system lid opened event.
//   2) removeEventListener stops receiving system lid opened event.
UNTRUSTED_TEST('UntrustedLidOpenedEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener('lid-opened', resolve));

  dpsl.system_events.lid.addOnLidOpenedListener(shouldNeverBeCalledCallback);
  dpsl.system_events.lid.removeOnLidOpenedListener(shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) => dpsl.system_events.lid.addOnLidOpenedListener(resolve));
});

// Tests that:
//   1) addEventListener receives system ac inserted event.
//   2) removeEventListener stops receiving system ac inserted event.
UNTRUSTED_TEST('UntrustedAcInsertedEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener('ac-inserted', resolve));

  dpsl.system_events.power.addOnAcInsertedListener(shouldNeverBeCalledCallback);
  dpsl.system_events.power.removeOnAcInsertedListener(
      shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) => dpsl.system_events.power.addOnAcInsertedListener(resolve));
});

// Tests that:
//   1) addEventListener receives system ac removed event.
//   2) removeEventListener stops receiving system ac removed event.
UNTRUSTED_TEST('UntrustedAcRemovedEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener('ac-removed', resolve));

  dpsl.system_events.power.addOnAcRemovedListener(shouldNeverBeCalledCallback);
  dpsl.system_events.power.removeOnAcRemovedListener(
      shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) => dpsl.system_events.power.addOnAcRemovedListener(resolve));
});

// Tests that:
//   1) addEventListener receives system os suspend event.
//   2) removeEventListener stops receiving system os suspend event.
UNTRUSTED_TEST('UntrustedOsSuspendEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener('os-suspend', resolve));

  dpsl.system_events.power.addOnOsSuspendListener(shouldNeverBeCalledCallback);
  dpsl.system_events.power.removeOnOsSuspendListener(
      shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) => dpsl.system_events.power.addOnOsSuspendListener(resolve));
});

// Tests that:
//   1) addEventListener receives system os resume event.
//   2) removeEventListener stops receiving system os resume event.
UNTRUSTED_TEST('UntrustedOsResumeEventListener', async () => {
  await new Promise(
      (resolve) => chromeos.telemetry.addEventListener('os-resume', resolve));

  dpsl.system_events.power.addOnOsResumeListener(shouldNeverBeCalledCallback);
  dpsl.system_events.power.removeOnOsResumeListener(
      shouldNeverBeCalledCallback);

  await new Promise(
      (resolve) => dpsl.system_events.power.addOnOsResumeListener(resolve));
});

// Tests that TelemetryInfo throws an error if category is unknown.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfoUnknownCategory', async () => {
  let caughtError = {};

  try {
    await chromeos.telemetry.probeTelemetryInfo(['unknown-category']);
  } catch (error) {
    caughtError = error;
  }

  assertEquals('TypeError', caughtError.name);
  assertEquals(
      'Telemetry category \'unknown-category\' is unknown.',
      caughtError.message);
});

// Tests that TelemetryInfo can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfo', async () => {
  // Rounded down to the nearest 100MiB due to privacy requirement.
  const availableSpace = BigInt(
      Math.floor(1125899906842624 / (100 * 1024 * 1024)) * (100 * 1024 * 1024));

  const expectedResult = {
    batteryResult: {
      batteryInfo: {
        cycleCount: BigInt(100000000000000),
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
        temperature: BigInt(7777777777777777),
      }
    },
    blockDeviceResult: {
      blockDeviceInfo: [{
        path: '/dev/device1',
        size: BigInt(5555555555555555),
        type: 'NVMe',
        manufacturerId: 200,
        name: 'goog',
        serial: '4287654321',
        bytesReadSinceLastBoot: BigInt(9000000000000000),
        bytesWrittenSinceLastBoot: BigInt(8000000000000000),
        readTimeSecondsSinceLastBoot: BigInt(7000000000000000),
        writeTimeSecondsSinceLastBoot: BigInt(6666666666666666),
        ioTimeSecondsSinceLastBoot: BigInt(1111111111111),
        discardTimeSecondsSinceLastBoot: BigInt(77777777777777)
      }]
    },
    vpdResult: {
      vpdInfo: {
        skuNumber: 'sku-18',
        serialNumber: '5CD9132880',
        modelName: 'XX ModelName 007 XY'
      }
    },
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
                idleTimeMs: BigInt(0),
                cStates: [
                  {
                    name: 'C1',
                    timeInStateSinceLastBootUs: BigInt(1125899906875957)
                  },
                  {
                    name: 'C2',
                    timeInStateSinceLastBootUs: BigInt(1125899906877777)
                  }
                ]
              },
              {
                maxClockSpeedKhz: 1147494759,
                scalingMaxFrequencyKhz: 1063764046,
                scalingCurrentFrequencyKhz: 936904246,
                idleTimeMs: BigInt(0),
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
        pageFaultsSinceLastBoot: BigInt(2199971148),
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
        totalSpace: BigInt(1125900006842624),
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
  };

  await Promise
      .all([
        chromeos.telemetry.probeTelemetryInfo([
          'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
          'timezone', 'memory', 'backlight', 'fan', 'stateful-partition',
          'bluetooth'
        ]),
        dpsl.telemetry.getBatteryInfo(),
        dpsl.telemetry.getNonRemovableBlockDevicesInfo(),
        dpsl.telemetry.getCachedVpdInfo(), dpsl.telemetry.getCpuInfo(),
        dpsl.telemetry.getTimezoneInfo(), dpsl.telemetry.getMemoryInfo(),
        dpsl.telemetry.getBacklightInfo(), dpsl.telemetry.getFanInfo(),
        dpsl.telemetry.getStatefulPartitionInfo(),
        dpsl.telemetry.getBluetoothInfo()
      ])
      .then((values) => {
        assertDeepEquals(
            [
              expectedResult, expectedResult.batteryResult.batteryInfo,
              expectedResult.blockDeviceResult.blockDeviceInfo,
              expectedResult.vpdResult.vpdInfo,
              expectedResult.cpuResult.cpuInfo,
              expectedResult.timezoneResult.timezoneInfo,
              expectedResult.memoryResult.memoryInfo,
              expectedResult.backlightResult.backlightInfo,
              expectedResult.fanResult.fanInfo,
              expectedResult.statefulPartitionResult.partitionInfo,
              expectedResult.bluetoothResult.bluetoothAdapterInfo
            ],
            values);
      });
});

// Tests that sendCommandToRoutine returns the correct Object
// for an interactive routine and that interactive routine.getStatus()
// returns the correct Object.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsInteractiveRoutineCommand', async () => {
      const expectedResult = {
        progressPercent: 0,
        output: 'This routine is running!',
        status: 'waiting',
        statusMessage: '',
        userMessage: 'unplug-ac-power'
      };
      const response = await chromeos.diagnostics.sendCommandToRoutine(
          987654321, 'remove', true);
      assertDeepEquals(
          {
            progressPercent: expectedResult.progressPercent,
            output: expectedResult.output,
            routineUpdateUnion:
                {interactiveUpdate: {userMessage: expectedResult.userMessage}}
          },
          response);
      const dpslRoutine = await dpsl.diagnostics.power.runAcConnectedRoutine();
      const routineStatus = await dpslRoutine.getStatus();
      assertDeepEquals(expectedResult, routineStatus);
    });

// Tests that sendCommandToRoutine returns the correct Object
// for a non-interactive routine and that a non-interactive routine.getStatus()
// returns the correct Object.
// Note: this is an end-to-end test using fake cros_healthd.
UNTRUSTED_TEST(
    'UntrustedDiagnosticsNonInteractiveRoutineCommand', async () => {
      const expectedResult = {
        progressPercent: 3147483771,
        output: '',
        status: 'ready',
        statusMessage: 'Routine ran by Google.',
        userMessage: ''
      }
      const response = await chromeos.diagnostics.sendCommandToRoutine(
          135797531, 'remove', true);
      assertDeepEquals(
          {
            progressPercent: expectedResult.progressPercent,
            output: expectedResult.output,
            routineUpdateUnion: {
              noninteractiveUpdate: {
                status: expectedResult.status,
                statusMessage: expectedResult.statusMessage
              }
            }
          },
          response);
      const dpslRoutine = await dpsl.diagnostics.battery.runCapacityRoutine();
      const routineStatus = await dpslRoutine.getStatus();
      assertDeepEquals(expectedResult, routineStatus);
});

// Tests that TelemetryInfo can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfoWithInterceptor', async () => {
  const probeTelemetryResponse = await chromeos.telemetry.probeTelemetryInfo([
    'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
    'timezone', 'memory', 'backlight', 'fan', 'stateful-partition', 'bluetooth'
  ]);
  assertDeepEquals({}, probeTelemetryResponse);

  const expectedResult = JSON.stringify(
      {type: 'no-result-error', msg: 'Backend returned no result'});

  // The order must be kept.
  // See UntrustedRequestTelemetryInfoWithInterceptor test in
  // telemetry_extension_ui_browsertests.js.
  await Promise.all([
    verifyErrorMessage(dpsl.telemetry.getBacklightInfo(), expectedResult),
    verifyErrorMessage(dpsl.telemetry.getBatteryInfo(), expectedResult),
    verifyErrorMessage(
        dpsl.telemetry.getNonRemovableBlockDevicesInfo(), expectedResult),
    verifyErrorMessage(dpsl.telemetry.getCachedVpdInfo(), expectedResult),
    verifyErrorMessage(dpsl.telemetry.getCpuInfo(), expectedResult),
    verifyErrorMessage(dpsl.telemetry.getTimezoneInfo(), expectedResult),
    verifyErrorMessage(dpsl.telemetry.getMemoryInfo(), expectedResult),
    verifyErrorMessage(dpsl.telemetry.getFanInfo(), expectedResult),
    verifyErrorMessage(
        dpsl.telemetry.getStatefulPartitionInfo(), expectedResult),
    verifyErrorMessage(dpsl.telemetry.getBluetoothInfo(), expectedResult)
  ]);
});

// Tests that TelemetryInfo with errors can be successfully requested from
// from chrome-untrusted://.
UNTRUSTED_TEST('UntrustedRequestTelemetryInfoWithErrors', async () => {
  const expectedResult = {
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
  };

  const probeTelemetryResponse = await chromeos.telemetry.probeTelemetryInfo([
    'battery', 'non-removable-block-devices', 'cached-vpd-data', 'cpu',
    'timezone', 'memory', 'backlight', 'fan', 'stateful-partition', 'bluetooth'
  ]);
  assertDeepEquals(expectedResult, probeTelemetryResponse);

  // Tests for dpsl.telemetry.*:
  await Promise.all([
    verifyErrorMessage(
        dpsl.telemetry.getBacklightInfo(),
        JSON.stringify(expectedResult.backlightResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getBatteryInfo(),
        JSON.stringify(expectedResult.batteryResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getNonRemovableBlockDevicesInfo(),
        JSON.stringify(expectedResult.blockDeviceResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getCachedVpdInfo(),
        JSON.stringify(expectedResult.vpdResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getCpuInfo(),
        JSON.stringify(expectedResult.cpuResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getTimezoneInfo(),
        JSON.stringify(expectedResult.timezoneResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getMemoryInfo(),
        JSON.stringify(expectedResult.memoryResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getFanInfo(),
        JSON.stringify(expectedResult.fanResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getStatefulPartitionInfo(),
        JSON.stringify(expectedResult.statefulPartitionResult.error)),
    verifyErrorMessage(
        dpsl.telemetry.getBluetoothInfo(),
        JSON.stringify(expectedResult.bluetoothResult.error))
  ]);
});

// Tests that dpsl.system_events.getAvailableEvents() return the correct list.
UNTRUSTED_TEST('UntrustedEventsServiceGetAvailableEvents', async () => {
  assertDeepEquals(
      [
        'ac-inserted', 'ac-removed', 'bluetooth-adapter-added',
        'bluetooth-adapter-property-changed', 'bluetooth-adapter-removed',
        'bluetooth-device-added', 'bluetooth-device-property-changed',
        'bluetooth-device-removed', 'lid-closed', 'lid-opened', 'os-resume',
        'os-suspend'
      ],
      dpsl.system_events.getAvailableEvents());
});
