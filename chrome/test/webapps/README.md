# Desktop Web App Integration Testing Framework

## Future work

* The framework test enumeration given the graph could be slightly improved to reduce the final tests. Extra tests can be created if there is a branch-and-rejoin in the graph, but thankfully not exponentially.
* The algorithm in `generate_framework_tests_and_coverage.py` file could be improved by not re-reading files and re-generating graphs. One way to do this is implement deep copying for the graph, so these can be saved & re-used in later iterations.
* Using the action name to infer `state_check` and `internal` properties. `state_check` actions should start with `assert_`, and `internal` actions should end with `_internal`.
* Change "action parameters" to "action modes".

## Background

The WebAppProvider system has very wide action space and testing state interactions between all of the subsystems is very difficult. The goal of this piece is to accept:
* A list of action-based tests which fully test the WebAppProvider system (required-coverage tests), and
* A list of actions supported by the integration test framework (per-platform),
and output
* a minimal number of tests (per-platform) for the framework to execute that has the most coverage possible of the original list of tests, and
* the resulting coverage of the system (with required-coverage tests as 100%).

This is done by downloading [data](data/) from a [google sheet](https://docs.google.com/spreadsheets/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml), processing it, and saving the results in the [output/](output/) folder.

See the [design doc](https://docs.google.com/document/d/e/2PACX-1vTFI0sXhZMvvg1B3sctYVUe64WbLVNzuXFUa6f3XyYTzKs2JnuFR8qKNyXYZsxE-rPPvsq__4ZCyrcS/pub) for more information and links.

## Terminology

* **Action** - A primitive test operation, or test building block, that can be used to create coverage tests. The integration test framework may or may not support an action on a given platform.
* **Test** - A sequence of actions used to test the WebAppProvider system.
* **'State Check' action** - Action that does not change any system state, and instead just inspects the state to ensure it is as expected (e.g. `assert_install_icon_shown`).
* **Full & Partial coverage** - this describes the way that integration tests may test the system. If all actions in the coverage tests are supported by the framework, then the framework will be able to 'fully' cover the required coverage tests. However, this is not always the case, and to help compensate for un-automatable actions, the framework supports adding 'partial coverage' nodes or paths to the graph. This allows the testing framework to execute a full test even if it couldn't fully cover an action in the middle of the test.

## Understanding and Implementing Test Cases

Actions are the basic building blocks of integration tests. A test is a sequence of actions. Each action has a name that must be a valid C++ identifier.

Actions are defined (and can be modified) in [this](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1864725389) sheet. Tests are defined (and can be modified) in [this](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=2008870403) sheet. Partial coverage path additions are defined (and can be modified) in [this](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=452077264) sheet.

### Action Creation & Specification

[Actions](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1864725389) are the building blocks of tests.

#### Templates
To help making test writing less repetitive, actions are described as templates in the [actions](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1864725389) spreadsheet. Action templates specify actions while avoiding rote repetition. Each action template has a name (the **action base name**). Each action template supports at most one parameter, which takes values from a predefined list associated with the template. Parameter values must also be valid C++ identifiers.

An action template without parameters specifies one action whose name matches the template. For example, the `assert_window_closed` template generates the `assert_window_closed` action.

An action template with a parameter that can take N values specifies N actions, whose names are the concatenations of the template name and the corresponding value name, separated by an underscore (_). For example, the `clear_app_badge` template generates the `clear_app_badge_site_a` and `clear_app_badge_site_b` actions.

#### Default Values

All parametrized templates can mark one of their parameter values as the default value. This value is used to magically convert template names to action names.

#### Internal Actions
The testing framework is not going to be able to test every single manual action. To help allow partial coverage of these actions, internal actions are often specified, and then used in the partial coverage file, to allow the framework to execute the full test still. These are generally signified by postfixing `_internal` on the action base name.

#### Specifying a Parameter
Human-friendly action names are a slight variation of the canonical names above.

Actions generated by parameter-less templates have the same human-friendly name as their (canonical?) name.

Actions generated by parametrized templates use parenthesis to separate the template name from the value name. For example, the actions generated by the `clear_app_badge` template have the human-friendly names `clear_app_badge(site_a)` and `clear_app_badge(site_b)`.

The template name can be used as the human-friendly name of the action generated by the template with the default value. For example, `clear_app_badge` is a shorter human-friendly name equivalent to `clear_app_badge(site_a)`.

### Test Creation & Specification

[Tests]((https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=2008870403)) are created specifying actions, and they are currently organized by **affected-by** edges.

#### Affected-by edges

This framework is designed to consume an exhaustive list of **affected-by** action edges. These are a pair of two actions, where the first action affects the second. For example, the action `set_app_badge` will affect the action `assert_app_badge_has_value`. Or, `uninstall_from_app_list` will affect `assert_platform_shortcut_exists`.

Once these edges are identified, tests can be created around them.

#### Creating a test

Given an **affected-by** edge, a test should be created that does the bare minimum necessary to set up the test before the first affected-by action, and then any required other actions in between that and the second affect-by action.

The framework is designed to be able to collapse tests that contain common non-'state-check' actions, so adding a new test does not always mean that a whole new test will be run by the framework. Sometimes it only adds a few extra state-check actions in an existing test.

#### Unsupportable actions & partial coverage

Some actions might be unsupportable by an automated test framework. For example, `clear_app_badge` cannot work as a browsertest action, as it (currently)  a level of OS integration that cannot happen if many browsertests are all happening at the same time. Thus, to still partially cover test cases that use this action, a `clear_app_badge_internal` action exists which hooks into a fake interface to track badging actions for a webapp. The framework can use this instead, and the test creator then has to specify this in the partial coverage file.

The [partial coverage](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=452077264) sheet specifies when user actions that cannot be automated can be replaced by one or more internal actions. For example, the actions `list_apps, uninstall_from_app_list` can be partially covered by the internal action `uninstall_internal`. When those two actions are encountered during test generation, the script knows that, if needed, the action `unintall_internal` can be used if the framework does not support the first two actions.

## Script Usage

### Downloading test data

The test data is hosted in this [spreadsheet](https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/edit). To download the latest copy of the data, run the included script:
```sh
./chrome/test/web_apps/download_data_from_sheet.sh
```

This will download the data from the sheet into csv files in the [data/](data/) directory:

* `actions.csv` This lists all actions that can be used, and specifies if the action is an 'state_check' action (which means it doesn't effect state). The first row of this file is skipped.
* `automated_tests.csv` This lists the audited Web App automated tests & their approximate actions. First column and row are skipped.
* `manual_tests.csv` This lists the audited manual tests & their approximate actions. First column and row are skipped.
* `coverage_required.csv` This is the full list of all tests needed to fully cover the Web App system. The first two rows are skipped.
* `framework_actions_*.csv` These are the supported actions by the browsertest testing framework for the given platform. First row is skipped.
* `partial_coverage_paths.csv` These are the partial coverage paths that can be use to augment the action graph. These help us add and use internal actions to partial cover tests that have actions that are hard or impossible to support in the automated framework.

The only data that is NOT downloaded is the supported framework action files (e.g. [framework_actions_linux.csv](data/framework_actions_linux.csv)), which should be modified directly in the Chromium tree, in the same CL that implements (or removes) the supported action in the framework.

### Generating test descriptions & coverage

Test descriptions for each platform are generated by running:
```sh
chrome/test/web_apps/generate_framework_tests_and_coverage.py
```
This uses the files in `chrome/test/web_apps/data` to generate the following in `chrome/test/web_apps/output`:

* Per-platform coverage `tsv` files that contain a copy of the `coverage_required.tsv` file, but with markers per action to allow a conditional formatter (like the one [here](https://docs.google.com/spreadsheets/d/1d3iAOAnojp4_WrPky9exz1-mjkeulOJVUav5QYG99MQ/edit#gid=884228058)) to highlight what was and was not covered by the testing framework.
  * These files also contain a coverage % at the top of the file. Full coverage is the percent of the input actions that were covered by the tests. Partial coverage is the percent of input actions plus any 'partial paths' that allow partial coverage of an unsupported action
* Per-platform framework test definition (e.g. `framework_tests_cros.csv`) that can be used by the Web App integration test browsertest framework.
  * Actions involving the sync system require a framework specialization, so these tests are stored in a separate file (e.g. `framework_tests_sync_cros.csv`).

#### Audited coverage information (and how to exclude this information)

The coverage numbers include coverage from the audited [automated](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1894585254) and [manual](https://docs.google.com/spreadsheets/u/1/d/e/2PACX-1vSbO6VsnWsq_9MN6JEXlL8asMqATHc2-pz9ed_Jlf5zHJGg2KAtegsorHqkQ5kydU6VCqebv_1gUCD5/pubhtml?gid=1424278080) test data. This was generated by auditing all existing automated tests in the codebase and all manual tests owned by the QA team. All tests that were supported by this framework were translated and recorded.

To exclude this information in the coverage analysis, you can specify the `--ignore_audited` flag like so:
```sh
chrome/test/web_apps/generate_framework_tests_and_coverage.py --ignore_audited
```

### Exploring the tested and coverage graphs

To view the directed graphs that are generated to process the test and coverage data, the `--graphs` switch can be specified:
```sh
chrome/test/web_apps/generate_framework_tests_and_coverage.py --graphs
```

This will generate:
* `coverage_required_graph.dot` - The graph of all of the required test coverage. Green nodes are actions explicitly listed in the coverage list, and orange nodes specify partial coverage paths.
* `framework_test_graph_<platform>.dot` - The graph that is now tested by the generated framework tests for the given platform, including partial coverage.
* `coverage_graph_<platform>.dot` - The graph of the coverage provided by the generated framework tests and audited tests (see above about how to exclude audited tests).

The [graphviz](https://graphviz.org/) library can be used to view these graphs. An install-free online version is [here](https://dreampuf.github.io/GraphvizOnline/).

### Debugging further

To help debug or explore further, please see the [`graph_cli_tool.py`](graph_cli_tool.py) script, which contains the actual algorithms, and also includes a number of command line utilities to process the various files.

Both this file and the [`generate_framework_tests_and_coverage.py`](generate_framework_tests_and_coverage.py) file support the `-v` option to print out informational logging.
