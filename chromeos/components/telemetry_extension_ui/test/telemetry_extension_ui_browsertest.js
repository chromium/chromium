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

// Test cases injected into the untrusted context.
// See implementations in untrusted_browsertest.js.

TEST_F(
    'TelemetryExtensionUIBrowserTest', 'UntrustedCanSpawnWorkers', async () => {
      await runTestInUntrusted('UntrustedCanSpawnWorkers');
      testDone();
    });

TEST_F(
    'TelemetryExtensionUIBrowserTest', 'UntrustedRequestAvailableRoutines',
    async () => {
      await runTestInUntrusted('UntrustedRequestAvailableRoutines');
      testDone();
    });

TEST_F(
    'TelemetryExtensionUIBrowserTest',
    'UntrustedDiagnosticsRequestRoutineUpdateUnknownArguments', async () => {
      await runTestInUntrusted(
          'UntrustedDiagnosticsRequestRoutineUpdateUnknownArguments');
      testDone();
    });

TEST_F(
    'TelemetryExtensionUIBrowserTest',
    'UntrustedRequestTelemetryInfoUnknownCategory', async () => {
      await runTestInUntrusted('UntrustedRequestTelemetryInfoUnknownCategory');
      testDone();
    });

TEST_F(
    'TelemetryExtensionUIBrowserTest', 'UntrustedRequestTelemetryInfo',
    async () => {
      await runTestInUntrusted('UntrustedRequestTelemetryInfo');
      testDone();
    });

var DiagnosticsInteractiveRoutineUpdate =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('ConfigureDiagnosticsForInteractiveUpdate();');
  }
}

TEST_F(
    'DiagnosticsInteractiveRoutineUpdate',
    'UntrustedDiagnosticsRequestInteractiveRoutineUpdate', async () => {
      await runTestInUntrusted(
          'UntrustedDiagnosticsRequestInteractiveRoutineUpdate');
      testDone();
    });

var DiagnosticsNonInteractiveRoutineUpdate =
    class extends TelemetryExtensionUIBrowserTest {
  /** @override */
  testGenPreamble() {
    GEN('ConfigureDiagnosticsForNonInteractiveUpdate();');
  }
}

TEST_F(
    'DiagnosticsNonInteractiveRoutineUpdate',
    'UntrustedDiagnosticsRequestNonInteractiveRoutineUpdate', async () => {
      await runTestInUntrusted(
          'UntrustedDiagnosticsRequestNonInteractiveRoutineUpdate');
      testDone();
    });

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
