# mini_installer Tests

Chromium's `mini_installer_tests` (driven by the
[`InstallerTest`](installer_test.py) class) evaluate machine state while
performing actions. This is Chromium's first-line defence against bugs in the
installer and browser that prevent Chromium from working as a user's chosen web
browser.

[TOC]

## Overview

`mini_installer_tests` is a
[typ](/third_party/catapult/third_party/typ/typ)-based test written in Python
that reads a [`config.config`](config/config.config) file, injects a `test_`
method for each test described therein, and runs the tests.

## Building

The `//chrome/test/mini_installer:mini_installer_tests` target builds all
required dependencies.

```
autoninja -C out/Default chrome/test/mini_installer:mini_installer_tests
```

## Running

The tests must be run from an elevated cmd prompt. The simplest case is to run
the test from the context of a build output directory like so:

```
cd out/Default
vpython3 ../../chrome/test/mini_installer/run_mini_installer_tests.py
```

## Configuration

The `config.config` file defines a list of `states`, a list of `actions`, and a
list of `tests`. Each named test defines a `traversal` list consisting of an
initial state followed by (action, state) pairs.

### States

Each state has a name (used in each test's `traversal` list) and a list of
property (`.prop`) file names. Each property file defines the expectations that
are validated by the test. A property file contains a dict that maps
[Validators](#validators) to the collection of properties to be validated.  All
of a state's property files are merged together before processing; in other
words, the state is valid if all expectations in the union of all properties are
satisfied.

#### Validators

Each validator is a simple Python function that takes the property name (e.g.,
the path to a file in the case of the [file](#Files) validator) and a dict
holding that property's type-specific validation data. The validator function
also takes a [`VariableExpander`](#variables) to be used to resolve variables in
the name and data.

##### Files

[`VerifyFileExpectation`](file_operations.py) asserts that a file either exists
or does not exist. For example:

```
{
  "Files": {
    "$PROGRAM_FILES\\$CHROME_DIR\\Application\\chrome.exe": {"exists": true}
  }
}
```

##### Processes

[`VerifyProcessExpectation`](process_operations.py) asserts that a process is or
is not running. For example:

```
{
  "Processes": {
    "$PROGRAM_FILES\\$CHROME_DIR\\Application\\chrome.exe": {"running": false}
  }
}
```

##### RegistryEntries

[`VerifyRegistryEntryExpectation`](registry_operations.py) asserts that a
registry key does not exist or does exist with specific values. For example:

```
{
  "RegistryEntries": {
    "HKEY_LOCAL_MACHINE\\$CHROME_CLIENT_STATE_KEY": {
      "exists": "required",
      "values": {
        "CleanInstallRequiredForVersionBelow": {
          "type": "SZ",
          "data": "$LAST_INSTALLER_BREAKING_VERSION"
        },
        "DowngradeCleanupCommand": {
          "type": "SZ",
          "data": "\"$PROGRAM_FILES\\$CHROME_DIR\\Application\\$MINI_INSTALLER_FILE_VERSION\\Installer\\setup.exe\" --cleanup-for-downgrade-version=$$1 --cleanup-for-downgrade-operation=$$2 --system-level --verbose-logging"
        }
      },
      "wow_key": "KEY_WOW64_32KEY"
    }
  }
}
```

#### Variables

Variables in property files are expanded during processing by a
[`VariableExpander`](variable_expander.py). This eases accommodation of Chromium
vs. Google Chrome branding.

#### Conditions

Tests in the main `config.config` file and states in `.prop` files may be
conditionally evaluated based on runtime state. This is generally used to only
run certain tests or evaluate certain expectations in branded builds. For
example, a test that only runs for Google Chrome might have a `condition` as
below:
```
{
  "tests": [
    {
      "name": "ChromeBeta",
      "description": "Verifies that Chrome Beta can be installed and uninstalled.",
      "condition": "'$BRAND' == 'Google Chrome'",
      "traversal": [
        "no_pv",
        "install_chrome_beta", "chrome_beta_installed_not_inuse",
        "test_chrome_with_chromedriver_beta", "chrome_beta_installed_not_inuse",
        "uninstall_chrome_beta", "clean"
      ]
    }
  ]
}
```

Such a test would be omitted from the collection of tests in `InstallerTest` if
the condition is not satisfied.

A property file that is only relevant for Google Chrome might have a top-level
`Condition` as below:
```
{
  "Condition": "'$BRAND' == 'Google Chrome'",
  "Files": {
    ...
  }
}
```

All expectations in such a property file would be ignored if the condition is
not satisfied.

An individual expectation for a Validator that is only relevant for Google
Chrome builds might have a `condition` as below:
```
{
  "Files": {
    "$PROGRAM_FILES\\$CHROME_DIR\\Application\\$MINI_INSTALLER_FILE_VERSION\\elevation_service.exe": {
      "condition": "'$CHROME_SHORT_NAME' == 'Chrome'",
      "exists": true
    }
  }
}
```

Such an expectation would be ignored while evaluating the expectations for any
state that included it.

### Actions

The `config.config` file contains a list of named `actions`. Each action is a
command that is run by the test to mutate the state of the machine in some way.
As of this writing, the primary actions are:

*   Run a `mini_installer`; e.g., `"\"$MINI_INSTALLER\" \"$LOG_FILE\"
    --verbose-logging --system-level --do-not-launch-chrome"` to perform a
    system-level installation.
*   Run some python script; e.g., `"\"$PYTHON_INTERPRETER\" uninstall_chrome.py
    \"$LOG_FILE\" --chrome-long-name=\"$CHROME_LONG_NAME\" --system-level"` to
    run the [`uninstall_chrome.py`](uninstall_chrome.py) script to uninstall a
    system-level Chrome via the command that would be used by the Add/Remove
    Programs control panel.

### Tests

The `config.config` file contains a list of named `tests`. Each has a
description and a `traversal`. A `traversal` is a list with an initial element
that names an item in the file's `states` followed by elements that, in turn,
each name an item in the file's `actions` and another item in the files
`states`. To run a given test, `InstallerTest` evaluates the state described by
the first item in the `traversal` list then iteratively performs each action and
evaluates the following state. A test passes if all actions succeed and
expectations are satisfied.
