// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TEST_TELEMETRY_EXTENSION_UI_BROWSERTEST_H_
#define CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TEST_TELEMETRY_EXTENSION_UI_BROWSERTEST_H_

#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/web_applications/test/sandboxed_web_ui_test_base.h"

class TelemetryExtensionUiBrowserTest : public SandboxedWebUiAppTestBase {
 public:
  TelemetryExtensionUiBrowserTest();
  ~TelemetryExtensionUiBrowserTest() override;

  TelemetryExtensionUiBrowserTest(const TelemetryExtensionUiBrowserTest&) =
      delete;
  TelemetryExtensionUiBrowserTest& operator=(
      const TelemetryExtensionUiBrowserTest&) = delete;

  // SandboxedWebUiAppTestBase overrides:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

  void ConfigureDiagnosticsForInteractiveUpdate();
  void ConfigureDiagnosticsForNonInteractiveUpdate();

  void ConfigureProbeServiceToReturnErrors();

  void ConfigureSystemEventsServiceToEmitEvents();

 private:
  // Use to post and cancel tasks for emitting system events.
  base::WeakPtrFactory<TelemetryExtensionUiBrowserTest>
      system_events_weak_ptr_factory_{this};
};

#endif  // CHROMEOS_COMPONENTS_TELEMETRY_EXTENSION_UI_TEST_TELEMETRY_EXTENSION_UI_BROWSERTEST_H_
