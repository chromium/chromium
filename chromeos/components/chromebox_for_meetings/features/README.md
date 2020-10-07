# Features

Note: Feature Flags are run time flags

## CfmMojoServices

```bash
$ ssh <cfm-dut>
$ echo "--enable-features=CfmMojoServices" >> /etc/chrome_dev.conf
$ reboot ui
```

This Feature flag is controlled by a server-side experiment that enables
Chromium to interact with mojom based Chromebox for Meetings services.
