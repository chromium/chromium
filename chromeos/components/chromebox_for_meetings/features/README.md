# Chromebox for Meetings Features

Note: Feature Flags are run time flags

## CfmMojoServices (Feature)

```bash
$ ssh <cfm-dut>
$ echo "--enable-features=CfmMojoServices" >> /etc/chrome_dev.conf
$ reboot ui
```

This feature flag is controlled by a server-side experiment that enables
Chromium to interact with mojom based Chromebox for Meetings services.

### ERPTelemetryService (Feature Param)

```bash
$ ssh <cfm-dut>
$ echo "--enable-features=CfmMojoServices:ERPTelemetryService/true" >> /etc/chrome_dev.conf
$ reboot ui
```

This feature parameter enables the use of the encrypted reporting service api for cloud logging and telemetry on the Chromebox for Meetings platforms. It is dependent on the `CfmMojoService` feature flag being enabled.
