# Overview
Diagnostic Processor Support Library (DPSL) is a collection of telemetry and
diagnostics interfaces exposed to third-parties:
   - chromeos.diagnostics
    | Diagnostics interface for running device diagnostics routines (tests).
   - chromeos.telemetry
    | Telemetry (a.k.a. Probe) interface for getting device telemetry
    | information.

# Usage Examples
```
// Fetch Telemetry Info: CPU, VPD Info.
const info = await chromeos.telemetry.probeTelemetryInfo(["cpu", "vpd"]);
console.log(info);

// Run NVMe Self Test routine
const response =
  await chromeos.diagnostics.runNvmeSelfTestRoutine('short-self-test');
console.log(response);
```
