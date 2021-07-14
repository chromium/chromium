# Desktop Web App Integration Testing Framework

## Background

The WebAppProvider system has very wide action space and testing state interactions between all of the subsystems is very difficult. The goal of this piece is to accept:
* A list of action-based tests which fully test the WebAppProvider system (required-coverage tests), and
* A list of actions supported by the integration test framework (per-platform),
and output
* a minimal number of tests (per-platform) for the framework to execute that has the most coverage possible of the original list of tests, and
* the resulting coverage of the system (with required-coverage tests as 100%).

This is done by downloading [data](data/) from a [google sheet](https://docs.google.com/spreadsheets/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml), processing it, and saving the results in the [output/](output/) folder.

See the [design doc](https://docs.google.com/document/d/e/2PACX-1vTFI0sXhZMvvg1B3sctYVUe64WbLVNzuXFUa6f3XyYTzKs2JnuFR8qKNyXYZsxE-rPPvsq__4ZCyrcS/pub) for more information and links.

This README covers how the spreadsheet & tests are read to compute coverage information and tell the script runner how to modify the tests to get the respective coverage. For information about how to implement actions in the framework (or how the framework implementation works), see the [testing framework implementation README.md](../../browser/ui/views/web_apps/README.md).

Related:
  * [WebAppProvider README.md](../../browser/web_applications/README.md)

## Future Work

* Integration manual test support.

## Terminology

### Action
A primitive test operation, or test building block, that can be used to create coverage tests. The integration test framework may or may not support an action on a given platform. Actions can fall into one of three types:
* State-change action
* State-check action
* parameterized action

Actions also can be fully, partially, or not supported on a given platform. This information is used when generating the tests to run & coverage report for a given platform. To make parsing easier, actions are always snake_case.

#### State-change Action
A state-changing action is expected to change the state chrome or the web app provider system.

Examples: `navigate_browser(SiteA)`, `switch_incognito_profile`, `sync_turn_off`, `set_app_badge`

#### State-check Action
Some actions are classified as "state check" actions, which means they do not change any state and only inspect the state of the system. In graph representations, state check actions are not given nodes, and instead live under the last non-state-check action.

All actions that start with `check_` are considered state-check actions.

Examples: `check_app_list_empty`, `check_install_icon_shown`, `check_platform_shortcut_exists(SiteA)`, `check_tab_created`

#### Action Mode
When creating tests, there emerged a common scenario where a given action could be applied to multiple different sites. For example, the “navigate the browser to an installable site” action was useful if “site” could be customized.

The simplest possible mode system to solve this:
* Each action can have at most one mode.
* Modes are static / pre-specified per action.
* A default mode can be specified to support the case where an action has modes but none were specified in the test.

To allow for future de-parsing of modes (when generating C++ tests), modes will always be CapitalCase.

#### Parameterized Action
To help with testing scenarios like outlined above, an action can be defined that references or 'turns into' a set of non-parameterized actions. For example, an action `install_windowed` can be created and reference the set of actions `install_omnibox_icon`, `install_menu_option`, `install_create_shortcut_windowed`, `add_policy_app_windowed_shortcuts`, and `add_policy_app_windowed_no_shortcuts`. When a test case includes this action, it will generate multiple tests in which the parameterized action is replaced with the non-parameterized action.

### Tests
A sequence of actions used to test the WebAppProvider system. A test that can be run by the test framework must not have any "parameterized" actions, as these are supposed to be used to generate multiple tests.

#### Unprocessed Required-coverage tests
This is the set of tests that, if all executed, should provide full test coverage for the WebAppProvider system. They currently live in this sheet as "unprocessed".

#### Required-coverage tests (processed)
Processed tests go through the following steps from the unprocessed version in the sheet:
* Tests with one or more "parameterized" actions have been processed to produce the resulting tests without parameterized actions.
* Actions in tests that have modes but do not specify them have the default mode added to them.

#### Platform-specific tests
Some tests are going to be platform-specific. For example, all tests that involve "locally installing" an app are only applicable on Windows/Mac/Linux, as ChromeOS automatically locally installs all apps from sync. Because of this, tests must be able to specify which platforms they should be run on. This is done by specifying the platforms each test applies to in a column on the spreadsheet.

## Understanding and Implementing Test Cases

Actions are the basic building blocks of integration tests. A test is a sequence of actions. Each action has a name that must be a valid C++ identifier.

Actions are defined (and can be modified) in [this](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1864725389) sheet. Tests are defined (and can be modified) in [this](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=2008870403) sheet.

### Action Creation & Specification

[Actions](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1864725389) are the building blocks of tests.

#### Templates
To help making test writing less repetitive, actions are described as templates in the [actions](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1864725389) spreadsheet. Action templates specify actions while avoiding rote repetition. Each action template has a name (the **action base name**). Each action template supports a mode, which takes values from a predefined list associated with the template. Parameter values must also be valid C++ identifiers.

An action template without modes specifies one action whose name matches the template. For example, the `check_tab_created` template generates the `check_tab_created` action.

An action template with a mode that can take N values specifies N actions, whose names are the concatenations of the template name and the corresponding value name, separated by an underscore (_). For example, the `clear_app_badge` template generates the `clear_app_badge_SiteA` and `clear_app_badge_SiteB` actions.

The templates also support [parameterizing](#parameterized-action) an action, which causes any test that uses the action to be expanded into multiple tests, one per specified output action. Modes will carry over into the output action, and if an output action doesn't support a given mode then that parameterization is simply excluded during test generation.

#### Default Values

All templates with modes can mark one of their mode values as the default value. This value is used to magically convert template names to action names.

#### Specifying a Mode
Human-friendly action names are a slight variation of the canonical names above.

Actions generated by mode-less templates have the same human-friendly name as their (canonical?) name.

Actions generated by parametrized templates use parenthesis to separate the template name from the value name. For example, the actions generated by the `clear_app_badge` template have the human-friendly names `clear_app_badge(SiteA)` and `clear_app_badge(SiteB)`.

The template name can be used as the human-friendly name of the action generated by the template with the default value. For example, `clear_app_badge` is a shorter human-friendly name equivalent to `clear_app_badge(SiteA)`.

### Test Creation & Specification

[Tests](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=2008870403) are created specifying actions.

#### Mindset
The mindset for test creation and organization is to really exhaustively check every possible string of user actions. The framework will automatically combine tests that are the same except for state check actions. They are currently organized by:
1. **Setup actions** - The state change actions needed to enter the system state that is being tested.
2. **Primary state-change action/s** - Generally one action that will change the system state after the setup.
3. **State check action** - One state check action, checking the state of the system after the previous actions have executed.

Each test can have at most one [state check](#state-check-action) action as the last action.

One way to enumerate tests is to think about **affected-by** action edges. These are a pair of two actions, where the first action affects the second. For example, the action `set_app_badge` will affect the action `check_app_badge_has_value`. Or, `uninstall_from_app_list` will affect `check_platform_shortcut_exists`. There is often then different setup states that would effect these actions. Once these edges are identified, tests can be created around them.

#### Creating a test

A test should be created that does the bare minimum necessary to set up the test before testing the primary state change action and then checking the state.

The framework is designed to be able to collapse tests that contain common non-'state-check' actions, so adding a new test does not always mean that a whole new test will be run by the framework. Sometimes it only adds a few extra state-check actions in an existing test.

#### Adding an action

If a new test needs a new action implemented, it will only be used in the generated tests if it is added to the [actions](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1864725389) sheet, and it is specifically marked as supported or partially supported. Then the script will print out the relevant browsertest to add to the relevant file.

The action also needs to be implemented by the testing framework. See the [Testing Framework Implementation README.md](../../browser/ui/views/web_apps/README.md) for more info about how to do that.

#### Adding 'support' for an action

To tell the script that an action is supported by the testing framework (on a given platform), modify the [`framework_supported_actions.csv`](./data/framework_supported_actions.csv) file, and use the following emojis to specify coverage for an action on a given platform:

* 🌕 - Full coverage
* 🌓 - Partial coverage
* 🌑 - No coverage

The script reads this file to determine what tests to generate.

## Script Usage

### Downloading test data

The test data is hosted in this [spreadsheet](https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/edit). To download the latest copy of the data, run the included script:
```sh
./chrome/test/webapps/download_data_from_sheet.py
```

This will download the data from the sheet into csv files in the [data/](data/) directory:

* `actions.csv` This describes all actions that can be used in the required coverage tests (processed or unprocessed).
* `coverage_required.csv` This is the full list of all tests needed to fully cover the Web App system. The first column specifies the platforms for testing, and the test starts on the fifth column.

### Generating test descriptions & coverage

Required test changes are printed and coverage files are written by running:
```sh
chrome/test/webapps/generate_framework_tests_and_coverage.py
```
This uses the files in `chrome/test/webapps/data` and existing browsertests on the system (see `custom_partitions` and `default_partitions` in [generate_framework_tests_and_coverage.py](generate_framework_tests_and_coverage.py)) to:

#### 1) Print to `stdout` all detected changes needed to browsertests.
The script is not smart enough to automatically add/remove/move tests to keep complexity to a minimum. Instead, it prints out the tests that need to be added or removed to have the tests match what it expects. It assumes:
  * Browsertests are correctly described by the `TestPartitionDescription`s in `generate_framework_tests_and_coverage.py`.
  * Browsertests with the per-platform suffixes (e.g. `_mac`, `_win`, etc) are only run on those platforms

This process doesn't modify the browsertest files so any test disabling done by sheriffs can remain. The script runner is thus expected to make the requested changes manually. In the rare case that a test is moving between files (if we are enabling a test on a new platform, for example), then the script runner should be careful to copy any sheriff changes to the browsertest as well.

#### 2) Generate per-platform processed required coverage `tsv` files in `chrome/test/webapps/coverage`
These are the processed required coverage tests with markers per action to allow a conditional formatter (like the one [here](https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/edit#gid=884228058)) to highlight what was and was not covered by the testing framework.
  * These files also contain a coverage % at the top of the file. Full coverage is the percent of the actions of the processed required coverage test that were executed and fully covered by the framework. Partial coverage also includes actions that are partially covered by the framework.
  * This includes loss of coverage from any disabled tests. Cool!

### Exploring the tested and coverage graphs

To view the directed graphs that are generated to process the test and coverage data, the `--graphs` switch can be specified:
```sh
chrome/test/webapps/generate_framework_tests_and_coverage.py --graphs
```

This will generate:
* `coverage_required_graph.dot` - The graph of all of the required test coverage. Green nodes are actions explicitly listed in the coverage list, and orange nodes specify partial coverage paths.
* `framework_test_graph_<platform>.dot` - The graph that is now tested by the generated framework tests for the given platform, including partial coverage.

The [graphviz](https://graphviz.org/) library can be used to view these graphs. An install-free online version is [here](https://dreampuf.github.io/GraphvizOnline/).

### Debugging further

To help debug or explore further, please see the [`graph_cli_tool.py`](graph_cli_tool.py) script which includes a number of command line utilities to process the various files.

Both this file and the [`generate_framework_tests_and_coverage.py`](generate_framework_tests_and_coverage.py) file support the `-v` option to print out informational logging.
