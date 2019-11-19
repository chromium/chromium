# Signing Scripts for Chrome on macOS

This directory contains Python modules that modify the Chrome application bundle
for various release channels, sign the resulting bundle, package it into
`.dmg`/`.pkg` files for distribution, and sign those resulting `.dmg`/`.pkg`
files.

## Invoking

The scripts are invoked using the driver located at
`//chrome/installer/mac/sign_chrome.py`.

The `.dmg` packaging aspect of the process require assets that are only available
in a `src-internal` checkout, so these scripts will not fully succeed if invoked
from a Chromium-only checkout. The signing-only aspects can be run without the
`.dmg` packaging by passing `--disable-packaging`.

## Running Tests

Simply run the wrapper script at
`//chrome/installer/mac/signing/run_mac_signing_tests.py`.

You can pass `--coverage` or `-c` to show coverage information. To generate a
HTML coverage report and Python coverage package is available (via `pip install
coverage`), run:

    coverage3 run -m unittest discover -p '*_test.py'
    coverage3 html

## Formatting

The code is automatically formatted with YAPF. Run:

    git cl format --python
