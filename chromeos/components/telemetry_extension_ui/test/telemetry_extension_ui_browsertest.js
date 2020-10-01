// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for chrome://telemetry-extension.
 */

GEN('#include "chromeos/components/telemetry_extension_ui/test/telemetry_extension_ui_browsertest.h"');

GEN('#include "content/public/test/browser_test.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

const HOST_ORIGIN = 'chrome://telemetry-extension';
const UNTRUSTED_HOST_ORIGIN = 'chrome-untrusted://telemetry-extension';

var TelemetryExtensionUIBrowserTest = class extends testing.Test {
  /** @override */
  get browsePreload() {
    return HOST_ORIGIN;
  }

  /** @override */
  get runAccessibilityChecks() {
    return false;
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kTelemetryExtension']};
  }

  /** @override */
  get typedefCppFixture() {
    return 'TelemetryExtensionUiBrowserTest';
  }

  /** @override */
  get isAsync() {
    return true;
  }

  /** @override */
  get extraLibraries() {
    return [
      ...super.extraLibraries,
      '//chromeos/components/telemetry_extension_ui/test/trusted_test_requester.js',
    ];
  }
};

// Tests that chrome://telemetry-extension runs js file and that it goes
// somewhere instead of 404ing or crashing.
TEST_F('TelemetryExtensionUIBrowserTest', 'HasChromeSchemeURL', () => {
  const title = document.querySelector('title');

  assertEquals(title.innerText, 'Telemetry Extension');
  assertEquals(document.location.origin, HOST_ORIGIN);
  testDone();
});

// Tests that chrome://telemetry-extension embeds a
// chrome-untrusted:// iframe.
TEST_F('TelemetryExtensionUIBrowserTest', 'HasChromeUntrustedIframe', () => {
  const iframe = document.querySelector('iframe');
  assertNotEquals(iframe, null);
  testDone();
});

TEST_F('TelemetryExtensionUIBrowserTest', 'ConvertDiagnosticsEnums', () => {
  // Unit tests for convertRoutineId
  assertEquals(diagnosticsProxy.convertRoutineId(234089591), 234089591);

  // Unit tests for convertCommandToEnum
  const commandEnum = chromeos.health.mojom.DiagnosticRoutineCommandEnum;

  assertEquals(
      diagnosticsProxy.convertCommandToEnum('continue'), commandEnum.kContinue);
  assertEquals(
      diagnosticsProxy.convertCommandToEnum('cancel'), commandEnum.kCancel);
  assertEquals(
      diagnosticsProxy.convertCommandToEnum('get-status'),
      commandEnum.kGetStatus);
  assertEquals(
      diagnosticsProxy.convertCommandToEnum('remove'), commandEnum.kRemove);

  // Unit tests for convertStatus
  const statusEnum = chromeos.health.mojom.DiagnosticRoutineStatusEnum;

  assertEquals(diagnosticsProxy.convertStatus(statusEnum.kReady), 'ready');
  assertEquals(diagnosticsProxy.convertStatus(statusEnum.kRunning), 'running');
  assertEquals(diagnosticsProxy.convertStatus(statusEnum.kWaiting), 'waiting');
  assertEquals(diagnosticsProxy.convertStatus(statusEnum.kPassed), 'passed');
  assertEquals(diagnosticsProxy.convertStatus(statusEnum.kFailed), 'failed');
  assertEquals(diagnosticsProxy.convertStatus(statusEnum.kError), 'error');
  assertEquals(
      diagnosticsProxy.convertStatus(statusEnum.kCancelled), 'cancelled');
  assertEquals(
      diagnosticsProxy.convertStatus(statusEnum.kFailedToStart),
      'failed-to-start');
  assertEquals(diagnosticsProxy.convertStatus(statusEnum.kRemoved), 'removed');
  assertEquals(
      diagnosticsProxy.convertStatus(statusEnum.kCancelling), 'cancelling');
  assertEquals(
      diagnosticsProxy.convertStatus(statusEnum.kUnsupported), 'unsupported');

  // Unit tests for convertUserMessage
  const userMessageEnum =
      chromeos.health.mojom.DiagnosticRoutineUserMessageEnum;

  assertEquals(
      diagnosticsProxy.convertUserMessage(userMessageEnum.kUnplugACPower),
      'unplug-ac-power');
  assertEquals(
      diagnosticsProxy.convertUserMessage(userMessageEnum.kPlugInACPower),
      'plug-in-ac-power');

  // Unit tests for convertPowerStatusToEnum
  const acPowerStatusEnum = chromeos.health.mojom.AcPowerStatusEnum;
  assertEquals(
      diagnosticsProxy.convertPowerStatusToEnum('connected'),
      acPowerStatusEnum.kConnected);
  assertEquals(
      diagnosticsProxy.convertPowerStatusToEnum('disconnected'),
      acPowerStatusEnum.kDisconnected);

  // Unit tests for convertNvmeSelfTestTypeToEnum
  const nvmeSelfTestTypeEnum = chromeos.health.mojom.NvmeSelfTestTypeEnum;
  assertEquals(
      diagnosticsProxy.convertNvmeSelfTestTypeToEnum('short-self-test'),
      nvmeSelfTestTypeEnum.kShortSelfTest);
  assertEquals(
      diagnosticsProxy.convertNvmeSelfTestTypeToEnum('long-self-test'),
      nvmeSelfTestTypeEnum.kLongSelfTest);

  // Unit tests for convertDiskReadTypeToEnum
  const diskReadRoutineTypeEnum = chromeos.health.mojom.DiskReadRoutineTypeEnum;
  assertEquals(
      diagnosticsProxy.convertDiskReadTypeToEnum('linear-read'),
      diskReadRoutineTypeEnum.kLinearRead);
  assertEquals(
      diagnosticsProxy.convertDiskReadTypeToEnum('random-read'),
      diskReadRoutineTypeEnum.kRandomRead);

  testDone();
});

// Tests that Telemetry correctly converts Mojo enums to strings.
TEST_F('TelemetryExtensionUIBrowserTest', 'ConvertTelemetryEnums', () => {
  // Unit tests for convertErrorType.
  const errorTypeEnum = chromeos.health.mojom.ErrorType;

  assertEquals(
      telemetryProxy.convertErrorType(errorTypeEnum.kFileReadError),
      'file-read-error');
  assertEquals(
      telemetryProxy.convertErrorType(errorTypeEnum.kParseError),
      'parse-error');
  assertEquals(
      telemetryProxy.convertErrorType(errorTypeEnum.kSystemUtilityError),
      'system-utility-error');
  assertEquals(
      telemetryProxy.convertErrorType(errorTypeEnum.kServiceUnavailable),
      'service-unavailable');

  // Unit tests for convertCpuArch.
  const cpuArchEnum = chromeos.health.mojom.CpuArchitectureEnum;

  assertEquals(telemetryProxy.convertCpuArch(cpuArchEnum.kUnknown), 'unknown');
  assertEquals(telemetryProxy.convertCpuArch(cpuArchEnum.kX86_64), 'x86-64');
  assertEquals(telemetryProxy.convertCpuArch(cpuArchEnum.kAArch64), 'AArch64');
  assertEquals(telemetryProxy.convertCpuArch(cpuArchEnum.kArmv7l), 'Armv7l');

  // Check that convertAllEnums converts all Mojo enums to strings and does not
  // crash if some enums are not present in TelemetryInfo.
  assertDeepEquals(telemetryProxy.convertAllEnums({}), {});
  assertDeepEquals(
      telemetryProxy.convertAllEnums({
        batteryResult: {},
        blockDeviceResult: {},
        vpdResult: {},
        cpuResult: {},
        timezoneResult: {},
        memoryResult: {},
        backlightResult: {},
        fanResult: {},
        statefulPartitionResult: {},
        bluetoothResult: {}
      }),
      {
        batteryResult: {},
        blockDeviceResult: {},
        vpdResult: {},
        cpuResult: {},
        timezoneResult: {},
        memoryResult: {},
        backlightResult: {},
        fanResult: {},
        statefulPartitionResult: {},
        bluetoothResult: {}
      });
  // Check architecture equals to 0 (i.e. kUnknown).
  assertDeepEquals(
      telemetryProxy.convertAllEnums({
        cpuResult:
            {cpuInfo: {architecture: cpuArchEnum.kUnknown, physicalCpus: []}}
      }),
      {cpuResult: {cpuInfo: {architecture: 'unknown', physicalCpus: []}}});
  assertDeepEquals(
      telemetryProxy.convertAllEnums({
        cpuResult:
            {cpuInfo: {architecture: cpuArchEnum.kX86_64, physicalCpus: []}}
      }),
      {cpuResult: {cpuInfo: {architecture: 'x86-64', physicalCpus: []}}});
  // Check error type equals to 0 (i.e. kFileReadError).
  assertDeepEquals(
      telemetryProxy.convertAllEnums({
        batteryResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'battery'}},
        blockDeviceResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'ssd'}},
        vpdResult: {error: {type: errorTypeEnum.kFileReadError, msg: 'vpd'}},
        cpuResult: {error: {type: errorTypeEnum.kFileReadError, msg: 'cpu'}},
        timezoneResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'timezone'}},
        memoryResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'memory'}},
        backlightResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'backlight'}},
        fanResult: {error: {type: errorTypeEnum.kFileReadError, msg: 'fan'}},
        statefulPartitionResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'partition'}},
        bluetoothResult:
            {error: {type: errorTypeEnum.kFileReadError, msg: 'bluetooth'}}
      }),
      {
        batteryResult: {error: {type: 'file-read-error', msg: 'battery'}},
        blockDeviceResult: {error: {type: 'file-read-error', msg: 'ssd'}},
        vpdResult: {error: {type: 'file-read-error', msg: 'vpd'}},
        cpuResult: {error: {type: 'file-read-error', msg: 'cpu'}},
        timezoneResult: {error: {type: 'file-read-error', msg: 'timezone'}},
        memoryResult: {error: {type: 'file-read-error', msg: 'memory'}},
        backlightResult: {error: {type: 'file-read-error', msg: 'backlight'}},
        fanResult: {error: {type: 'file-read-error', msg: 'fan'}},
        statefulPartitionResult:
            {error: {type: 'file-read-error', msg: 'partition'}},
        bluetoothResult: {error: {type: 'file-read-error', msg: 'bluetooth'}}
      });
  assertDeepEquals(
      telemetryProxy.convertAllEnums({
        batteryResult:
            {error: {type: errorTypeEnum.kParseError, msg: 'battery'}},
        blockDeviceResult:
            {error: {type: errorTypeEnum.kSystemUtilityError, msg: 'ssd'}},
        vpdResult:
            {error: {type: errorTypeEnum.kServiceUnavailable, msg: 'vpd'}},
        cpuResult: {error: {type: errorTypeEnum.kParseError, msg: 'cpu'}},
        timezoneResult:
            {error: {type: errorTypeEnum.kSystemUtilityError, msg: 'timezone'}},
        memoryResult:
            {error: {type: errorTypeEnum.kServiceUnavailable, msg: 'memory'}},
        backlightResult:
            {error: {type: errorTypeEnum.kParseError, msg: 'backlight'}},
        fanResult:
            {error: {type: errorTypeEnum.kSystemUtilityError, msg: 'fan'}},
        statefulPartitionResult: {
          error: {type: errorTypeEnum.kServiceUnavailable, msg: 'partition'}
        },
        bluetoothResult:
            {error: {type: errorTypeEnum.kParseError, msg: 'bluetooth'}}
      }),
      {
        batteryResult: {error: {type: 'parse-error', msg: 'battery'}},
        blockDeviceResult: {error: {type: 'system-utility-error', msg: 'ssd'}},
        vpdResult: {error: {type: 'service-unavailable', msg: 'vpd'}},
        cpuResult: {error: {type: 'parse-error', msg: 'cpu'}},
        timezoneResult:
            {error: {type: 'system-utility-error', msg: 'timezone'}},
        memoryResult: {error: {type: 'service-unavailable', msg: 'memory'}},
        backlightResult: {error: {type: 'parse-error', msg: 'backlight'}},
        fanResult: {error: {type: 'system-utility-error', msg: 'fan'}},
        statefulPartitionResult:
            {error: {type: 'service-unavailable', msg: 'partition'}},
        bluetoothResult: {error: {type: 'parse-error', msg: 'bluetooth'}}
      });

  testDone();
});

// Tests that Telemetry.convert method correctly converts Mojo types into WebIDL
// types.
TEST_F(
    'TelemetryExtensionUIBrowserTest', 'ConvertTelemetryMojoTypesToWebIDLTypes',
    () => {
      // null/undefined --> null.
      assertEquals(telemetryProxy.convert(null), null);
      assertEquals(telemetryProxy.convert(undefined), null);

      // number/string --> number/string.
      assertEquals(telemetryProxy.convert('simple string'), 'simple string');
      assertEquals(telemetryProxy.convert(2020), 2020);

      // {value: X} --> X if X is a number.
      assertEquals(telemetryProxy.convert({value: 15}), 15);
      assertEquals(telemetryProxy.convert({value: 777.555}), 777.555);

      // {value: X} --> X if X is a booelan.
      assertEquals(telemetryProxy.convert({value: false}), false);
      assertEquals(telemetryProxy.convert({value: true}), true);

      // {value: X} --> {value: X} if X is not a number.
      assertDeepEquals(telemetryProxy.convert({value: 'ABC'}), {value: 'ABC'});
      assertDeepEquals(
          telemetryProxy.convert({value: {k: 'v'}}), {value: {k: 'v'}});

      // omit null/undefined properties.
      assertDeepEquals(
          telemetryProxy.convert({a: 1, b: null, c: undefined}), {a: 1});
      assertDeepEquals(
          telemetryProxy.convert({a: {x: null, y: undefined, z: 'zZz'}}),
          {a: {z: 'zZz'}});

      // covnert arrays.
      assertDeepEquals(telemetryProxy.convert([]), []);
      assertDeepEquals(telemetryProxy.convert([null, undefined]), []);
      assertDeepEquals(telemetryProxy.convert([{value: 1}]), [1]);
      assertDeepEquals(
          telemetryProxy.convert([null, undefined, {a: 1}, []]), [{a: 1}, []]);
      assertDeepEquals(
          telemetryProxy.convert(
              [{a: null, b: undefined}, {x: 'xxx', y: null}]),
          [{x: 'xxx'}]);

      // convert objects without properties to null.
      assertEquals(telemetryProxy.convert({}), null);
      assertEquals(telemetryProxy.convert({a: null, b: undefined}), null);
      assertEquals(telemetryProxy.convert({a: {x: null, y: undefined}}), null);

      assertDeepEquals(
          telemetryProxy.convert({
            a: 1,
            b: null,
            c: {x: {value: 1000}, y: 'YYY', z: {value: 'ZzZ'}}
          }),
          {a: 1, c: {x: 1000, y: 'YYY', z: {value: 'ZzZ'}}});

      testDone();
    });

var TelemetryExtensionUIWithInteractiveRoutineUpdateBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('ConfigureDiagnosticsForInteractiveUpdate();');
  }
}

var TelemetryExtensionUIWithNonInteractiveRoutineUpdateBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('ConfigureDiagnosticsForNonInteractiveUpdate();');
  }
}

var TelemetryExtensionUIWithProbeServiceErrorsBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('ConfigureProbeServiceToReturnErrors();');
  }
}

// Test cases injected into the untrusted context.
// See implementations in untrusted_browsertest.js.
//
// To register a test with TelemetryExtensionUIBrowserTest class add an array
// with the test name from untrusted_browsertest.js.
// Example: ['UntrustedAwesomeTest']
//
// To register a test with a custom test class add an array with the test name
// from untrusted_browsertest.js and test class name whose definition needs to
// be above untrustedTests definition.
// Example: ['UntrustedAwesomeTest', 'MyAwesomeTestClass']
const untrustedTests = [
  ['UntrustedCanSpawnWorkers'],
  ['UntrustedDiagnosticsRequestAvailableRoutines'],
  ['UntrustedDiagnosticsRequestRoutineUpdateUnknownArguments'],
  ['UntrustedDiagnosticsRequestRunBatteryCapacityRoutine'],
  ['UntrustedDiagnosticsRequestRunBatteryHealthRoutine'],
  ['UntrustedDiagnosticsRequestRunSmartctlCheckRoutine'],
  ['UntrustedDiagnosticsRequestRunAcPowerRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunAcPowerRoutine'],
  ['UntrustedDiagnosticsRequestRunCpuCacheRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunCpuCacheRoutine'],
  ['UntrustedDiagnosticsRequestRunCpuStressRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunCpuStressRoutine'],
  ['UntrustedDiagnosticsRequestRunFPAccuracyRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunFPAccuracyRoutine'],
  ['UntrustedDiagnosticsRequestRunNvmeWearLevelRoutine'],
  ['UntrustedDiagnosticsRequestRunNvmeSelfTestRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunNvmeSelfTestRoutine'],
  ['UntrustedDiagnosticsRequestRunDiskReadRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunDiskReadRoutine'],
  ['UntrustedDiagnosticsRequestRunPrimeSearchRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunPrimeSearchRoutine'],
  ['UntrustedDiagnosticsRequestRunBatteryDischargeRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunBatteryDischargeRoutine'],
  ['UntrustedDiagnosticsRequestRunBatteryChargeRoutineInvalidInput'],
  ['UntrustedDiagnosticsRequestRunBatteryChargeRoutine'],
  ['UntrustedLidEventListener'],
  ['UntrustedRequestTelemetryInfoUnknownCategory'],
  ['UntrustedRequestTelemetryInfo'],
  [
    'UntrustedDiagnosticsRequestInteractiveRoutineUpdate',
    'TelemetryExtensionUIWithInteractiveRoutineUpdateBrowserTest'
  ],
  [
    'UntrustedDiagnosticsRequestNonInteractiveRoutineUpdate',
    'TelemetryExtensionUIWithNonInteractiveRoutineUpdateBrowserTest'
  ],
  [
    'UntrustedRequestTelemetryInfoWithErrors',
    'TelemetryExtensionUIWithProbeServiceErrorsBrowserTest'
  ],
].forEach(test => registerUntrustedTest(...test));

/**
 * Registers a test in chrome-untrusted://.
 * @param {!string} testName
 * @param {!string=} testClass
 */
function registerUntrustedTest(
    testName, testClass = 'TelemetryExtensionUIBrowserTest') {
  TEST_F(testClass, testName, async () => {
    await runTestInUntrusted(testName);
    testDone();
  });
}

/**
 * @implements {chromeos.health.mojom.ProbeServiceInterface}
 */
class TestProbeService {
  constructor() {
    /**
     * @type {chromeos.health.mojom.ProbeServiceReceiver}
     */
    this.receiver_ = null;

    /**
     * @type {Array<!chromeos.health.mojom.ProbeCategoryEnum>}
     */
    this.probeTelemetryInfoCategories = null;
  }

  /**
   * @param {!MojoHandle} handle
   */
  bind(handle) {
    this.receiver_ = new chromeos.health.mojom.ProbeServiceReceiver(this);
    this.receiver_.$.bindHandle(handle);
  }

  /**
   * @override
   * @param { !Array<!chromeos.health.mojom.ProbeCategoryEnum> } categories
   * @return {!Promise<{telemetryInfo:
   *     !chromeos.health.mojom.TelemetryInfo}>}
   */
  probeTelemetryInfo(categories) {
    this.probeTelemetryInfoCategories = categories;

    const telemetryInfo =
        /** @type {!chromeos.health.mojom.TelemetryInfo} */ ({
          backlightResult: null,
          batteryResult: null,
          blockDeviceResult: null,
          bluetoothResult: null,
          cpuResult: null,
          fanResult: null,
          memoryResult: null,
          statefulPartitionResult: null,
          timezoneResult: null,
          vpdResult: null,
        });
    return Promise.resolve({telemetryInfo});
  }
};

// Tests with a testing Mojo probe service, so we can test for example strings
// conversion to Mojo enum values.
var TelemetryExtensionUIWithInterceptorBrowserTest =
    class extends TelemetryExtensionUIBrowserTest {
  constructor() {
    super();

    /**
     * @type {TestProbeService}
     */
    this.probeService = null;

    this.probeServiceInterceptor = null;
  }

  /** @override */
  setUp() {
    this.probeService = new TestProbeService();

    /** @suppress {undefinedVars} */
    this.probeServiceInterceptor = new MojoInterfaceInterceptor(
        chromeos.health.mojom.ProbeService.$interfaceName);
    this.probeServiceInterceptor.oninterfacerequest = (e) => {
      this.probeService.bind(e.handle);
    };
    this.probeServiceInterceptor.start();
  }
};

// Test cases injected into the untrusted context.
// See implementations in untrusted_browsertest.js.

TEST_F(
    'TelemetryExtensionUIWithInterceptorBrowserTest',
    'UntrustedRequestTelemetryInfoWithInterceptor', async function() {
      await runTestInUntrusted('UntrustedRequestTelemetryInfoWithInterceptor');

      assertDeepEquals(this.probeService.probeTelemetryInfoCategories, [
        chromeos.health.mojom.ProbeCategoryEnum.kBattery,
        chromeos.health.mojom.ProbeCategoryEnum.kNonRemovableBlockDevices,
        chromeos.health.mojom.ProbeCategoryEnum.kCachedVpdData,
        chromeos.health.mojom.ProbeCategoryEnum.kCpu,
        chromeos.health.mojom.ProbeCategoryEnum.kTimezone,
        chromeos.health.mojom.ProbeCategoryEnum.kMemory,
        chromeos.health.mojom.ProbeCategoryEnum.kBacklight,
        chromeos.health.mojom.ProbeCategoryEnum.kFan,
        chromeos.health.mojom.ProbeCategoryEnum.kStatefulPartition,
        chromeos.health.mojom.ProbeCategoryEnum.kBluetooth
      ]);

      testDone();
    });
