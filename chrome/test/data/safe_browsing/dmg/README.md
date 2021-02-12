# Safe Browsing DMG Test Data

This directory contains scripts to generate test DMG and HFS files for
unit-testing the Safe Browsing archive scanner.

The `generate_test_data.sh` script that produces the files in the `data/`
subdirectory. It should be invoked like so:

    chrome/test/data/safe_browsing/dmg/generate_test_data.sh chrome/test/data/safe_browsing/dmg/data

The script will produce the data files and bundle them for uploading to
[CIPD](../../../../../docs/cipd_and_3pp.md). The script will produce a CIPD package
named `data.zip`. The ZIP contents should be inspected and, if they look good,
uploaded to the CIPD service and tagged with a version:

    cipd pkg-register data.zip -tag version:YYYYMMDD.N

For versioning the CIPD package, use a date in the form YYYYMMDD.N, where N is
an integer starting at 1 to differentiate different versions on the same day.

After uploading a new version to CIPD, the data need to be rolled into Chromium
by updating the version referenced in the root `DEPS` file.

Generating the data at build time is slow and has [caused](https://crbug.com/696529)
[issues](https://crbug.com/817663) on the bots in the past.
