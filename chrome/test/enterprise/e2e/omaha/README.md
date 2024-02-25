# Omaha Enterprise E2E test

Requirement Doc: <https://goto.google.com/cbe-omaha4-tests>

## How to run the test locally:

``` bash
vpython3 test.py --test omaha.rollback_to_target_version.google_update_policy_gpo.GoogleUpdatePolicyGPO --host=${HOME}/sandbox/ChromeEnterpriseLab/config/env.host.textpb --cel_ctl=${HOME}/go/src/chromium.googlesource.com/enterprise/cel/out/linux_amd64/bin/cel_ctl --test_arg=--chrome_installer=${HOME}/tmp/GoogleChromeStandaloneEnterprise.msi --test_arg=--chromedriver=${HOME}/tmp/chromedriver.exe --test_arg=--omaha_installer=${HOME}/tmp/UpdaterSetup.exe --test_arg=--omaha_updater=${HOME}/tmp/updater.exe
```

## Details about the test setup and framework
https://g3doc.corp.google.com/googleclient/chrome/enterprise/g3doc/celab/write_enterprise_test.md
